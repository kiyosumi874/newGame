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

	ComPtr<ID3D12PipelineState> m_pipeline = nullptr; // PMD�p�p�C�v���C��
	ComPtr<ID3D12RootSignature> m_rootSignature = nullptr; // PMD�p���[�g�V�O�l�`��

	// PMD�p���ʃe�N�X�`��(���A���A�O���C�X�P�[���O���f�[�V����)
	ComPtr<ID3D12Resource> m_whiteTex = nullptr;
	ComPtr<ID3D12Resource> m_blackTex = nullptr;
	ComPtr<ID3D12Resource> m_gradTex = nullptr;

	ID3D12Resource* CreateDefaultTexture(size_t width, size_t height);
	ID3D12Resource* CreateWhiteTexture();//���e�N�X�`���̐���
	ID3D12Resource* CreateBlackTexture();//���e�N�X�`���̐���
	ID3D12Resource* CreateGrayGradationTexture();//�O���[�e�N�X�`���̐���

	//�p�C�v���C��������
	HRESULT CreateGraphicsPipelineForPMD();
	//���[�g�V�O�l�`��������
	HRESULT CreateRootSignatureForPMD();

	bool CheckShaderCompileResult(HRESULT result, ID3DBlob* error = nullptr);


public:
	PMDRenderer(Dx12Wrapper& dx12);
	~PMDRenderer();
	void Update();
	void Draw();
	// �Q�b�^�[
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