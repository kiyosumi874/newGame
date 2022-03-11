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
	// DXGI(�O���t�B�b�N�X�C���t���X�g���N�`��)�܂��
	ComPtr<IDXGIFactory4> m_factory = nullptr; // DXGI�C���^�[�t�F�C�X
	ComPtr<IDXGISwapChain4> m_swapChain = nullptr; // �X���b�v�`�F�C��

	// DirectX12�܂��
	ComPtr<ID3D12Device> m_device = nullptr; // �f�o�C�X
	ComPtr<ID3D12CommandAllocator> m_cmdAllocator = nullptr; // �R�}���h�A���P�[�^
	ComPtr<ID3D12GraphicsCommandList> m_cmdList = nullptr; // �R�}���h���X�g
	ComPtr<ID3D12CommandQueue> m_cmdQueue = nullptr; // �R�}���h�L���[

	// �\���Ɋւ��o�b�t�@����
	ComPtr<ID3D12Resource> m_depthBuffer = nullptr; // �[�x�o�b�t�@
	std::vector<ID3D12Resource*> m_backBuffers; // �o�b�N�o�b�t�@(2�ȏ�c�X���b�v�`�F�C�����m��)
	ComPtr<ID3D12DescriptorHeap> m_rtvDescHeap = nullptr; // �����_�[�^�[�Q�b�g�p�f�X�N���v�^�q�[�v
	ComPtr<ID3D12DescriptorHeap> m_dsvDescHeap = nullptr; // �[�x�o�b�t�@�r���[�p�f�X�N���v�^�q�[�v
	std::unique_ptr<D3D12_VIEWPORT> m_viewPort; // �r���[�|�[�g
	std::unique_ptr<D3D12_RECT> m_scissorRect; // �V�U�[��`

	// �V�[�����\������o�b�t�@�܂��
	ComPtr<ID3D12Resource> m_sceneConstBuff = nullptr;
	ComPtr<ID3D12DescriptorHeap> m_sceneDescHeap = nullptr;
	struct SceneData
	{
		DirectX::XMMATRIX view; // �r���[�s��
		DirectX::XMMATRIX proj; // �v���W�F�N�V�����s��
		DirectX::XMFLOAT3 eye;  // ���_���W
	};
	SceneData* m_mappedSceneData = nullptr;

	// �t�F���X
	ComPtr<ID3D12Fence> m_fence = nullptr;
	UINT64 m_fenceVal = 0;

	// ���[�h�p�e�[�u��
	using LoadLambda_t = std::function<HRESULT(const std::wstring& path, DirectX::TexMetadata*, DirectX::ScratchImage&)>;
	std::map<std::string, LoadLambda_t> m_loadLambdaTable;

	// �e�N�X�`���e�[�u��
	std::unordered_map<std::string, ComPtr<ID3D12Resource>> m_textureTable;

	// �f�o�b�O���C���[��L���ɂ���
	HRESULT EnableDebugLayer();
	// DXGIFactory�̏�����
	HRESULT InitializeIDXGIFactory();
	// �f�o�C�X�̏�����
	HRESULT InitializeID3D12Device();
	// �R�}���h�A���P�[�^�[�A���X�g�A�L���[�̏�����
	HRESULT InitializeCommand();

	// �ŏI�I�ȃ����_�[�^�[�Q�b�g�̐���
	HRESULT	CreateFinalRenderTargets();

	// �f�v�X�X�e���V���r���[�̐���
	HRESULT CreateDepthStencilView();

	// �X���b�v�`�F�C���̐���
	HRESULT CreateSwapChain(const HWND& hwnd);

	// �r���[�v���W�F�N�V�����p�r���[�̐���
	HRESULT CreateSceneView();

	// �t�F���X�̍쐬
	HRESULT CreateFence();

	// �e�N�X�`��������e�N�X�`���o�b�t�@�쐬�A���g���R�s�[
	ID3D12Resource* CreateTextureFromFile(const char* texpath);

	// �e�N�X�`�����[�_�e�[�u��
	void CreateTextureLoaderTable();

public:
	Dx12Wrapper(HWND hwnd); // �R���X�g���N�^
	~Dx12Wrapper() {} // �f�X�g���N�^

	void Update() {} // ���[�v����
	void BeginDraw(); // �S�̂̕`�揀��
	void EndDraw(); // �`��I���
	void SetScene(); // ���݂̃V�[��(�r���[�v���W�F�N�V����)���Z�b�g

	/// �e�N�X�`���p�X����K�v�ȃe�N�X�`���o�b�t�@�ւ̃|�C���^��Ԃ�
	/// @param texpath �e�N�X�`���t�@�C���p�X
	ComPtr<ID3D12Resource> GetTextureByPath(const char* texpath);

	ComPtr<ID3D12Device> Device() { return m_device; } // �f�o�C�X
	ComPtr<ID3D12GraphicsCommandList> CommandList() { return m_cmdList; } // �R�}���h���X�g
	ComPtr<IDXGISwapChain4> Swapchain() { return m_swapChain; } // �X���b�v�`�F�C��
};