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

	m_dx12Wrapper.reset(new Dx12Wrapper(m_hwnd));
	m_print.reset(new Print(*m_dx12Wrapper));
	

	m_pmdRenderer.reset(new PMDRenderer(*m_dx12Wrapper));
	m_pmdActor.reset(new PMDActor("model/初音ミク.pmd", *m_pmdRenderer, *m_dx12Wrapper));
	m_pmdActor->LoadVMDFile("motion/motion.vmd");
	m_pmdActor->PlayAnimation();

	return result;
}

void Application::Run()
{
	MSG msg = {};
	ShowWindow(m_hwnd, SW_SHOW); // ウィンドウ表示
	auto input = std::make_shared<Input>();
	auto sceneCon = std::make_unique<SceneController>(*input.get());

	while (!input->IsDown(input->BUTTON_ID_UP))
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
		// 全体の描画準備
		m_dx12Wrapper->BeginDraw();
		// PMD用の描画パイプラインに合わせる
		m_dx12Wrapper->CommandList()->SetPipelineState(m_pmdRenderer->GetPipelineState());
		// ルートシグネチャもPMD用に合わせる
		m_dx12Wrapper->CommandList()->SetGraphicsRootSignature(m_pmdRenderer->GetRootSignature());
	
		m_dx12Wrapper->CommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		m_dx12Wrapper->SetScene();

		sceneCon->SceneUpdate();

		m_pmdActor->Update();
		m_pmdActor->Draw();

		m_print->Draw();
		
		m_dx12Wrapper->EndDraw();
		//フリップ
		m_dx12Wrapper->Swapchain()->Present(1, 0);
		m_print->Commit();
		
	}
	
}

void Application::Terminate()
{
	// もうクラス使わんから登録解除してや
	UnregisterClass(m_windowClass.lpszClassName, m_windowClass.hInstance);
}