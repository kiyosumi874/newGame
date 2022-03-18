#pragma once
#include <ResourceUploadBatch.h> // DirectXTK関連のリソースを使用するために必要
#include <SpriteFont.h> // 文字列を表示するのに必要なもの
#include <wrl.h>
#include <memory>
#include "Dx12Wrapper.h"

using namespace DirectX;
using namespace Microsoft::WRL;
using namespace std;
class Print
{
private:
	// DitectXTK
	unique_ptr<SpriteBatch> m_spriteBatch = nullptr; // スプライト表示用オブジェクト
	unique_ptr<SpriteFont> m_spriteFont = nullptr; // フォント表示用オブジェクト
	ComPtr<ID3D12DescriptorHeap> m_heapForSpriteFont = nullptr;

	Dx12Wrapper* m_dx12 = nullptr;
public:
	Print(Dx12Wrapper& dx12);
	~Print() {}

	void Draw();
	void DrawBegin();
	void DrawString(const char* str, DirectX::XMFLOAT2 pos, XMVECTORF32 color);
	void DrawEnd();

};