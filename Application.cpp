#include "Application.h"
#include "Dx12Wrapper.h"
#include "PMDRenderer.h"
#include "PMDActor.h"
#include "Print.h"
#include "SceneController.h"
#include "Input.h"
#include <memory>
//#include <imgui.h>
//#include <imgui_impl_win32.h>
//#include <imgui_impl_dx12.h>

namespace
{
	// 面倒だけど書かなあかんやつ
	LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
		if (msg == WM_DESTROY) // ウィンドウが破棄されたら呼ばれます
		{
			PostQuitMessage(0); // OSに対して「もうこのアプリは終わるんや」と伝える
			return 0;
		}
		return DefWindowProc(hwnd, msg, wparam, lparam); // 規定の処理を行う
	}
}

void Application::CreateGameWindow(HWND& hwnd, WNDCLASSEX& windowClass)
{
	// ウィンドウクラス生成＆登録
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.lpfnWndProc = (WNDPROC)WindowProcedure; // コールバック関数の指定
	windowClass.lpszClassName = "DirectXTest"; // アプリケーションクラス名(適当でいいです)
	windowClass.hInstance = GetModuleHandle(NULL); // ハンドルの取得
	RegisterClassEx(&windowClass); // アプリケーションクラス(こういうの作るからよろしくってOSに予告する)

	RECT wrc = { 0,0, m_windowWidth, m_windowHeight }; // ウィンドウサイズを決める
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false); // ウィンドウのサイズはちょっと面倒なので関数を使って補正する

	// ウィンドウオブジェクトの生成
	hwnd = CreateWindow(windowClass.lpszClassName, // クラス名指定
		_T("DX12リファクタリング"), // タイトルバーの文字
		WS_OVERLAPPEDWINDOW, // タイトルバーと境界線があるウィンドウです
		CW_USEDEFAULT, // 表示X座標はOSにお任せします
		CW_USEDEFAULT, // 表示Y座標はOSにお任せします
		wrc.right - wrc.left, // ウィンドウ幅
		wrc.bottom - wrc.top, // ウィンドウ高
		nullptr, // 親ウィンドウハンドル
		nullptr, // メニューハンドル
		windowClass.hInstance, // 呼び出しアプリケーションハンドル
		nullptr); // 追加パラメータ
}

HRESULT Application::Init()
{
	using namespace std;
	auto result = CoInitializeEx(0, COINIT_MULTITHREADED);
	// ゲーム用ウィンドウの作成
	CreateGameWindow(m_hwnd, m_windowClass);

	return result;
}

void Application::Run()
{
	

	auto dx12 = std::make_unique<Dx12Wrapper>(m_hwnd);
	auto input = std::make_unique<Input>();
	auto sceneCon = std::make_unique<SceneController>(*input.get(), *dx12.get());
	unique_ptr<GraphicsMemory> gmemory = make_unique<GraphicsMemory>(dx12->Device()); // グラフィックスメモリオブジェクト
	MSG msg = {};
	ShowWindow(m_hwnd, SW_SHOW); // ウィンドウ表示

	while (true)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		//もうアプリケーションが終わるって時にmessageがWM_QUITになる
		if (msg.message == WM_QUIT)
		{
			break;
		}

		if (!sceneCon->SceneUpdate())
		{
			break;
		}
		
	}
	
}

void Application::Terminate()
{
	// もうクラス使わんから登録解除してや
	UnregisterClass(m_windowClass.lpszClassName, m_windowClass.hInstance);
}