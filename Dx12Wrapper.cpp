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
// �������O���
//------------------------------------
namespace
{
	/// �t�@�C��������g���q���擾����
	/// @param path �Ώۂ̃p�X������
	/// @return �g���q
	string GetExtension(const std::string& path)
	{
		int idx = path.rfind('.');
		return path.substr(idx + 1, path.length() - idx - 1);
	}

	/// �t�@�C��������g���q���擾����(���C�h������)
	/// @param path �Ώۂ̃p�X������
	/// @return �g���q
	wstring GetExtension(const std::wstring& path)
	{
		int idx = path.rfind(L'.');
		return path.substr(idx + 1, path.length() - idx - 1);
	}

	/// string(�}���`�o�C�g������)����wstring(���C�h������)�𓾂�
		/// @param str �}���`�o�C�g������
		/// @return �ϊ����ꂽ���C�h������
	std::wstring GetWideStringFromString(const std::string& str)
	{
		// �Ăяo��1���(�����񐔂𓾂�)
		auto num1 = MultiByteToWideChar(
			CP_ACP,
			MB_PRECOMPOSED | MB_ERR_INVALID_CHARS,
			str.c_str(), -1, nullptr, 0);

		std::wstring wstr; // string��wchar_t��
		wstr.resize(num1); // ����ꂽ�����񐔂Ń��T�C�Y

		// �Ăяo��2���(�m�ۍς݂�wstr�ɕϊ���������R�s�[)
		auto num2 = MultiByteToWideChar(
			CP_ACP,
			MB_PRECOMPOSED | MB_ERR_INVALID_CHARS,
			str.c_str(), -1, &wstr[0], num1);

		assert(num1 == num2); // �ꉞ�`�F�b�N
		return wstr;
	}
}
//--------------------------------------------
// publicClass
//--------------------------------------------

Dx12Wrapper::Dx12Wrapper(HWND hwnd)
{
#ifdef _DEBUG
	// �f�o�b�O���C���[���I����
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

	// �e�N�X�`�����[�_�[�֘A������
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
	// �o�b�N�o�b�t�@�̃C���f�b�N�X���擾
	auto bbIdx = m_swapChain->GetCurrentBackBufferIndex();

	// �o���A(���\�[�X�̏�ԑJ�ڂ�GPU�ɋ����Ă��������)
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_backBuffers[bbIdx],
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	m_cmdList->ResourceBarrier(1, &barrier);

	// �����_�[�^�[�Q�b�g���w��
	auto rtvH = m_rtvDescHeap->GetCPUDescriptorHandleForHeapStart();
	rtvH.ptr += bbIdx * m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	
	// �[�x���w��
	auto dsvH = m_dsvDescHeap->GetCPUDescriptorHandleForHeapStart();

	m_cmdList->OMSetRenderTargets(1, &rtvH, true, &dsvH);
	m_cmdList->ClearDepthStencilView(dsvH, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	// ��ʃN���A
	float clearColor[] = { 0.0f,1.0f,0.0f,1.0f };
	m_cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);

	// �r���[�|�[�g�A�V�U�[��`�̃Z�b�g
	m_cmdList->RSSetViewports(1, m_viewPort.get());
	m_cmdList->RSSetScissorRects(1, m_scissorRect.get());
}

void Dx12Wrapper::EndDraw()
{
	// �o�b�N�o�b�t�@�̃C���f�b�N�X���擾
	auto bbIdx = m_swapChain->GetCurrentBackBufferIndex();

	// �o���A(���\�[�X�̏�ԑJ�ڂ�GPU�ɋ����Ă��������)
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_backBuffers[bbIdx],
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	m_cmdList->ResourceBarrier(1, &barrier);

	// ���߂̃N���[�Y
	m_cmdList->Close();

	// �R�}���h���X�g�̎��s
	ID3D12CommandList* cmdlists[] = { m_cmdList.Get() };
	m_cmdQueue->ExecuteCommandLists(1, cmdlists);

	// �҂�
	m_cmdQueue->Signal(m_fence.Get(), ++m_fenceVal);

	// GetCompletedValue��GPU�������I������Ƃ��ɍX�V�����AGPU���̃t�F���X�l��Ԃ�
	if (m_fence->GetCompletedValue() != m_fenceVal)
	{
		// �C�x���g�n���h���̎擾
		auto event = CreateEvent(nullptr, false, false, nullptr);
		m_fence->SetEventOnCompletion(m_fenceVal, event);
		// �C�x���g����������܂ő҂�������
		WaitForSingleObject(event, INFINITE);
		// �C�x���g�n���h�������
		CloseHandle(event);
	}

	m_cmdAllocator->Reset(); // �L���[���N���A
	m_cmdList->Reset(m_cmdAllocator.Get(), nullptr); // �ĂуR�}���h���X�g�����߂鏀��
}

void Dx12Wrapper::SetScene()
{
	// ���݂̃V�[��(�r���[�v���W�F�N�V����)���Z�b�g
	ID3D12DescriptorHeap* sceneheaps[] = { m_sceneDescHeap.Get() };
	m_cmdList->SetDescriptorHeaps(1, sceneheaps);
	m_cmdList->SetGraphicsRootDescriptorTable(0, m_sceneDescHeap->GetGPUDescriptorHandleForHeapStart());
}

ComPtr<ID3D12Resource> Dx12Wrapper::GetTextureByPath(const char* texpath)
{
	auto it = m_textureTable.find(texpath);
	if (it != m_textureTable.end())
	{
		// �e�[�u���ɓ��ɂ������烍�[�h����̂ł͂Ȃ��}�b�v����
		// ���\�[�X��Ԃ�
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
	// �t�B�[�`�����x����
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

// �R�}���h�A���P�[�^�[�A���X�g�A�L���[�̏�����
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
	cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE; // �^�C���A�E�g�Ȃ�
	cmdQueueDesc.NodeMask = 0;
	cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL; // �v���C�I���e�B���Ɏw��Ȃ�
	cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT; // �����̓R�}���h���X�g�ƍ��킹�Ă�������
	result = m_device->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(m_cmdQueue.ReleaseAndGetAddressOf())); // �R�}���h�L���[����
	assert(SUCCEEDED(result));
	return result;
}

HRESULT Dx12Wrapper::CreateFinalRenderTargets()
{
	DXGI_SWAP_CHAIN_DESC1 desc = {};
	auto result = m_swapChain->GetDesc1(&desc);

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; // �����_�[�^�[�Q�b�g�r���[�Ȃ̂œ��RRTV
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 2; // �\���̂Q��
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // ���Ɏw��Ȃ�

	result = m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(m_rtvDescHeap.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		return result;
	}

	DXGI_SWAP_CHAIN_DESC swcDesc = {};
	result = m_swapChain->GetDesc(&swcDesc);
	m_backBuffers.resize(swcDesc.BufferCount);

	D3D12_CPU_DESCRIPTOR_HANDLE handle = m_rtvDescHeap->GetCPUDescriptorHandleForHeapStart();

	// SRGB�����_�[�^�[�Q�b�g�r���[�ݒ�
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

	// �[�x�o�b�t�@�쐬
	D3D12_RESOURCE_DESC depthResDesc = {};
	depthResDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; // 2�����̃e�N�X�`���f�[�^
	depthResDesc.DepthOrArraySize = 1; // �e�N�X�`���z��ł��A3D�z��ł��Ȃ�
	depthResDesc.Width = desc.Width;
	depthResDesc.Height = desc.Height;
	depthResDesc.Format = DXGI_FORMAT_D32_FLOAT;
	depthResDesc.SampleDesc.Count = 1;
	depthResDesc.SampleDesc.Quality = 0;
	depthResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	depthResDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthResDesc.MipLevels = 1;
	depthResDesc.Alignment = 0;

	// �f�v�X�p�q�[�v�v���p�e�B
	auto depthHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	CD3DX12_CLEAR_VALUE depthClearValue(DXGI_FORMAT_D32_FLOAT, 1.0f, 0);

	result = m_device->CreateCommittedResource(&depthHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&depthResDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE, //�f�v�X�������݂Ɏg�p
		&depthClearValue,
		IID_PPV_ARGS(m_depthBuffer.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		return result;
	}

	// �[�x�̂��߂̃f�X�N���v�^�q�[�v�쐬
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {}; // �[�x�Ɏg����Ƃ��������킩��΂���
	dsvHeapDesc.NumDescriptors = 1; // �[�x�r���[1�̂�
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV; // �f�v�X�X�e���V���r���[�Ƃ��Ďg��
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	result = m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(m_dsvDescHeap.ReleaseAndGetAddressOf()));

	// �[�x�r���[�쐬
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT; // �f�v�X�l��32bit�g�p
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D; // 2D�e�N�X�`��
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE; // �t���O�͓��ɂȂ�
	m_device->CreateDepthStencilView(m_depthBuffer.Get(), &dsvDesc, m_dsvDescHeap->GetCPUDescriptorHandleForHeapStart());
}

// �X���b�v�`�F�C�������֐�
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

	// �萔�o�b�t�@�쐬
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

	m_mappedSceneData = nullptr; // �}�b�v��������|�C���^
	result = m_sceneConstBuff->Map(0, nullptr, (void**)&m_mappedSceneData); // �}�b�v

	XMFLOAT3 eye(0, 15, -15);
	XMFLOAT3 target(0, 15, 0);
	XMFLOAT3 up(0, 1, 0);
	m_mappedSceneData->view = XMMatrixLookAtLH(XMLoadFloat3(&eye), XMLoadFloat3(&target), XMLoadFloat3(&up));
	m_mappedSceneData->proj = XMMatrixPerspectiveFovLH(XM_PIDIV4,         // ��p��45��
		static_cast<float>(desc.Width) / static_cast<float>(desc.Height), // �A�X��
		0.1f,     // �߂���
		1000.0f); // ������

	m_mappedSceneData->eye = eye;

	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE; // �V�F�[�_���猩����悤��
	descHeapDesc.NodeMask = 0; // �}�X�N��0
	descHeapDesc.NumDescriptors = 1;
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; // �f�X�N���v�^�q�[�v���
	result = m_device->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(m_sceneDescHeap.ReleaseAndGetAddressOf())); // ����

	//// �f�X�N���v�^�̐擪�n���h�����擾���Ă���
	auto heapHandle = m_sceneDescHeap->GetCPUDescriptorHandleForHeapStart();

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = m_sceneConstBuff->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = m_sceneConstBuff->GetDesc().Width;

	// �萔�o�b�t�@�r���[�̍쐬
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
	// �e�N�X�`���̃��[�h
	TexMetadata metaData = {};
	ScratchImage scratchImg = {};
	auto wTexPath = GetWideStringFromString(texPath); // �e�N�X�`���̃t�@�C���p�X
	auto ext = GetExtension(texPath); // �g���q���擾
	auto result = m_loadLambdaTable[ext](wTexPath,
		&metaData,
		scratchImg);
	if (FAILED(result))
	{
		return nullptr;
	}

	auto img = scratchImg.GetImage(0, 0, 0); // ���f�[�^���o

	// WriteToSubresource�œ]������p�̃q�[�v�ݒ�
	auto texHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY_WRITE_BACK, D3D12_MEMORY_POOL_L0);
	auto resDesc = CD3DX12_RESOURCE_DESC::Tex2D(metaData.format, metaData.width, metaData.height, metaData.arraySize, metaData.mipLevels);

	ID3D12Resource* texBuff = nullptr;
	result = m_device->CreateCommittedResource(&texHeapProp,
		D3D12_HEAP_FLAG_NONE, // ���Ɏw��Ȃ�
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&texBuff));
	if (FAILED(result))
	{
		return nullptr;
	}

	result = texBuff->WriteToSubresource(0,
		nullptr, // �S�̈�փR�s�[
		img->pixels, // ���f�[�^�A�h���X
		img->rowPitch, // 1���C���T�C�Y
		img->slicePitch); // �S�T�C�Y
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

