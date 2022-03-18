#include "PMDActor.h"
#include "PMDRenderer.h"
#include "Dx12Wrapper.h"
#include <d3dx12.h>
#include <algorithm>
#include <sstream>
#include <array>

#pragma comment(lib,"winmm.lib")

using namespace Microsoft::WRL;
using namespace std;
using namespace DirectX;

//------------------------------------
// 無名名前空間
//------------------------------------
namespace
{
	//誤差の範囲内かどうかに使用する定数
	constexpr float epsilon = 0.0005f;
	///テクスチャのパスをセパレータ文字で分離する
	///@param path 対象のパス文字列
	///@param splitter 区切り文字
	///@return 分離前後の文字列ペア
	pair<string, string> SplitFileName(const std::string& path, const char splitter = '*')
	{
		int idx = path.find(splitter);
		pair<string, string> ret;
		ret.first = path.substr(0, idx);
		ret.second = path.substr(idx + 1, path.length() - idx - 1);
		return ret;
	}
	///ファイル名から拡張子を取得する
	///@param path 対象のパス文字列
	///@return 拡張子
	string GetExtension(const std::string& path) 
	{
		int idx = path.rfind('.');
		return path.substr(idx + 1, path.length() - idx - 1);
	}
	///モデルのパスとテクスチャのパスから合成パスを得る
	///@param modelPath アプリケーションから見たpmdモデルのパス
	///@param texPath PMDモデルから見たテクスチャのパス
	///@return アプリケーションから見たテクスチャのパス
	std::string GetTexturePathFromModelAndTexPath(const std::string& modelPath, const char* texPath)
	{
		//ファイルのフォルダ区切りは\と/の二種類が使用される可能性があり
		//ともかく末尾の\か/を得られればいいので、双方のrfindをとり比較する
		//int型に代入しているのは見つからなかった場合はrfindがepos(-1→0xffffffff)を返すため
		int pathIndex1 = modelPath.rfind('/');
		int pathIndex2 = modelPath.rfind('\\');
		auto pathIndex = max(pathIndex1, pathIndex2);
		auto folderPath = modelPath.substr(0, pathIndex + 1);
		return folderPath + texPath;
	}
	///Z軸を特定の方向を向かす行列を返す関数
	///@param lookat 向かせたい方向ベクトル
	///@param up 上ベクトル
	///@param right 右ベクトル
	XMMATRIX LookAtMatrix(const XMVECTOR& lookat, const XMFLOAT3& up, const XMFLOAT3& right) 
	{
		//向かせたい方向(z軸)
		XMVECTOR vz = lookat;

		//(向かせたい方向を向かせたときの)仮のy軸ベクトル
		XMVECTOR vy = XMVector3Normalize(XMLoadFloat3(&up));

		//(向かせたい方向を向かせたときの)y軸
		//XMVECTOR vx = XMVector3Normalize(XMVector3Cross(vz, vx));
		XMVECTOR vx = XMVector3Normalize(XMVector3Cross(vy, vz));
		vy = XMVector3Normalize(XMVector3Cross(vz, vx));

		///LookAtとupが同じ方向を向いてたらright基準で作り直す
		if (abs(XMVector3Dot(vy, vz).m128_f32[0]) == 1.0f) {
			//仮のX方向を定義
			vx = XMVector3Normalize(XMLoadFloat3(&right));
			//向かせたい方向を向かせたときのY軸を計算
			vy = XMVector3Normalize(XMVector3Cross(vz, vx));
			//真のX軸を計算
			vx = XMVector3Normalize(XMVector3Cross(vy, vz));
		}
		XMMATRIX ret = XMMatrixIdentity();
		ret.r[0] = vx;
		ret.r[1] = vy;
		ret.r[2] = vz;
		return ret;
	}

	///特定のベクトルを特定の方向に向けるための行列を返す
	///@param origin 特定のベクトル
	///@param lookat 向かせたい方向
	///@param up 上ベクトル
	///@param right 右ベクトル
	///@retval 特定のベクトルを特定の方向に向けるための行列
	XMMATRIX LookAtMatrix(const XMVECTOR& origin, const XMVECTOR& lookat, const XMFLOAT3& up, const XMFLOAT3& right)
	{
		return XMMatrixTranspose(LookAtMatrix(origin, up, right)) *
			LookAtMatrix(lookat, up, right);
	}

	//ボーン種別
	enum class BoneType 
	{
		Rotation,//回転
		RotAndMove,//回転＆移動
		IK,//IK
		Undefined,//未定義
		IKChild,//IK影響ボーン
		RotationChild,//回転影響ボーン
		IKDestination,//IK接続先
		Invisible//見えないボーン
	};
}
//--------------------------------------------
// publicClass
//--------------------------------------------
PMDActor::PMDActor(const char* filepath, PMDRenderer& renderer, Dx12Wrapper& wrapper)
	: m_pmdRenderer(&renderer)
	, m_dx12Wrapper(&wrapper)
	, m_angle(0.0f)
{
	m_transform.world = XMMatrixIdentity();
	LoadPMDFile(filepath);
	CreateTransformView();
	CreateMaterialData();
	CreateMaterialAndTextureView();
}

PMDActor::~PMDActor()
{
}

void PMDActor::LoadVMDFile(const char* filepath)
{
	FILE* fp = nullptr;
	fopen_s(&fp, filepath, "rb");
	fseek(fp, 50, SEEK_SET);//最初の50バイトは飛ばしてOK
	unsigned int keyframeNum = 0;
	fread(&keyframeNum, sizeof(keyframeNum), 1, fp);

	struct VMDKeyFrame
	{
		char boneName[15]; // ボーン名
		unsigned int frameNo; // フレーム番号(読込時は現在のフレーム位置を0とした相対位置)
		XMFLOAT3 location; // 位置
		XMFLOAT4 quaternion; // Quaternion // 回転
		unsigned char bezier[64]; // [4][4][4]  ベジェ補完パラメータ
	};
	vector<VMDKeyFrame> keyframes(keyframeNum);
	for (auto& keyframe : keyframes) 
	{
		fread(keyframe.boneName, sizeof(keyframe.boneName), 1, fp);//ボーン名
		fread(&keyframe.frameNo, sizeof(keyframe.frameNo) +//フレーム番号
			sizeof(keyframe.location) +//位置(IKのときに使用予定)
			sizeof(keyframe.quaternion) +//クオータニオン
			sizeof(keyframe.bezier), 1, fp);//補間ベジェデータ
	}

#pragma pack(1)
	//表情データ(頂点モーフデータ)
	struct VMDMorph
	{
		char name[15];//名前(パディングしてしまう)
		uint32_t frameNo;//フレーム番号
		float weight;//ウェイト(0.0f～1.0f)
	};//全部で23バイトなのでpragmapackで読む
#pragma pack()
	uint32_t morphCount = 0;
	fread(&morphCount, sizeof(morphCount), 1, fp);
	vector<VMDMorph> morphs(morphCount);
	fread(morphs.data(), sizeof(VMDMorph), morphCount, fp);

#pragma pack(1)
	//カメラ
	struct VMDCamera
	{
		uint32_t frameNo; // フレーム番号
		float distance; // 距離
		XMFLOAT3 pos; // 座標
		XMFLOAT3 eulerAngle; // オイラー角
		uint8_t Interpolation[24]; // 補完
		uint32_t fov; // 視界角
		uint8_t persFlg; // パースフラグON/OFF
	};//61バイト(これもpragma pack(1)の必要あり)
#pragma pack()
	uint32_t vmdCameraCount = 0;
	fread(&vmdCameraCount, sizeof(vmdCameraCount), 1, fp);
	vector<VMDCamera> cameraData(vmdCameraCount);
	fread(cameraData.data(), sizeof(VMDCamera), vmdCameraCount, fp);

	// ライト照明データ
	struct VMDLight 
	{
		uint32_t frameNo; // フレーム番号
		XMFLOAT3 rgb; //ライト色
		XMFLOAT3 vec; //光線ベクトル(平行光線)
	};

	uint32_t vmdLightCount = 0;
	fread(&vmdLightCount, sizeof(vmdLightCount), 1, fp);
	vector<VMDLight> lights(vmdLightCount);
	fread(lights.data(), sizeof(VMDLight), vmdLightCount, fp);

#pragma pack(1)
	// セルフ影データ
	struct VMDSelfShadow
	{
		uint32_t frameNo; // フレーム番号
		uint8_t mode; //影モード(0:影なし、1:モード１、2:モード２)
		float distance; //距離
	};
#pragma pack()
	uint32_t selfShadowCount = 0;
	fread(&selfShadowCount, sizeof(selfShadowCount), 1, fp);
	vector<VMDSelfShadow> selfShadowData(selfShadowCount);
	fread(selfShadowData.data(), sizeof(VMDSelfShadow), selfShadowCount, fp);

	//IKオンオフ切り替わり数
	uint32_t ikSwitchCount = 0;
	fread(&ikSwitchCount, sizeof(ikSwitchCount), 1, fp);
	//IK切り替えのデータ構造は少しだけ特殊で、いくつ切り替えようが
	//そのキーフレームは消費されます。その中で切り替える可能性のある
	//IKの名前とそのフラグがすべて登録されている状態です。

	//ここからは気を遣って読み込みます。キーフレームごとのデータであり
	//IKボーン(名前で検索)ごとにオン、オフフラグを持っているというデータであるとして
	//構造体を作っていきましょう。
	m_inverseKinematicsEnableData.resize(ikSwitchCount);
	for (auto& ikEnable : m_inverseKinematicsEnableData)
	{
		//キーフレーム情報なのでまずはフレーム番号読み込み
		fread(&ikEnable.frameNumber, sizeof(ikEnable.frameNumber), 1, fp);
		//次に可視フラグがありますがこれは使用しないので1バイトシークでも構いません
		uint8_t visibleFlg = 0;
		fread(&visibleFlg, sizeof(visibleFlg), 1, fp);
		//対象ボーン数読み込み
		uint32_t ikBoneCount = 0;
		fread(&ikBoneCount, sizeof(ikBoneCount), 1, fp);
		//ループしつつ名前とON/OFF情報を取得
		for (int i = 0; i < ikBoneCount; ++i)
		{
			char ikBoneName[20];
			fread(ikBoneName, _countof(ikBoneName), 1, fp);
			uint8_t flg = 0;
			fread(&flg, sizeof(flg), 1, fp);
			ikEnable.ikEnableTable[ikBoneName] = flg;
		}
	}
	fclose(fp);

	//VMDのキーフレームデータから、実際に使用するキーフレームテーブルへ変換
	for (auto& f : keyframes) 
	{
		m_motionData[f.boneName].emplace_back(KeyFrame(f.frameNo, XMLoadFloat4(&f.quaternion), f.location,
			XMFLOAT2((float)f.bezier[3] / 127.0f, (float)f.bezier[7] / 127.0f),
			XMFLOAT2((float)f.bezier[11] / 127.0f, (float)f.bezier[15] / 127.0f)));
		m_duration = std::max<unsigned int>(m_duration, f.frameNo);
	}

	for (auto& motion : m_motionData)
	{
		sort(motion.second.begin(), motion.second.end(),
			[](const KeyFrame& lval, const KeyFrame& rval)
			{
				return lval.frameNumber <= rval.frameNumber;
			});
	}

	for (auto& bonemotion : m_motionData) 
	{
		auto itBoneNode = m_boneNodeTable.find(bonemotion.first);
		if (itBoneNode == m_boneNodeTable.end())
		{
			continue;
		}
		auto node = itBoneNode->second;
		auto& pos = node.startPos;
		auto mat = XMMatrixTranslation(-pos.x, -pos.y, -pos.z) *
			XMMatrixRotationQuaternion(bonemotion.second[0].quaternion) *
			XMMatrixTranslation(pos.x, pos.y, pos.z);
		m_boneMatrices[node.boneIndex] = mat;
	}
	RecursiveMatrixMultipy(&m_boneNodeTable["センター"], XMMatrixIdentity());
	copy(m_boneMatrices.begin(), m_boneMatrices.end(), m_mappedMatrices + 1);
	
}

void PMDActor::Update()
{
	//_angle += 0.03f;
	//_mappedMatrices[0] = XMMatrixRotationY(_angle);
	MotionUpdate();
}

void PMDActor::Draw()
{
	m_dx12Wrapper->CommandList()->IASetVertexBuffers(0, 1, &m_vertexBufferView);
	m_dx12Wrapper->CommandList()->IASetIndexBuffer(&m_indexBufferView);

	m_dx12Wrapper->CommandList()->DrawInstanced(4, 1, 0, 0);

	ID3D12DescriptorHeap* transheaps[] = { m_transformHeap.Get() };
	m_dx12Wrapper->CommandList()->SetDescriptorHeaps(1, transheaps);
	m_dx12Wrapper->CommandList()->SetGraphicsRootDescriptorTable(1, m_transformHeap->GetGPUDescriptorHandleForHeapStart());



	ID3D12DescriptorHeap* mdh[] = { m_materialHeap.Get() };
	//マテリアル
	m_dx12Wrapper->CommandList()->SetDescriptorHeaps(1, mdh);

	auto materialH = m_materialHeap->GetGPUDescriptorHandleForHeapStart();
	unsigned int idxOffset = 0;

	auto cbvsrvIncSize = m_dx12Wrapper->Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 5;
	for (auto& m : m_materials) 
	{
		m_dx12Wrapper->CommandList()->SetGraphicsRootDescriptorTable(2, materialH);
		m_dx12Wrapper->CommandList()->DrawIndexedInstanced(m.indicesNum, 1, idxOffset, 0, 0);
		materialH.ptr += cbvsrvIncSize;
		idxOffset += m.indicesNum;
	}
}

void PMDActor::PlayAnimation()
{
	m_startTime = timeGetTime();
}
//--------------------------------------------
// privateClass
//--------------------------------------------

void PMDActor::RecursiveMatrixMultipy(BoneNode* node, const DirectX::XMMATRIX& mat, bool flg)
{
	m_boneMatrices[node->boneIndex] *= mat;

	for (auto& cnode : node->children)
	{
		RecursiveMatrixMultipy(cnode, m_boneMatrices[node->boneIndex]);
	}
}

HRESULT PMDActor::CreateMaterialAndTextureView()
{
	D3D12_DESCRIPTOR_HEAP_DESC materialDescHeapDesc = {};
	materialDescHeapDesc.NumDescriptors = m_materials.size() * 5;//マテリアル数ぶん(定数1つ、テクスチャ3つ)
	materialDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	materialDescHeapDesc.NodeMask = 0;

	materialDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;//デスクリプタヒープ種別
	auto result = m_dx12Wrapper->Device()->CreateDescriptorHeap(&materialDescHeapDesc, IID_PPV_ARGS(m_materialHeap.ReleaseAndGetAddressOf()));//生成
	if (FAILED(result)) 
	{
		assert(SUCCEEDED(result));
		return result;
	}
	auto materialBuffSize = sizeof(MaterialForHlsl);
	materialBuffSize = (materialBuffSize + 0xff) & ~0xff;
	D3D12_CONSTANT_BUFFER_VIEW_DESC matCBVDesc = {};
	matCBVDesc.BufferLocation = m_materialBuffer->GetGPUVirtualAddress();
	matCBVDesc.SizeInBytes = materialBuffSize;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;//後述
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;//2Dテクスチャ
	srvDesc.Texture2D.MipLevels = 1;//ミップマップは使用しないので1
	CD3DX12_CPU_DESCRIPTOR_HANDLE matDescHeapH(m_materialHeap->GetCPUDescriptorHandleForHeapStart());
	auto incSize = m_dx12Wrapper->Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	for (int i = 0; i < m_materials.size(); ++i)
	{
		//マテリアル固定バッファビュー
		m_dx12Wrapper->Device()->CreateConstantBufferView(&matCBVDesc, matDescHeapH);
		matDescHeapH.ptr += incSize;
		matCBVDesc.BufferLocation += materialBuffSize;
		if (m_textureResources[i] == nullptr) 
		{
			srvDesc.Format = m_pmdRenderer->GetWhiteTex()->GetDesc().Format;
			m_dx12Wrapper->Device()->CreateShaderResourceView(m_pmdRenderer->GetWhiteTex().Get(), &srvDesc, matDescHeapH);
		}
		else
		{
			srvDesc.Format = m_textureResources[i]->GetDesc().Format;
			m_dx12Wrapper->Device()->CreateShaderResourceView(m_textureResources[i].Get(), &srvDesc, matDescHeapH);
		}
		matDescHeapH.Offset(incSize);

		if (m_sphResources[i] == nullptr)
		{
			srvDesc.Format = m_pmdRenderer->GetWhiteTex()->GetDesc().Format;
			m_dx12Wrapper->Device()->CreateShaderResourceView(m_pmdRenderer->GetWhiteTex().Get(), &srvDesc, matDescHeapH);
		}
		else
		{
			srvDesc.Format = m_sphResources[i]->GetDesc().Format;
			m_dx12Wrapper->Device()->CreateShaderResourceView(m_sphResources[i].Get(), &srvDesc, matDescHeapH);
		}
		matDescHeapH.ptr += incSize;

		if (m_spaResources[i] == nullptr)
		{
			srvDesc.Format = m_pmdRenderer->GetBlackTex()->GetDesc().Format;
			m_dx12Wrapper->Device()->CreateShaderResourceView(m_pmdRenderer->GetBlackTex().Get(), &srvDesc, matDescHeapH);
		}
		else 
		{
			srvDesc.Format = m_spaResources[i]->GetDesc().Format;
			m_dx12Wrapper->Device()->CreateShaderResourceView(m_spaResources[i].Get(), &srvDesc, matDescHeapH);
		}
		matDescHeapH.ptr += incSize;


		if (m_toonResources[i] == nullptr)
		{
			srvDesc.Format = m_pmdRenderer->GetGradTex()->GetDesc().Format;
			m_dx12Wrapper->Device()->CreateShaderResourceView(m_pmdRenderer->GetGradTex().Get(), &srvDesc, matDescHeapH);
		}
		else 
		{
			srvDesc.Format = m_toonResources[i]->GetDesc().Format;
			m_dx12Wrapper->Device()->CreateShaderResourceView(m_toonResources[i].Get(), &srvDesc, matDescHeapH);
		}
		matDescHeapH.ptr += incSize;
	}
}

HRESULT PMDActor::CreateMaterialData()
{
	//マテリアルバッファを作成
	auto materialBuffSize = sizeof(MaterialForHlsl);
	materialBuffSize = (materialBuffSize + 0xff) & ~0xff;

	auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(materialBuffSize * m_materials.size());

	auto result = m_dx12Wrapper->Device()->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,//勿体ないけど仕方ないですね
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(m_materialBuffer.ReleaseAndGetAddressOf())
	);
	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	//マップマテリアルにコピー
	char* mapMaterial = nullptr;
	result = m_materialBuffer->Map(0, nullptr, (void**)&mapMaterial);
	if (FAILED(result)) 
	{
		assert(SUCCEEDED(result));
		return result;
	}
	for (auto& m : m_materials)
	{
		*((MaterialForHlsl*)mapMaterial) = m.material;//データコピー
		mapMaterial += materialBuffSize;//次のアライメント位置まで進める
	}
	m_materialBuffer->Unmap(0, nullptr);

	return S_OK;
}

HRESULT PMDActor::CreateTransformView()
{
	//GPUバッファ作成
	auto buffSize = sizeof(XMMATRIX) * (1 + m_boneMatrices.size());
	buffSize = (buffSize + 0xff) & ~0xff;

	auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(buffSize);

	auto result = m_dx12Wrapper->Device()->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(m_transformBuffer.ReleaseAndGetAddressOf())
	);
	if (FAILED(result)) 
	{
		assert(SUCCEEDED(result));
		return result;
	}

	//マップとコピー
	result = m_transformBuffer->Map(0, nullptr, (void**)&m_mappedMatrices);
	if (FAILED(result)) 
	{
		assert(SUCCEEDED(result));
		return result;
	}
	// ワールド変換行列のコピー
	m_mappedMatrices[0] = m_transform.world;

	//ビューの作成
	D3D12_DESCRIPTOR_HEAP_DESC transformDescHeapDesc = {};
	transformDescHeapDesc.NumDescriptors = 1;//とりあえずワールドひとつ
	transformDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	transformDescHeapDesc.NodeMask = 0;
	transformDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;//デスクリプタヒープ種別

	result = m_dx12Wrapper->Device()->CreateDescriptorHeap(&transformDescHeapDesc, IID_PPV_ARGS(m_transformHeap.ReleaseAndGetAddressOf()));//生成
	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = m_transformBuffer->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = buffSize;

	m_dx12Wrapper->Device()->CreateConstantBufferView(&cbvDesc, m_transformHeap->GetCPUDescriptorHandleForHeapStart());

	return S_OK;
}

HRESULT PMDActor::LoadPMDFile(const char* path)
{
	//PMDヘッダ構造体
	struct PMDHeader
	{
		float version; //例：00 00 80 3F == 1.00
		char model_name[20];//モデル名
		char comment[256];//モデルコメント
	};
	char signature[3];
	PMDHeader pmdheader = {};

	string strModelPath = path;
	FILE* fp = nullptr;
	fopen_s(&fp, strModelPath.c_str(), "rb");

	//auto fp = fopen(strModelPath.c_str(), "rb");

	if (fp == nullptr) 
	{
		//エラー処理
		assert(0);
		return ERROR_FILE_NOT_FOUND;
	}
	fread(signature, sizeof(signature[0]), 3, fp);
	fread(&pmdheader, sizeof(pmdheader), 1, fp);

	unsigned int vertNum;//頂点数
	fread(&vertNum, sizeof(vertNum), 1, fp);


#pragma pack(1)//ここから1バイトパッキング…アライメントは発生しない
	//PMDマテリアル構造体
	struct PMDMaterial
	{
		XMFLOAT3 diffuse; //ディフューズ色
		float alpha; // ディフューズα
		float specularity;//スペキュラの強さ(乗算値)
		XMFLOAT3 specular; //スペキュラ色
		XMFLOAT3 ambient; //アンビエント色
		unsigned char toonIdx; //トゥーン番号(後述)
		unsigned char edgeFlg;//マテリアル毎の輪郭線フラグ
		//2バイトのパディングが発生！！
		unsigned int indicesNum; //このマテリアルが割り当たるインデックス数
		char texFilePath[20]; //テクスチャファイル名(プラスアルファ…後述)
	};//70バイトのはず…でもパディングが発生するため72バイト
#pragma pack()//1バイトパッキング解除

	constexpr unsigned int pmdvertex_size = 38;//頂点1つあたりのサイズ
	std::vector<unsigned char> vertices(vertNum * pmdvertex_size);//バッファ確保
	fread(vertices.data(), sizeof(vertices[0]), vertNum * pmdvertex_size, fp);//一気に読み込み

	unsigned int indicesNum;//インデックス数
	fread(&indicesNum, sizeof(indicesNum), 1, fp);//

	auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(vertices.size() * sizeof(vertices[0]));

	//UPLOAD(確保は可能)
	auto result = m_dx12Wrapper->Device()->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(m_vertexBuffer.ReleaseAndGetAddressOf()));

	unsigned char* vertMap = nullptr;
	result = m_vertexBuffer->Map(0, nullptr, (void**)&vertMap);
	std::copy(vertices.begin(), vertices.end(), vertMap);
	m_vertexBuffer->Unmap(0, nullptr);


	m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();//バッファの仮想アドレス
	m_vertexBufferView.SizeInBytes = vertices.size();//全バイト数
	m_vertexBufferView.StrideInBytes = pmdvertex_size;//1頂点あたりのバイト数

	std::vector<unsigned short> indices(indicesNum);
	fread(indices.data(), sizeof(indices[0]), indicesNum, fp);//一気に読み込み


	auto resDescBuf = CD3DX12_RESOURCE_DESC::Buffer(indices.size() * sizeof(indices[0]));


	//設定は、バッファのサイズ以外頂点バッファの設定を使いまわして
	//OKだと思います。
	result = m_dx12Wrapper->Device()->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDescBuf,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(m_indexBuffer.ReleaseAndGetAddressOf()));

	//作ったバッファにインデックスデータをコピー
	unsigned short* mappedIdx = nullptr;
	m_indexBuffer->Map(0, nullptr, (void**)&mappedIdx);
	std::copy(indices.begin(), indices.end(), mappedIdx);
	m_indexBuffer->Unmap(0, nullptr);


	//インデックスバッファビューを作成
	m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
	m_indexBufferView.Format = DXGI_FORMAT_R16_UINT;
	m_indexBufferView.SizeInBytes = indices.size() * sizeof(indices[0]);

	unsigned int materialNum;
	fread(&materialNum, sizeof(materialNum), 1, fp);
	m_materials.resize(materialNum);
	m_textureResources.resize(materialNum);
	m_sphResources.resize(materialNum);
	m_spaResources.resize(materialNum);
	m_toonResources.resize(materialNum);

	std::vector<PMDMaterial> pmdMaterials(materialNum);
	fread(pmdMaterials.data(), sizeof(pmdMaterials[0]), materialNum, fp);
	//コピー
	for (int i = 0; i < pmdMaterials.size(); ++i) 
	{
		m_materials[i].indicesNum = pmdMaterials[i].indicesNum;
		m_materials[i].material.diffuse = pmdMaterials[i].diffuse;
		m_materials[i].material.alpha = pmdMaterials[i].alpha;
		m_materials[i].material.specular = pmdMaterials[i].specular;
		m_materials[i].material.specularity = pmdMaterials[i].specularity;
		m_materials[i].material.ambient = pmdMaterials[i].ambient;
		m_materials[i].additional.toonIndex = pmdMaterials[i].toonIdx;
	}

	for (int i = 0; i < pmdMaterials.size(); ++i)
	{
		//トゥーンリソースの読み込み
		char toonFilePath[32];
		sprintf_s(toonFilePath, "toon/toon02.bmp", pmdMaterials[i].toonIdx + 1);
		m_toonResources[i] = m_dx12Wrapper->GetTextureByPath(toonFilePath);

		if (strlen(pmdMaterials[i].texFilePath) == 0) 
		{
			m_textureResources[i] = nullptr;
			continue;
		}

		string texFileName = pmdMaterials[i].texFilePath;
		string sphFileName = "";
		string spaFileName = "";
		if (count(texFileName.begin(), texFileName.end(), '*') > 0) //スプリッタがある
		{
			auto namepair = SplitFileName(texFileName);
			if (GetExtension(namepair.first) == "sph")
			{
				texFileName = namepair.second;
				sphFileName = namepair.first;
			}
			else if (GetExtension(namepair.first) == "spa")
			{
				texFileName = namepair.second;
				spaFileName = namepair.first;
			}
			else {
				texFileName = namepair.first;
				if (GetExtension(namepair.second) == "sph")
				{
					sphFileName = namepair.second;
				}
				else if (GetExtension(namepair.second) == "spa") 
				{
					spaFileName = namepair.second;
				}
			}
		}
		else
		{
			if (GetExtension(pmdMaterials[i].texFilePath) == "sph")
			{
				sphFileName = pmdMaterials[i].texFilePath;
				texFileName = "";
			}
			else if (GetExtension(pmdMaterials[i].texFilePath) == "spa")
			{
				spaFileName = pmdMaterials[i].texFilePath;
				texFileName = "";
			}
			else 
			{
				texFileName = pmdMaterials[i].texFilePath;
			}
		}
		//モデルとテクスチャパスからアプリケーションからのテクスチャパスを得る
		if (texFileName != "") 
		{
			auto texFilePath = GetTexturePathFromModelAndTexPath(strModelPath, texFileName.c_str());
			m_textureResources[i] = m_dx12Wrapper->GetTextureByPath(texFilePath.c_str());
		}
		if (sphFileName != "")
		{
			auto sphFilePath = GetTexturePathFromModelAndTexPath(strModelPath, sphFileName.c_str());
			m_sphResources[i] = m_dx12Wrapper->GetTextureByPath(sphFilePath.c_str());
		}
		if (spaFileName != "") 
		{
			auto spaFilePath = GetTexturePathFromModelAndTexPath(strModelPath, spaFileName.c_str());
			m_spaResources[i] = m_dx12Wrapper->GetTextureByPath(spaFilePath.c_str());
		}
	}

	unsigned short boneNum = 0;
	fread(&boneNum, sizeof(boneNum), 1, fp);

#pragma pack(1)
	//読み込み用ボーン構造体
	struct Bone
	{
		char boneName[20];//ボーン名
		unsigned short parentNo;//親ボーン番号
		unsigned short nextNo;//先端のボーン番号
		unsigned char type;//ボーン種別
		unsigned short ikBoneNo;//IKボーン番号
		XMFLOAT3 pos;//ボーンの基準点座標
	};
#pragma pack()

	vector<Bone> pmdBones(boneNum);
	fread(pmdBones.data(), sizeof(pmdBones[0]), boneNum, fp);

	uint16_t ikNum = 0;
	fread(&ikNum, sizeof(ikNum), 1, fp);

	m_inverseKinematicsData.resize(ikNum);
	for (auto& ik : m_inverseKinematicsData) 
	{
		fread(&ik.boneIndex, sizeof(ik.boneIndex), 1, fp);
		fread(&ik.targetIndex, sizeof(ik.targetIndex), 1, fp);
		uint8_t chainLen = 0;
		fread(&chainLen, sizeof(chainLen), 1, fp);
		ik.nodeIndexes.resize(chainLen);
		fread(&ik.iterations, sizeof(ik.iterations), 1, fp);
		fread(&ik.limit, sizeof(ik.limit), 1, fp);
		if (chainLen == 0)continue;//間ノード数が0ならばここで終わり
		fread(ik.nodeIndexes.data(), sizeof(ik.nodeIndexes[0]), chainLen, fp);
	}

	fclose(fp);

	//読み込み後の処理
	m_boneNameArray.resize(pmdBones.size());
	m_boneNodeAddressArray.resize(pmdBones.size());

	//ボーン情報構築
	//ボーンノードマップを作る
	m_kneeIndexes.clear();
	for (int idx = 0; idx < pmdBones.size(); ++idx)
	{
		auto& pb = pmdBones[idx];
		auto& node = m_boneNodeTable[pb.boneName];
		node.boneIndex = idx;
		node.startPos = pb.pos;
		node.boneType = pb.type;
		node.parentBone = pb.parentNo;
		node.ikParentBone = pb.ikBoneNo;
		//インデックス検索がしやすいように
		m_boneNameArray[idx] = pb.boneName;
		m_boneNodeAddressArray[idx] = &node;
		string boneName = pb.boneName;
		if (boneName.find("ひざ") != std::string::npos) 
		{
			m_kneeIndexes.emplace_back(idx);
		}
	}

	//ツリー親子関係を構築する
	for (auto& pb : pmdBones) {
		//親インデックスをチェック(あり得ない番号なら飛ばす)
		if (pb.parentNo >= pmdBones.size()) {
			continue;
		}
		auto parentName = m_boneNameArray[pb.parentNo];
		m_boneNodeTable[parentName].children.emplace_back(&m_boneNodeTable[pb.boneName]);
	}

	//ボーン構築
	m_boneMatrices.resize(pmdBones.size());
	//ボーンをすべて初期化する。
	std::fill(m_boneMatrices.begin(), m_boneMatrices.end(), XMMatrixIdentity());

	//IKデバッグ用
	auto getNameFromIdx = [&](uint16_t idx)->string
	{
		auto it = find_if(m_boneNodeTable.begin(), m_boneNodeTable.end(), [idx](const pair<string, BoneNode>& obj)
			{
			return obj.second.boneIndex == idx;
			});
		if (it != m_boneNodeTable.end())
		{
			return it->first;
		}
		else 
		{
			return "";
		}
	};
	for (auto& ik : m_inverseKinematicsData)
	{
		std::ostringstream oss;
		oss << "IKボーン番号=" << ik.boneIndex << ":" << getNameFromIdx(ik.boneIndex) << endl;
		for (auto& node : ik.nodeIndexes) 
		{
			oss << "\tノードボーン=" << node << ":" << getNameFromIdx(node) << endl;
		}
		OutputDebugString(oss.str().c_str());
	}
}

void* PMDActor::Transform::operator new(size_t size)
{
	return _aligned_malloc(size, 16);
}

float PMDActor::GetYFromXOnBezier(float x, const DirectX::XMFLOAT2& a, const DirectX::XMFLOAT2& b, uint8_t n)
{
	if (a.x == a.y && b.x == b.y)return x;//計算不要
	float t = x;
	const float k0 = 1 + 3 * a.x - 3 * b.x;//t^3の係数
	const float k1 = 3 * b.x - 6 * a.x;//t^2の係数
	const float k2 = 3 * a.x;//tの係数


	for (int i = 0; i < n; ++i)
	{
		//f(t)求めまーす
		auto ft = k0 * t * t * t + k1 * t * t + k2 * t - x;
		//もし結果が0に近い(誤差の範囲内)なら打ち切り
		if (ft <= epsilon && ft >= -epsilon)break;

		t -= ft / 2;
	}
	//既に求めたいtは求めているのでyを計算する
	auto r = 1 - t;
	return t * t * t + 3 * t * t * r * b.y + 3 * t * r * r * a.y;
}

void PMDActor::MotionUpdate()
{
	auto  elapsedTime/*経過時間*/ = timeGetTime() - m_startTime;//経過時間を測る
	unsigned int frameNumber /*経過フレーム数*/ = 30 * (elapsedTime / 1000.0f);

	// 再生フレームを0に戻す
	if (frameNumber > m_duration)
	{
		m_startTime = timeGetTime();
		frameNumber = 0;
	}

	//行列情報クリア(してないと前フレームのポーズが重ね掛けされてモデルが壊れる)
	std::fill(m_boneMatrices.begin(), m_boneMatrices.end(), XMMatrixIdentity());

	//モーションデータ更新
	for (auto& bonemotion : m_motionData)
	{
		auto node = m_boneNodeTable[bonemotion.first];
		//合致するものを探す
		auto keyframes = bonemotion.second;

		auto rit = find_if(keyframes.rbegin(), keyframes.rend(), [frameNumber](const KeyFrame& keyframe)
			{
			return keyframe.frameNumber <= frameNumber;
			});

		if (rit == keyframes.rend())continue;//合致するものがなければ飛ばす

		XMMATRIX rotation;
		auto it = rit.base();
		if (it != keyframes.end()) 
		{
			// 線形補間のための値取得
			auto t = static_cast<float>(frameNumber - rit->frameNumber) /
				static_cast<float>(it->frameNumber - rit->frameNumber);
			t = GetYFromXOnBezier(t, it->p1, it->p2, 12);

			// 球面線形補間
			rotation = XMMatrixRotationQuaternion(
				XMQuaternionSlerp(rit->quaternion, it->quaternion, t)
			);
		}
		else 
		{
			rotation = XMMatrixRotationQuaternion(rit->quaternion);
		}

		auto& pos = node.startPos;
		auto mat = XMMatrixTranslation(-pos.x, -pos.y, -pos.z) * //原点に戻し
			rotation * //回転
			XMMatrixTranslation(pos.x, pos.y, pos.z);//元の座標に戻す
		m_boneMatrices[node.boneIndex] = mat;
	}
	RecursiveMatrixMultipy(&m_boneNodeTable["センター"], XMMatrixIdentity());

	IKSolve(frameNumber);

	copy(m_boneMatrices.begin(), m_boneMatrices.end(), m_mappedMatrices + 1);
}

void PMDActor::SolveCCDIK(const PMDIK& ik)
{
	//ターゲット
	auto targetBoneNode = m_boneNodeAddressArray[ik.boneIndex];
	auto targetOriginPos = XMLoadFloat3(&targetBoneNode->startPos);

	auto parentMat = m_boneMatrices[m_boneNodeAddressArray[ik.boneIndex]->ikParentBone];
	XMVECTOR det;
	auto invParentMat = XMMatrixInverse(&det, parentMat);
	auto targetNextPos = XMVector3Transform(targetOriginPos, m_boneMatrices[ik.boneIndex] * invParentMat);


	//まずはIKの間にあるボーンの座標を入れておく(逆順注意)
	std::vector<XMVECTOR> bonePositions;
	//auto endPos = XMVector3Transform(
	//	XMLoadFloat3(&_boneNodeAddressArray[ik.targetIdx]->startPos),
	//	//_boneMatrices[ik.targetIdx]);
	//	XMMatrixIdentity());
	//末端ノード
	auto endPos = XMLoadFloat3(&m_boneNodeAddressArray[ik.targetIndex]->startPos);
	//中間ノード(ルートを含む)
	for (auto& cidx : ik.nodeIndexes)
	{
		//bonePositions.emplace_back(XMVector3Transform(XMLoadFloat3(&_boneNodeAddressArray[cidx]->startPos),
			//_boneMatrices[cidx] ));
		bonePositions.push_back(XMLoadFloat3(&m_boneNodeAddressArray[cidx]->startPos));
	}

	vector<XMMATRIX> mats(bonePositions.size());
	fill(mats.begin(), mats.end(), XMMatrixIdentity());
	//ちょっとよくわからないが、PMDエディタの6.8°が0.03になっており、これは180で割っただけの値である。
	//つまりこれをラジアンとして使用するにはXM_PIを乗算しなければならない…と思われる。
	auto ikLimit = ik.limit * XM_PI;
	//ikに設定されている試行回数だけ繰り返す
	for (int c = 0; c < ik.iterations; ++c)
	{
		//ターゲットと末端がほぼ一致したら抜ける
		if (XMVector3Length(XMVectorSubtract(endPos, targetNextPos)).m128_f32[0] <= epsilon)
		{
			break;
		}
		//それぞれのボーンを遡りながら角度制限に引っ掛からないように曲げていく
		for (int bidx = 0; bidx < bonePositions.size(); ++bidx) 
		{
			const auto& pos = bonePositions[bidx];

			//まず現在のノードから末端までと、現在のノードからターゲットまでのベクトルを作る
			auto vecToEnd = XMVectorSubtract(endPos, pos);
			auto vecToTarget = XMVectorSubtract(targetNextPos, pos);
			vecToEnd = XMVector3Normalize(vecToEnd);
			vecToTarget = XMVector3Normalize(vecToTarget);

			//ほぼ同じベクトルになってしまった場合は外積できないため次のボーンに引き渡す
			if (XMVector3Length(XMVectorSubtract(vecToEnd, vecToTarget)).m128_f32[0] <= epsilon) 
			{
				continue;
			}
			//外積計算および角度計算
			auto cross = XMVector3Normalize(XMVector3Cross(vecToEnd, vecToTarget));
			float angle = XMVector3AngleBetweenVectors(vecToEnd, vecToTarget).m128_f32[0];
			angle = min(angle, ikLimit);//回転限界補正
			XMMATRIX rot = XMMatrixRotationAxis(cross, angle);//回転行列
			//posを中心に回転
			auto mat = XMMatrixTranslationFromVector(-pos) *
				rot *
				XMMatrixTranslationFromVector(pos);
			mats[bidx] *= mat;//回転行列を保持しておく(乗算で回転重ね掛けを作っておく)
			//対象となる点をすべて回転させる(現在の点から見て末端側を回転)
			for (auto idx = bidx - 1; idx >= 0; --idx)//自分を回転させる必要はない
			{
				bonePositions[idx] = XMVector3Transform(bonePositions[idx], mat);
			}
			endPos = XMVector3Transform(endPos, mat);
			//もし正解に近くなってたらループを抜ける
			if (XMVector3Length(XMVectorSubtract(endPos, targetNextPos)).m128_f32[0] <= epsilon)
			{
				break;
			}
		}
	}
	int idx = 0;
	for (auto& cidx : ik.nodeIndexes) 
	{
		m_boneMatrices[cidx] = mats[idx];
		++idx;
	}
	auto node = m_boneNodeAddressArray[ik.nodeIndexes.back()];
	RecursiveMatrixMultipy(node, parentMat, true);
}

void PMDActor::SolveCosineIK(const PMDIK& ik)
{
	vector<XMVECTOR> positions;//IK構成点を保存
	std::array<float, 2> edgeLens;//IKのそれぞれのボーン間の距離を保存

	//ターゲット(末端ボーンではなく、末端ボーンが近づく目標ボーンの座標を取得)
	auto& targetNode = m_boneNodeAddressArray[ik.boneIndex];
	auto targetPos = XMVector3Transform(XMLoadFloat3(&targetNode->startPos), m_boneMatrices[ik.boneIndex]);

	//IKチェーンが逆順なので、逆に並ぶようにしている
	//末端ボーン
	auto endNode = m_boneNodeAddressArray[ik.targetIndex];
	positions.emplace_back(XMLoadFloat3(&endNode->startPos));
	//中間及びルートボーン
	for (auto& chainBoneIdx : ik.nodeIndexes)
	{
		auto boneNode = m_boneNodeAddressArray[chainBoneIdx];
		positions.emplace_back(XMLoadFloat3(&boneNode->startPos));
	}
	//ちょっと分かりづらいと思ったので逆にしておきます。そうでもない人はそのまま
	//計算してもらって構わないです。
	reverse(positions.begin(), positions.end());

	//元の長さを測っておく
	edgeLens[0] = XMVector3Length(XMVectorSubtract(positions[1], positions[0])).m128_f32[0];
	edgeLens[1] = XMVector3Length(XMVectorSubtract(positions[2], positions[1])).m128_f32[0];

	//ルートボーン座標変換(逆順になっているため使用するインデックスに注意)
	positions[0] = XMVector3Transform(positions[0], m_boneMatrices[ik.nodeIndexes[1]]);
	//真ん中はどうせ自動計算されるので計算しない
	//先端ボーン
	positions[2] = XMVector3Transform(positions[2], m_boneMatrices[ik.boneIndex]);//ホンマはik.targetIdxだが…！？

	//ルートから先端へのベクトルを作っておく
	auto linearVec = XMVectorSubtract(positions[2], positions[0]);
	float A = XMVector3Length(linearVec).m128_f32[0];
	float B = edgeLens[0];
	float C = edgeLens[1];

	linearVec = XMVector3Normalize(linearVec);

	//ルートから真ん中への角度計算
	float theta1 = acosf((A * A + B * B - C * C) / (2 * A * B));

	//真ん中からターゲットへの角度計算
	float theta2 = acosf((B * B + C * C - A * A) / (2 * B * C));

	//「軸」を求める
	//もし真ん中が「ひざ」であった場合には強制的にX軸とする。
	XMVECTOR axis;
	if (find(m_kneeIndexes.begin(), m_kneeIndexes.end(), ik.nodeIndexes[0]) == m_kneeIndexes.end())
	{
		auto vm = XMVector3Normalize(XMVectorSubtract(positions[2], positions[0]));
		auto vt = XMVector3Normalize(XMVectorSubtract(targetPos, positions[0]));
		axis = XMVector3Cross(vt, vm);
	}
	else
	{
		auto right = XMFLOAT3(1, 0, 0);
		axis = XMLoadFloat3(&right);
	}

	//注意点…IKチェーンは根っこに向かってから数えられるため1が根っこに近い
	auto mat1 = XMMatrixTranslationFromVector(-positions[0]);
	mat1 *= XMMatrixRotationAxis(axis, theta1);
	mat1 *= XMMatrixTranslationFromVector(positions[0]);


	auto mat2 = XMMatrixTranslationFromVector(-positions[1]);
	mat2 *= XMMatrixRotationAxis(axis, theta2 - XM_PI);
	mat2 *= XMMatrixTranslationFromVector(positions[1]);

	m_boneMatrices[ik.nodeIndexes[1]] *= mat1;
	m_boneMatrices[ik.nodeIndexes[0]] = mat2 * m_boneMatrices[ik.nodeIndexes[1]];
	m_boneMatrices[ik.targetIndex] = m_boneMatrices[ik.nodeIndexes[0]];//直前の影響を受ける
	//_boneMatrices[ik.nodeIdxes[1]] = _boneMatrices[ik.boneIdx];
	//_boneMatrices[ik.nodeIdxes[0]] = _boneMatrices[ik.boneIdx];
	//_boneMatrices[ik.targetIdx] *= _boneMatrices[ik.boneIdx];
}

void PMDActor::SolveLookAt(const PMDIK& ik)
{
	//この関数に来た時点でノードはひとつしかなく、チェーンに入っているノード番号は
	//IKのルートノードのものなので、このルートノードからターゲットに向かうベクトルを考えればよい
	auto rootNode = m_boneNodeAddressArray[ik.nodeIndexes[0]];
	auto targetNode = m_boneNodeAddressArray[ik.targetIndex];//!?

	auto opos1 = XMLoadFloat3(&rootNode->startPos);
	auto tpos1 = XMLoadFloat3(&targetNode->startPos);

	auto opos2 = XMVector3Transform(opos1, m_boneMatrices[ik.nodeIndexes[0]]);
	auto tpos2 = XMVector3Transform(tpos1, m_boneMatrices[ik.boneIndex]);

	auto originVec = XMVectorSubtract(tpos1, opos1);
	auto targetVec = XMVectorSubtract(tpos2, opos2);

	originVec = XMVector3Normalize(originVec);
	targetVec = XMVector3Normalize(targetVec);

	XMMATRIX mat = XMMatrixTranslationFromVector(-opos2) *
		LookAtMatrix(originVec, targetVec, XMFLOAT3(0, 1, 0), XMFLOAT3(1, 0, 0)) *
		XMMatrixTranslationFromVector(opos2);

	//auto parent = _boneNodeAddressArray[ik.boneIdx]->parentBone;

	m_boneMatrices[ik.nodeIndexes[0]] = mat;// _boneMatrices[ik.boneIdx] * _boneMatrices[parent];
	//_boneMatrices[ik.targetIdx] = _boneMatrices[parent];
}

void PMDActor::IKSolve(int frameNo)
{
	//いつもの逆から検索
	auto it = find_if(m_inverseKinematicsEnableData.rbegin(), m_inverseKinematicsEnableData.rend(),
		[frameNo](const VMDIKEnable& ikenable)
		{
			return ikenable.frameNumber <= frameNo;
		});
	//まずはIKのターゲットボーンを動かす
	for (auto& ik : m_inverseKinematicsData) //IK解決のためのループ
	{
		if (it != m_inverseKinematicsEnableData.rend())
		{
			auto ikEnableIt = it->ikEnableTable.find(m_boneNameArray[ik.boneIndex]);
			if (ikEnableIt != it->ikEnableTable.end()) 
			{
				if (!ikEnableIt->second)//もしOFFなら打ち切ります
				{
					continue;
				}
			}
		}
		auto childrenNodesCount = ik.nodeIndexes.size();
		switch (childrenNodesCount) 
		{
		case 0://間のボーン数が0(ありえない)
			assert(0);
			continue;
		case 1://間のボーン数が1のときはLookAt
			SolveLookAt(ik);
			break;
		case 2://間のボーン数が2のときは余弦定理IK
			SolveCosineIK(ik);
			break;
		default://3以上の時はCCD-IK
			SolveCCDIK(ik);
		}
	}
}
