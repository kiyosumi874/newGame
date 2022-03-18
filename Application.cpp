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
	// �ʓ|�����Ǐ����Ȃ�������
	LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
		if (msg == WM_DESTROY) // �E�B���h�E���j�����ꂽ��Ă΂�܂�
		{
			PostQuitMessage(0); // OS�ɑ΂��āu�������̃A�v���͏I�����v�Ɠ`����
			return 0;
		}
		return DefWindowProc(hwnd, msg, wparam, lparam); // �K��̏������s��
	}
}

void Application::CreateGameWindow(HWND& hwnd, WNDCLASSEX& windowClass)
{
	// �E�B���h�E�N���X�������o�^
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.lpfnWndProc = (WNDPROC)WindowProcedure; // �R�[���o�b�N�֐��̎w��
	windowClass.lpszClassName = "DirectXTest"; // �A�v���P�[�V�����N���X��(�K���ł����ł�)
	windowClass.hInstance = GetModuleHandle(NULL); // �n���h���̎擾
	RegisterClassEx(&windowClass); // �A�v���P�[�V�����N���X(���������̍�邩���낵������OS�ɗ\������)

	RECT wrc = { 0,0, m_windowWidth, m_windowHeight }; // �E�B���h�E�T�C�Y�����߂�
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false); // �E�B���h�E�̃T�C�Y�͂�����Ɩʓ|�Ȃ̂Ŋ֐����g���ĕ␳����

	// �E�B���h�E�I�u�W�F�N�g�̐���
	hwnd = CreateWindow(windowClass.lpszClassName, // �N���X���w��
		_T("DX12���t�@�N�^�����O"), // �^�C�g���o�[�̕���
		WS_OVERLAPPEDWINDOW, // �^�C�g���o�[�Ƌ��E��������E�B���h�E�ł�
		CW_USEDEFAULT, // �\��X���W��OS�ɂ��C�����܂�
		CW_USEDEFAULT, // �\��Y���W��OS�ɂ��C�����܂�
		wrc.right - wrc.left, // �E�B���h�E��
		wrc.bottom - wrc.top, // �E�B���h�E��
		nullptr, // �e�E�B���h�E�n���h��
		nullptr, // ���j���[�n���h��
		windowClass.hInstance, // �Ăяo���A�v���P�[�V�����n���h��
		nullptr); // �ǉ��p�����[�^
}

HRESULT Application::Init()
{
	using namespace std;
	auto result = CoInitializeEx(0, COINIT_MULTITHREADED);
	// �Q�[���p�E�B���h�E�̍쐬
	CreateGameWindow(m_hwnd, m_windowClass);

	return result;
}

void Application::Run()
{
	

	auto dx12 = std::make_unique<Dx12Wrapper>(m_hwnd);
	auto input = std::make_unique<Input>();
	auto sceneCon = std::make_unique<SceneController>(*input.get(), *dx12.get());
	unique_ptr<GraphicsMemory> gmemory = make_unique<GraphicsMemory>(dx12->Device()); // �O���t�B�b�N�X�������I�u�W�F�N�g
	MSG msg = {};
	ShowWindow(m_hwnd, SW_SHOW); // �E�B���h�E�\��

	while (true)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		//�����A�v���P�[�V�������I�����Ď���message��WM_QUIT�ɂȂ�
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
	// �����N���X�g��񂩂�o�^�������Ă�
	UnregisterClass(m_windowClass.lpszClassName, m_windowClass.hInstance);
}