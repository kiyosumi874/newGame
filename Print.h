#pragma once
#include <ResourceUploadBatch.h> // DirectXTK�֘A�̃��\�[�X���g�p���邽�߂ɕK�v
#include <SpriteFont.h> // �������\������̂ɕK�v�Ȃ���
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
	unique_ptr<SpriteBatch> m_spriteBatch = nullptr; // �X�v���C�g�\���p�I�u�W�F�N�g
	unique_ptr<SpriteFont> m_spriteFont = nullptr; // �t�H���g�\���p�I�u�W�F�N�g
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