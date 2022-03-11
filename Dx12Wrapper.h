#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXTex.h>
#include <wrl.h>
#include <memory>
#include <string>
#include <map>
#include <unordered_map>

using namespace Microsoft::WRL;

class Dx12Wrapper
{
private:
	// DXGI(グラフィックスインフラストラクチャ)まわり
	ComPtr<IDXGIFactory4> m_factory = nullptr; // DXGIインターフェイス
	ComPtr<IDXGISwapChain4> m_swapChain = nullptr; // スワップチェイン

	// DirectX12まわり
	ComPtr<ID3D12Device> m_device = nullptr; // デバイス
	ComPtr<ID3D12CommandAllocator> m_cmdAllocator = nullptr; // コマンドアロケータ
	ComPtr<ID3D12GraphicsCommandList> m_cmdList = nullptr; // コマンドリスト
	ComPtr<ID3D12CommandQueue> m_cmdQueue = nullptr; // コマンドキュー

	// 表示に関わるバッファ周り
	ComPtr<ID3D12Resource> m_depthBuffer = nullptr; // 深度バッファ
	std::vector<ID3D12Resource*> m_backBuffers; // バックバッファ(2つ以上…スワップチェインが確保)
	ComPtr<ID3D12DescriptorHeap> m_rtvDescHeap = nullptr; // レンダーターゲット用デスクリプタヒープ
	ComPtr<ID3D12DescriptorHeap> m_dsvDescHeap = nullptr; // 深度バッファビュー用デスクリプタヒープ
	std::unique_ptr<D3D12_VIEWPORT> m_viewPort; // ビューポート
	std::unique_ptr<D3D12_RECT> m_scissorRect; // シザー矩形

	// シーンを構成するバッファまわり
	ComPtr<ID3D12Resource> m_sceneConstBuff = nullptr;
	ComPtr<ID3D12DescriptorHeap> m_sceneDescHeap = nullptr;
	struct SceneData
	{
		DirectX::XMMATRIX view; // ビュー行列
		DirectX::XMMATRIX proj; // プロジェクション行列
		DirectX::XMFLOAT3 eye;  // 視点座標
	};
	SceneData* m_mappedSceneData = nullptr;

	// フェンス
	ComPtr<ID3D12Fence> m_fence = nullptr;
	UINT64 m_fenceVal = 0;

	// ロード用テーブル
	using LoadLambda_t = std::function<HRESULT(const std::wstring& path, DirectX::TexMetadata*, DirectX::ScratchImage&)>;
	std::map<std::string, LoadLambda_t> m_loadLambdaTable;

	// テクスチャテーブル
	std::unordered_map<std::string, ComPtr<ID3D12Resource>> m_textureTable;

	// デバッグレイヤーを有効にする
	HRESULT EnableDebugLayer();
	// DXGIFactoryの初期化
	HRESULT InitializeIDXGIFactory();
	// デバイスの初期化
	HRESULT InitializeID3D12Device();
	// コマンドアロケーター、リスト、キューの初期化
	HRESULT InitializeCommand();

	// 最終的なレンダーターゲットの生成
	HRESULT	CreateFinalRenderTargets();

	// デプスステンシルビューの生成
	HRESULT CreateDepthStencilView();

	// スワップチェインの生成
	HRESULT CreateSwapChain(const HWND& hwnd);

	// ビュープロジェクション用ビューの生成
	HRESULT CreateSceneView();

	// フェンスの作成
	HRESULT CreateFence();

	// テクスチャ名からテクスチャバッファ作成、中身をコピー
	ID3D12Resource* CreateTextureFromFile(const char* texpath);

	// テクスチャローダテーブル
	void CreateTextureLoaderTable();

public:
	Dx12Wrapper(HWND hwnd); // コンストラクタ
	~Dx12Wrapper() {} // デストラクタ

	void Update() {} // ループ処理
	void BeginDraw(); // 全体の描画準備
	void EndDraw(); // 描画終わり
	void SetScene(); // 現在のシーン(ビュープロジェクション)をセット

	/// テクスチャパスから必要なテクスチャバッファへのポインタを返す
	/// @param texpath テクスチャファイルパス
	ComPtr<ID3D12Resource> GetTextureByPath(const char* texpath);

	ComPtr<ID3D12Device> Device() { return m_device; } // デバイス
	ComPtr<ID3D12GraphicsCommandList> CommandList() { return m_cmdList; } // コマンドリスト
	ComPtr<IDXGISwapChain4> Swapchain() { return m_swapChain; } // スワップチェイン
};