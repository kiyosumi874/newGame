#include "Dx12Wrapper.h"
#include <cassert>
#include <d3dx12.h>
#include <d3dcompiler.h>
#include "Application.h"

#pragma comment(lib,"DirectXTex.lib")
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")

using namespace std;
using namespace DirectX;

//------------------------------------
// 無名名前空間
//------------------------------------
namespace
{
	/// ファイル名から拡張子を取得する
	/// @param path 対象のパス文字列
	/// @return 拡張子
	string GetExtension(const std::string& path)
	{
		int idx = path.rfind('.');
		return path.substr(idx + 1, path.length() - idx - 1);
	}

	/// ファイル名から拡張子を取得する(ワイド文字版)
	/// @param path 対象のパス文字列
	/// @return 拡張子
	wstring GetExtension(const std::wstring& path)
	{
		int idx = path.rfind(L'.');
		return path.substr(idx + 1, path.length() - idx - 1);
	}

	/// string(マルチバイト文字列)からwstring(ワイド文字列)を得る
		/// @param str マルチバイト文字列
		/// @return 変換されたワイド文字列
	std::wstring GetWideStringFromString(const std::string& str)
	{
		// 呼び出し1回目(文字列数を得る)
		auto num1 = MultiByteToWideChar(
			CP_ACP,
			MB_PRECOMPOSED | MB_ERR_INVALID_CHARS,
			str.c_str(), -1, nullptr, 0);

		std::wstring wstr; // stringのwchar_t版
		wstr.resize(num1); // 得られた文字列数でリサイズ

		// 呼び出し2回目(確保済みのwstrに変換文字列をコピー)
		auto num2 = MultiByteToWideChar(
			CP_ACP,
			MB_PRECOMPOSED | MB_ERR_INVALID_CHARS,
			str.c_str(), -1, &wstr[0], num1);

		assert(num1 == num2); // 一応チェック
		return wstr;
	}
}
//--------------------------------------------
// publicClass
//--------------------------------------------

Dx12Wrapper::Dx12Wrapper(HWND hwnd)
{
#ifdef _DEBUG
	// デバッグレイヤーをオンに
	if (FAILED(EnableDebugLayer()))
	{
		assert(NULL);
		return;
	}
#endif

	typedef HRESULT(Dx12Wrapper::* FUNC)();
	std::vector<FUNC> funcVec;

	funcVec.push_back(&Dx12Wrapper::InitializeIDXGIFactory);
	funcVec.push_back(&Dx12Wrapper::InitializeID3D12Device);
	funcVec.push_back(&Dx12Wrapper::InitializeCommand);

	for (auto func : funcVec)
	{
		if (FAILED((this->*func)()))
		{
			assert(NULL);
			return;
		}
	}
	funcVec.clear();

	if (FAILED(CreateSwapChain(hwnd)))
	{
		assert(NULL);
		return;
	}

	// テクスチャローダー関連初期化
	CreateTextureLoaderTable();

	funcVec.push_back(&Dx12Wrapper::CreateFinalRenderTargets);
	funcVec.push_back(&Dx12Wrapper::CreateSceneView);
	funcVec.push_back(&Dx12Wrapper::CreateDepthStencilView);
	funcVec.push_back(&Dx12Wrapper::CreateFence);

	for (auto func : funcVec)
	{
		if (FAILED((this->*func)()))
		{
			assert(NULL);
			return;
		}
	}

}

void Dx12Wrapper::BeginDraw()
{
	// バックバッファのインデックスを取得
	auto bbIdx = m_swapChain->GetCurrentBackBufferIndex();

	// バリア(リソースの状態遷移をGPUに教えてあげるもの)
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_backBuffers[bbIdx],
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	m_cmdList->ResourceBarrier(1, &barrier);

	// レンダーターゲットを指定
	auto rtvH = m_rtvDescHeap->GetCPUDescriptorHandleForHeapStart();
	rtvH.ptr += bbIdx * m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	
	// 深度を指定
	auto dsvH = m_dsvDescHeap->GetCPUDescriptorHandleForHeapStart();

	m_cmdList->OMSetRenderTargets(1, &rtvH, true, &dsvH);
	m_cmdList->ClearDepthStencilView(dsvH, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	// 画面クリア
	float clearColor[] = { 0.0f,1.0f,0.0f,1.0f };
	m_cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);

	// ビューポート、シザー矩形のセット
	m_cmdList->RSSetViewports(1, m_viewPort.get());
	m_cmdList->RSSetScissorRects(1, m_scissorRect.get());
}

void Dx12Wrapper::EndDraw()
{
	// バックバッファのインデックスを取得
	auto bbIdx = m_swapChain->GetCurrentBackBufferIndex();

	// バリア(リソースの状態遷移をGPUに教えてあげるもの)
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_backBuffers[bbIdx],
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	m_cmdList->ResourceBarrier(1, &barrier);

	// 命令のクローズ
	m_cmdList->Close();

	// コマンドリストの実行
	ID3D12CommandList* cmdlists[] = { m_cmdList.Get() };
	m_cmdQueue->ExecuteCommandLists(1, cmdlists);

	// 待ち
	m_cmdQueue->Signal(m_fence.Get(), ++m_fenceVal);

	// GetCompletedValueはGPU処理が終わったときに更新される、GPU側のフェンス値を返す
	if (m_fence->GetCompletedValue() != m_fenceVal)
	{
		// イベントハンドルの取得
		auto event = CreateEvent(nullptr, false, false, nullptr);
		m_fence->SetEventOnCompletion(m_fenceVal, event);
		// イベントが発生するまで待ち続ける
		WaitForSingleObject(event, INFINITE);
		// イベントハンドルを閉じる
		CloseHandle(event);
	}

	m_cmdAllocator->Reset(); // キューをクリア
	m_cmdList->Reset(m_cmdAllocator.Get(), nullptr); // 再びコマンドリストをためる準備
}

void Dx12Wrapper::SetScene()
{
	// 現在のシーン(ビュープロジェクション)をセット
	ID3D12DescriptorHeap* sceneheaps[] = { m_sceneDescHeap.Get() };
	m_cmdList->SetDescriptorHeaps(1, sceneheaps);
	m_cmdList->SetGraphicsRootDescriptorTable(0, m_sceneDescHeap->GetGPUDescriptorHandleForHeapStart());
}

ComPtr<ID3D12Resource> Dx12Wrapper::GetTextureByPath(const char* texpath)
{
	auto it = m_textureTable.find(texpath);
	if (it != m_textureTable.end())
	{
		// テーブルに内にあったらロードするのではなくマップ内の
		// リソースを返す
		return m_textureTable[texpath];
	}
	else
	{
		return ComPtr<ID3D12Resource>(CreateTextureFromFile(texpath));
	}
}

//--------------------------------------------
// privateClass
//--------------------------------------------

HRESULT Dx12Wrapper::EnableDebugLayer()
{
	ComPtr<ID3D12Debug> debugLayer = nullptr;
	auto result = D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer));
	debugLayer->EnableDebugLayer();
	return result;
}

HRESULT Dx12Wrapper::InitializeIDXGIFactory()
{
	UINT flagsDXGI = 0;
	flagsDXGI |= DXGI_CREATE_FACTORY_DEBUG;
	auto result = CreateDXGIFactory2(flagsDXGI, IID_PPV_ARGS(m_factory.ReleaseAndGetAddressOf()));

	return result;
}

HRESULT Dx12Wrapper::InitializeID3D12Device()
{
	// フィーチャレベル列挙
	D3D_FEATURE_LEVEL levels[] =
	{
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};

	std::vector<IDXGIAdapter*> adapters;
	IDXGIAdapter* tmpAdapter = nullptr;
	for (int i = 0; m_factory->EnumAdapters(i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND; ++i)
	{
		adapters.push_back(tmpAdapter);
	}

	for (auto adpt : adapters)
	{
		DXGI_ADAPTER_DESC adesc = {};
		adpt->GetDesc(&adesc);
		std::wstring strDesc = adesc.Description;
		if (strDesc.find(L"NVIDIA") != std::string::npos)
		{
			tmpAdapter = adpt;
			break;
		}
	}

	auto result = S_FALSE;
	for (auto lv : levels)
	{
		if (SUCCEEDED(D3D12CreateDevice(tmpAdapter, lv, IID_PPV_ARGS(m_device.ReleaseAndGetAddressOf()))))
		{
			result = S_OK;
			break;
		}
	}
	return result;
}

// コマンドアロケーター、リスト、キューの初期化
HRESULT Dx12Wrapper::InitializeCommand()
{
	auto result = m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(m_cmdAllocator.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(NULL);
		return result;
	}

	result = m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_cmdAllocator.Get(), nullptr, IID_PPV_ARGS(m_cmdList.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(NULL);
		return result;
	}

	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
	cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE; // タイムアウトなし
	cmdQueueDesc.NodeMask = 0;
	cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL; // プライオリティ特に指定なし
	cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT; // ここはコマンドリストと合わせてください
	result = m_device->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(m_cmdQueue.ReleaseAndGetAddressOf())); // コマンドキュー生成
	assert(SUCCEEDED(result));
	return result;
}

HRESULT Dx12Wrapper::CreateFinalRenderTargets()
{
	DXGI_SWAP_CHAIN_DESC1 desc = {};
	auto result = m_swapChain->GetDesc1(&desc);

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; // レンダーターゲットビューなので当然RTV
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 2; // 表裏の２つ
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // 特に指定なし

	result = m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(m_rtvDescHeap.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		return result;
	}

	DXGI_SWAP_CHAIN_DESC swcDesc = {};
	result = m_swapChain->GetDesc(&swcDesc);
	m_backBuffers.resize(swcDesc.BufferCount);

	D3D12_CPU_DESCRIPTOR_HANDLE handle = m_rtvDescHeap->GetCPUDescriptorHandleForHeapStart();

	// SRGBレンダーターゲットビュー設定
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	for (int i = 0; i < swcDesc.BufferCount; ++i)
	{
		result = m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i]));
		assert(SUCCEEDED(result));
		rtvDesc.Format = m_backBuffers[i]->GetDesc().Format;
		m_device->CreateRenderTargetView(m_backBuffers[i], &rtvDesc, handle);
		handle.ptr += m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	m_viewPort.reset(new CD3DX12_VIEWPORT(m_backBuffers[0]));
	m_scissorRect.reset(new CD3DX12_RECT(0, 0, desc.Width, desc.Height));
	return result;
}

HRESULT Dx12Wrapper::CreateDepthStencilView()
{
	DXGI_SWAP_CHAIN_DESC1 desc = {};
	auto result = m_swapChain->GetDesc1(&desc);

	// 深度バッファ作成
	D3D12_RESOURCE_DESC depthResDesc = {};
	depthResDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; // 2次元のテクスチャデータ
	depthResDesc.DepthOrArraySize = 1; // テクスチャ配列でも、3D配列でもない
	depthResDesc.Width = desc.Width;
	depthResDesc.Height = desc.Height;
	depthResDesc.Format = DXGI_FORMAT_D32_FLOAT;
	depthResDesc.SampleDesc.Count = 1;
	depthResDesc.SampleDesc.Quality = 0;
	depthResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	depthResDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthResDesc.MipLevels = 1;
	depthResDesc.Alignment = 0;

	// デプス用ヒーププロパティ
	auto depthHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	CD3DX12_CLEAR_VALUE depthClearValue(DXGI_FORMAT_D32_FLOAT, 1.0f, 0);

	result = m_device->CreateCommittedResource(&depthHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&depthResDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE, //デプス書き込みに使用
		&depthClearValue,
		IID_PPV_ARGS(m_depthBuffer.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		return result;
	}

	// 深度のためのデスクリプタヒープ作成
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {}; // 深度に使うよという事がわかればいい
	dsvHeapDesc.NumDescriptors = 1; // 深度ビュー1つのみ
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV; // デプスステンシルビューとして使う
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	result = m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(m_dsvDescHeap.ReleaseAndGetAddressOf()));

	// 深度ビュー作成
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT; // デプス値に32bit使用
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE; // フラグは特になし
	m_device->CreateDepthStencilView(m_depthBuffer.Get(), &dsvDesc, m_dsvDescHeap->GetCPUDescriptorHandleForHeapStart());
}

// スワップチェイン生成関数
HRESULT Dx12Wrapper::CreateSwapChain(const HWND& hwnd)
{
	RECT rc = {};
	GetWindowRect(hwnd, &rc);
	const auto& app = Application::Instance();
	auto winSize = app.GetWindowSize();
	DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};
	swapchainDesc.Width = winSize.cx;
	swapchainDesc.Height = winSize.cy;
	swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapchainDesc.Stereo = false;
	swapchainDesc.SampleDesc.Count = 1;
	swapchainDesc.SampleDesc.Quality = 0;
	swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapchainDesc.BufferCount = 2;
	swapchainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapchainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	auto result = m_factory->CreateSwapChainForHwnd(m_cmdQueue.Get(),
		hwnd,
		&swapchainDesc,
		nullptr,
		nullptr,
		(IDXGISwapChain1**)m_swapChain.ReleaseAndGetAddressOf());
	assert(SUCCEEDED(result));
	return result;
}

HRESULT Dx12Wrapper::CreateSceneView()
{
	DXGI_SWAP_CHAIN_DESC1 desc = {};
	auto result = m_swapChain->GetDesc1(&desc);
	auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto resDesc = CD3DX12_RESOURCE_DESC::Buffer((sizeof(SceneData) + 0xff) & ~0xff);

	// 定数バッファ作成
	result = m_device->CreateCommittedResource(&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(m_sceneConstBuff.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	m_mappedSceneData = nullptr; // マップ先を示すポインタ
	result = m_sceneConstBuff->Map(0, nullptr, (void**)&m_mappedSceneData); // マップ

	XMFLOAT3 eye(0, 15, -15);
	XMFLOAT3 target(0, 15, 0);
	XMFLOAT3 up(0, 1, 0);
	m_mappedSceneData->view = XMMatrixLookAtLH(XMLoadFloat3(&eye), XMLoadFloat3(&target), XMLoadFloat3(&up));
	m_mappedSceneData->proj = XMMatrixPerspectiveFovLH(XM_PIDIV4,         // 画角は45°
		static_cast<float>(desc.Width) / static_cast<float>(desc.Height), // アス比
		0.1f,     // 近い方
		1000.0f); // 遠い方

	m_mappedSceneData->eye = eye;

	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE; // シェーダから見えるように
	descHeapDesc.NodeMask = 0; // マスクは0
	descHeapDesc.NumDescriptors = 1;
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; // デスクリプタヒープ種別
	result = m_device->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(m_sceneDescHeap.ReleaseAndGetAddressOf())); // 生成

	//// デスクリプタの先頭ハンドルを取得しておく
	auto heapHandle = m_sceneDescHeap->GetCPUDescriptorHandleForHeapStart();

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = m_sceneConstBuff->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = m_sceneConstBuff->GetDesc().Width;

	// 定数バッファビューの作成
	m_device->CreateConstantBufferView(&cbvDesc, heapHandle);
	return result;
}

HRESULT Dx12Wrapper::CreateFence()
{
	return m_device->CreateFence(m_fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(m_fence.ReleaseAndGetAddressOf()));
}

ID3D12Resource* Dx12Wrapper::CreateTextureFromFile(const char* texpath)
{
	string texPath = texpath;
	// テクスチャのロード
	TexMetadata metaData = {};
	ScratchImage scratchImg = {};
	auto wTexPath = GetWideStringFromString(texPath); // テクスチャのファイルパス
	auto ext = GetExtension(texPath); // 拡張子を取得
	auto result = m_loadLambdaTable[ext](wTexPath,
		&metaData,
		scratchImg);
	if (FAILED(result))
	{
		return nullptr;
	}

	auto img = scratchImg.GetImage(0, 0, 0); // 生データ抽出

	// WriteToSubresourceで転送する用のヒープ設定
	auto texHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY_WRITE_BACK, D3D12_MEMORY_POOL_L0);
	auto resDesc = CD3DX12_RESOURCE_DESC::Tex2D(metaData.format, metaData.width, metaData.height, metaData.arraySize, metaData.mipLevels);

	ID3D12Resource* texBuff = nullptr;
	result = m_device->CreateCommittedResource(&texHeapProp,
		D3D12_HEAP_FLAG_NONE, // 特に指定なし
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&texBuff));
	if (FAILED(result))
	{
		return nullptr;
	}

	result = texBuff->WriteToSubresource(0,
		nullptr, // 全領域へコピー
		img->pixels, // 元データアドレス
		img->rowPitch, // 1ラインサイズ
		img->slicePitch); // 全サイズ
	if (FAILED(result))
	{
		return nullptr;
	}

	return texBuff;
}

void Dx12Wrapper::CreateTextureLoaderTable()
{
	m_loadLambdaTable["sph"] = m_loadLambdaTable["spa"] = m_loadLambdaTable["bmp"] = m_loadLambdaTable["png"] = m_loadLambdaTable["jpg"] = [](const wstring& path, TexMetadata* meta, ScratchImage& img)->HRESULT
	{
		return LoadFromWICFile(path.c_str(), WIC_FLAGS_NONE, meta, img);
	};

	m_loadLambdaTable["tga"] = [](const wstring& path, TexMetadata* meta, ScratchImage& img)->HRESULT
	{
		return LoadFromTGAFile(path.c_str(), meta, img);
	};

	m_loadLambdaTable["dds"] = [](const wstring& path, TexMetadata* meta, ScratchImage& img)->HRESULT
	{
		return LoadFromDDSFile(path.c_str(), DDS_FLAGS_NONE, meta, img);
	};
}

