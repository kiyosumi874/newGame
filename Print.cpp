#include "Print.h"

#pragma comment(lib, "DirectXTK12.lib")
#pragma comment(lib, "dxguid.lib")

Print::Print(Dx12Wrapper& dx12)
	: m_dx12(&dx12)
{
	// フォント用処理
	m_gmemory.reset(new DirectX::GraphicsMemory(m_dx12->Device()));
	// SpriteBatchオブジェクトの初期化
	DirectX::ResourceUploadBatch resUploadBatch(m_dx12->Device());
	resUploadBatch.Begin();
	DirectX::RenderTargetState rtState(DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_D32_FLOAT);
	DirectX::SpriteBatchPipelineStateDescription pd(rtState);
	m_spriteBatch.reset(new DirectX::SpriteBatch(m_dx12->Device(),
		resUploadBatch,
		pd));

	// SpriteFontオブジェクトの初期化
	m_heapForSpriteFont = m_dx12->CreateDescriptorHeapForSpriteFont();
	m_spriteFont.reset(new DirectX::SpriteFont(m_dx12->Device(),
		resUploadBatch,
		L"font/fonttest.spritefont",
		m_heapForSpriteFont->GetCPUDescriptorHandleForHeapStart(),
		m_heapForSpriteFont->GetGPUDescriptorHandleForHeapStart()));

	auto future = resUploadBatch.End(m_dx12->CmdQue());
	m_dx12->WaitForCommandQueue();
	future.wait();
	m_spriteBatch->SetViewport(m_dx12->GetViewPort());
}

void Print::Draw()
{
	m_dx12->CommandList()->SetDescriptorHeaps(1, m_heapForSpriteFont.GetAddressOf());
	m_spriteBatch->Begin(m_dx12->CommandList());
	m_spriteFont->DrawString(m_spriteBatch.get(), "Hellow World", DirectX::XMFLOAT2(102, 102), DirectX::Colors::Black);
	m_spriteFont->DrawString(m_spriteBatch.get(), "Hellow World", DirectX::XMFLOAT2(100, 100), DirectX::Colors::Yellow);
	m_spriteBatch->End();
}

void Print::Commit()
{
	m_gmemory->Commit(m_dx12->CmdQue());
}

void Print::DrawBegin()
{
	m_dx12->CommandList()->SetDescriptorHeaps(1, m_heapForSpriteFont.GetAddressOf());
	m_spriteBatch->Begin(m_dx12->CommandList());
}

void Print::DrawString(const char* str, DirectX::XMFLOAT2 pos, XMVECTORF32 color)
{
	
	m_spriteFont->DrawString(m_spriteBatch.get(), str, pos, color);
}

void Print::DrawEnd()
{
	m_spriteBatch->End();
}
