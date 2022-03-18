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
	// シェーダ側に投げられるマテリアルデータ
	struct MaterialForHlsl 
	{
		DirectX::XMFLOAT3 diffuse;  // ディフューズ色
		float alpha;                // ディフューズα
		DirectX::XMFLOAT3 specular; // スペキュラ色
		float specularity;          // スペキュラの強さ(乗算値)
		DirectX::XMFLOAT3 ambient;  // アンビエント色
	};
	// それ以外のマテリアルデータ
	struct AdditionalMaterial
	{
		std::string texPath; // テクスチャファイルパス
		int toonIndex;       // トゥーン番号
		bool edgeFlg;        // マテリアル毎の輪郭線フラグ
	};
	// まとめたもの
	struct Material
	{
		unsigned int indicesNum; //インデックス数
		MaterialForHlsl material;
		AdditionalMaterial additional;
	};

	struct Transform 
	{
		// 内部に持ってるXMMATRIXメンバが16バイトアライメントであるため
		// Transformをnewする際には16バイト境界に確保する
		void* operator new(size_t size);
		DirectX::XMMATRIX world;
	};
	
	struct BoneNode
	{
		uint32_t boneIndex;              // ボーンインデックス
		uint32_t boneType;               // ボーン種別
		uint32_t parentBone;
		uint32_t ikParentBone;           // IK親ボーン
		DirectX::XMFLOAT3 startPos;      // ボーン基準点(回転中心)
		std::vector<BoneNode*> children; // 子ノード
	};

	// キーフレーム構造体
	struct KeyFrame 
	{
		unsigned int frameNumber;     // フレーム№(アニメーション開始からの経過時間)
		DirectX::XMVECTOR quaternion; // クォータニオン
		DirectX::XMFLOAT3 offset;     // IKの初期座標からのオフセット情報
		DirectX::XMFLOAT2 p1, p2;     // ベジェの中間コントロールポイント
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
		uint16_t boneIndex;                // IK対象のボーンを示す
		uint16_t targetIndex;              // ターゲットに近づけるためのボーンのインデックス
		uint16_t iterations;               // 試行回数
		float limit;                       // 一回当たりの回転制限
		std::vector<uint16_t> nodeIndexes; // 間のノード番号
	};

	// IKオンオフデータ
	struct VMDIKEnable
	{
		uint32_t frameNumber;
		std::unordered_map<std::string, bool> ikEnableTable;
	};

	Dx12Wrapper* m_dx12Wrapper = nullptr;
	PMDRenderer* m_pmdRenderer = nullptr;

	// 頂点関係
	ComPtr<ID3D12Resource> m_vertexBuffer = nullptr;
	ComPtr<ID3D12Resource> m_indexBuffer = nullptr;
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView = {};
	D3D12_INDEX_BUFFER_VIEW m_indexBufferView = {};
	
	// 座標変換やらなんやら
	Transform m_transform;
	DirectX::XMMATRIX* m_mappedMatrices = nullptr;
	ComPtr<ID3D12Resource> m_transformBuffer = nullptr;
	ComPtr<ID3D12DescriptorHeap> m_transformHeap = nullptr;

	//マテリアル関連
	std::vector<Material> m_materials;
	ComPtr<ID3D12Resource> m_materialBuffer = nullptr;
	ComPtr<ID3D12DescriptorHeap> m_materialHeap = nullptr;//マテリアルヒープ(5個ぶん)
	std::vector<ComPtr<ID3D12Resource>> m_textureResources;
	std::vector<ComPtr<ID3D12Resource>> m_sphResources;
	std::vector<ComPtr<ID3D12Resource>> m_spaResources;
	std::vector<ComPtr<ID3D12Resource>> m_toonResources;

	//ボーン関連
	std::vector<DirectX::XMMATRIX> m_boneMatrices;
	std::map<std::string, BoneNode> m_boneNodeTable;
	std::vector<std::string> m_boneNameArray; // インデックスから名前を検索しやすいようにしておく
	std::vector<BoneNode*> m_boneNodeAddressArray; // インデックスからノードを検索しやすいようにしておく

	std::unordered_map<std::string, std::vector<KeyFrame>> m_motionData;

	std::vector<uint32_t> m_kneeIndexes;

	std::vector<PMDIK> m_inverseKinematicsData;

	std::vector<VMDIKEnable> m_inverseKinematicsEnableData;

	DWORD m_startTime = 0;//アニメーション開始時点のミリ秒時刻

	unsigned int m_duration = 0; // 最大フレーム番号
	float m_angle; // テスト用Y軸回転

	//PMDファイルのロード
	HRESULT LoadPMDFile(const char* path);
	
	//読み込んだマテリアルをもとにマテリアルバッファを作成
	HRESULT CreateMaterialData();

	//マテリアル＆テクスチャのビューを作成
	HRESULT CreateMaterialAndTextureView();

	//座標変換用ビューの生成
	HRESULT CreateTransformView();

	// 再帰処理
	void RecursiveMatrixMultipy(BoneNode* node, const DirectX::XMMATRIX& mat, bool flg = false);

	// ベジェ曲線補間メソッド（半分刻み法）:魔導書参照
	float GetYFromXOnBezier(float x, const DirectX::XMFLOAT2& a, const DirectX::XMFLOAT2& b, uint8_t n = 12);

	void MotionUpdate();

	///CCD-IKによりボーン方向を解決
	///@param ik 対象IKオブジェクト
	void SolveCCDIK(const PMDIK& ik);

	///余弦定理IKによりボーン方向を解決
	///@param ik 対象IKオブジェクト
	void SolveCosineIK(const PMDIK& ik);

	///LookAt行列によりボーン方向を解決
	///@param ik 対象IKオブジェクト
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