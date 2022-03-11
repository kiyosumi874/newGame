#pragma once
#include <d3d12.h>
#include <vector>
#include <wrl.h>
#include <memory>

using namespace Microsoft::WRL;

class Dx12Wrapper;
class PMDRenderer
{
private:
	std::shared_ptr<Dx12Wrapper> m_dx12Wrapper = nullptr;

	ComPtr<ID3D12PipelineState> m_pipeline = nullptr; // PMD用パイプライン
	ComPtr<ID3D12RootSignature> m_rootSignature = nullptr; // PMD用ルートシグネチャ

	// PMD用共通テクスチャ(白、黒、グレイスケールグラデーション)
	ComPtr<ID3D12Resource> m_whiteTex = nullptr;
	ComPtr<ID3D12Resource> m_blackTex = nullptr;
	ComPtr<ID3D12Resource> m_gradTex = nullptr;

	ID3D12Resource* CreateDefaultTexture(size_t width, size_t height);
	ID3D12Resource* CreateWhiteTexture();//白テクスチャの生成
	ID3D12Resource* CreateBlackTexture();//黒テクスチャの生成
	ID3D12Resource* CreateGrayGradationTexture();//グレーテクスチャの生成

	//パイプライン初期化
	HRESULT CreateGraphicsPipelineForPMD();
	//ルートシグネチャ初期化
	HRESULT CreateRootSignatureForPMD();

	bool CheckShaderCompileResult(HRESULT result, ID3DBlob* error = nullptr);


public:
	PMDRenderer(Dx12Wrapper& dx12);
	~PMDRenderer();
	void Update();
	void Draw();
	// ゲッター
	ID3D12PipelineState* GetPipelineState()
	{
		return m_pipeline.Get();
	}
	ID3D12RootSignature* GetRootSignature()
	{
		return m_rootSignature.Get();
	}

	ComPtr<ID3D12Resource> GetWhiteTex()
	{
		return m_whiteTex;
	}

	ComPtr<ID3D12Resource> GetBlackTex()
	{
		return m_blackTex;
	}

	ComPtr<ID3D12Resource> GetGradTex()
	{
		return m_gradTex;
	}
};