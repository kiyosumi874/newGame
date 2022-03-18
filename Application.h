#pragma once
#include <Windows.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <d3dx12.h>
#include <dxgi1_6.h>
#include <DirectXTex.h>
#include <DirectXMath.h>
#include <tchar.h>
#include <wrl.h>
#include <memory>
#include <stdexcept>
#include <iostream>
#include <vector>
#include <map>
#include "Print.h"


class Dx12Wrapper;
class PMDRenderer;
class PMDActor;
// シングルトンクラス
class Application
{
private:
	// ウィンドウ周り
	WNDCLASSEX m_windowClass = {};
	HWND m_hwnd = {};
	std::shared_ptr<Dx12Wrapper> m_dx12Wrapper = nullptr;
	std::shared_ptr<PMDRenderer> m_pmdRenderer = nullptr;
	std::shared_ptr<PMDActor> m_pmdActor = nullptr;
	std::unique_ptr<Print> m_print = nullptr;
	

	// ウィンドウ定数
	const unsigned int m_windowWidth = 1280;
	const unsigned int m_windowHeight = 720;

	// ゲーム用ウィンドウの生成
	void CreateGameWindow(HWND& hwnd, WNDCLASSEX& windowClass);

	// ↓シングルトンのためにコンストラクタをprivateに
	// さらにコピーと代入を禁止に
	Application() {}
	Application(const Application&) = delete;
	void operator=(const Application&) = delete;
public:
	// Applicationのシングルトンインスタンスを得る
	static Application& Instance()
	{
		static Application instance;
		return instance;
	}

	// 初期化
	HRESULT Init();

	// ループ起動
	void Run();

	// 後処理
	void Terminate();

	SIZE GetWindowSize() const
	{
		SIZE ret;
		ret.cx = m_windowWidth;
		ret.cy = m_windowHeight;
		return ret;
	}

	~Application() {}
};