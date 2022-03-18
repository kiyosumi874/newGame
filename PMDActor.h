#pragma once
#include <d3d12.h>
#include <DirectXMath.h>
#include <wrl.h>
#include <vector>
#include <map>
#include <unordered_map>
#include <string>
#include <memory>

using namespace Microsoft::WRL;

class Dx12Wrapper;
class PMDRenderer;
class PMDActor
{
private:
	// �V�F�[�_���ɓ�������}�e���A���f�[�^
	struct MaterialForHlsl 
	{
		DirectX::XMFLOAT3 diffuse;  // �f�B�t���[�Y�F
		float alpha;                // �f�B�t���[�Y��
		DirectX::XMFLOAT3 specular; // �X�y�L�����F
		float specularity;          // �X�y�L�����̋���(��Z�l)
		DirectX::XMFLOAT3 ambient;  // �A���r�G���g�F
	};
	// ����ȊO�̃}�e���A���f�[�^
	struct AdditionalMaterial
	{
		std::string texPath; // �e�N�X�`���t�@�C���p�X
		int toonIndex;       // �g�D�[���ԍ�
		bool edgeFlg;        // �}�e���A�����̗֊s���t���O
	};
	// �܂Ƃ߂�����
	struct Material
	{
		unsigned int indicesNum; //�C���f�b�N�X��
		MaterialForHlsl material;
		AdditionalMaterial additional;
	};

	struct Transform 
	{
		// �����Ɏ����Ă�XMMATRIX�����o��16�o�C�g�A���C�����g�ł��邽��
		// Transform��new����ۂɂ�16�o�C�g���E�Ɋm�ۂ���
		void* operator new(size_t size);
		DirectX::XMMATRIX world;
	};
	
	struct BoneNode
	{
		uint32_t boneIndex;              // �{�[���C���f�b�N�X
		uint32_t boneType;               // �{�[�����
		uint32_t parentBone;
		uint32_t ikParentBone;           // IK�e�{�[��
		DirectX::XMFLOAT3 startPos;      // �{�[����_(��]���S)
		std::vector<BoneNode*> children; // �q�m�[�h
	};

	// �L�[�t���[���\����
	struct KeyFrame 
	{
		unsigned int frameNumber;     // �t���[����(�A�j���[�V�����J�n����̌o�ߎ���)
		DirectX::XMVECTOR quaternion; // �N�H�[�^�j�I��
		DirectX::XMFLOAT3 offset;     // IK�̏������W����̃I�t�Z�b�g���
		DirectX::XMFLOAT2 p1, p2;     // �x�W�F�̒��ԃR���g���[���|�C���g
		KeyFrame(
			unsigned int fno,
			const DirectX::XMVECTOR& q,
			const DirectX::XMFLOAT3& ofst,
			const DirectX::XMFLOAT2& ip1,
			const DirectX::XMFLOAT2& ip2) :
			frameNumber(fno),
			quaternion(q),
			offset(ofst),
			p1(ip1),
			p2(ip2) {}
	};

	struct PMDIK 
	{
		uint16_t boneIndex;                // IK�Ώۂ̃{�[��������
		uint16_t targetIndex;              // �^�[�Q�b�g�ɋ߂Â��邽�߂̃{�[���̃C���f�b�N�X
		uint16_t iterations;               // ���s��
		float limit;                       // ��񓖂���̉�]����
		std::vector<uint16_t> nodeIndexes; // �Ԃ̃m�[�h�ԍ�
	};

	// IK�I���I�t�f�[�^
	struct VMDIKEnable
	{
		uint32_t frameNumber;
		std::unordered_map<std::string, bool> ikEnableTable;
	};

	Dx12Wrapper* m_dx12Wrapper = nullptr;
	PMDRenderer* m_pmdRenderer = nullptr;

	// ���_�֌W
	ComPtr<ID3D12Resource> m_vertexBuffer = nullptr;
	ComPtr<ID3D12Resource> m_indexBuffer = nullptr;
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView = {};
	D3D12_INDEX_BUFFER_VIEW m_indexBufferView = {};
	
	// ���W�ϊ����Ȃ���
	Transform m_transform;
	DirectX::XMMATRIX* m_mappedMatrices = nullptr;
	ComPtr<ID3D12Resource> m_transformBuffer = nullptr;
	ComPtr<ID3D12DescriptorHeap> m_transformHeap = nullptr;

	//�}�e���A���֘A
	std::vector<Material> m_materials;
	ComPtr<ID3D12Resource> m_materialBuffer = nullptr;
	ComPtr<ID3D12DescriptorHeap> m_materialHeap = nullptr;//�}�e���A���q�[�v(5�Ԃ�)
	std::vector<ComPtr<ID3D12Resource>> m_textureResources;
	std::vector<ComPtr<ID3D12Resource>> m_sphResources;
	std::vector<ComPtr<ID3D12Resource>> m_spaResources;
	std::vector<ComPtr<ID3D12Resource>> m_toonResources;

	//�{�[���֘A
	std::vector<DirectX::XMMATRIX> m_boneMatrices;
	std::map<std::string, BoneNode> m_boneNodeTable;
	std::vector<std::string> m_boneNameArray; // �C���f�b�N�X���疼�O���������₷���悤�ɂ��Ă���
	std::vector<BoneNode*> m_boneNodeAddressArray; // �C���f�b�N�X����m�[�h���������₷���悤�ɂ��Ă���

	std::unordered_map<std::string, std::vector<KeyFrame>> m_motionData;

	std::vector<uint32_t> m_kneeIndexes;

	std::vector<PMDIK> m_inverseKinematicsData;

	std::vector<VMDIKEnable> m_inverseKinematicsEnableData;

	DWORD m_startTime = 0;//�A�j���[�V�����J�n���_�̃~���b����

	unsigned int m_duration = 0; // �ő�t���[���ԍ�
	float m_angle; // �e�X�g�pY����]

	//PMD�t�@�C���̃��[�h
	HRESULT LoadPMDFile(const char* path);
	
	//�ǂݍ��񂾃}�e���A�������ƂɃ}�e���A���o�b�t�@���쐬
	HRESULT CreateMaterialData();

	//�}�e���A�����e�N�X�`���̃r���[���쐬
	HRESULT CreateMaterialAndTextureView();

	//���W�ϊ��p�r���[�̐���
	HRESULT CreateTransformView();

	// �ċA����
	void RecursiveMatrixMultipy(BoneNode* node, const DirectX::XMMATRIX& mat, bool flg = false);

	// �x�W�F�Ȑ���ԃ��\�b�h�i�������ݖ@�j:�������Q��
	float GetYFromXOnBezier(float x, const DirectX::XMFLOAT2& a, const DirectX::XMFLOAT2& b, uint8_t n = 12);

	void MotionUpdate();

	///CCD-IK�ɂ��{�[������������
	///@param ik �Ώ�IK�I�u�W�F�N�g
	void SolveCCDIK(const PMDIK& ik);

	///�]���藝IK�ɂ��{�[������������
	///@param ik �Ώ�IK�I�u�W�F�N�g
	void SolveCosineIK(const PMDIK& ik);

	///LookAt�s��ɂ��{�[������������
	///@param ik �Ώ�IK�I�u�W�F�N�g
	void SolveLookAt(const PMDIK& ik);

	void IKSolve(int frameNo);
public:
	PMDActor(const char* filepath, PMDRenderer& renderer, Dx12Wrapper& wrapper);
	~PMDActor();
	void LoadVMDFile(const char* filepath);
	void Update();
	void Draw();
	void PlayAnimation();
};