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
// �V���O���g���N���X
class Application
{
private:
	// �E�B���h�E����
	WNDCLASSEX m_windowClass = {};
	HWND m_hwnd = {};
	std::shared_ptr<Dx12Wrapper> m_dx12Wrapper = nullptr;
	std::shared_ptr<PMDRenderer> m_pmdRenderer = nullptr;
	std::shared_ptr<PMDActor> m_pmdActor = nullptr;
	std::unique_ptr<Print> m_print = nullptr;
	

	// �E�B���h�E�萔
	const unsigned int m_windowWidth = 1280;
	const unsigned int m_windowHeight = 720;

	// �Q�[���p�E�B���h�E�̐���
	void CreateGameWindow(HWND& hwnd, WNDCLASSEX& windowClass);

	// ���V���O���g���̂��߂ɃR���X�g���N�^��private��
	// ����ɃR�s�[�Ƒ�����֎~��
	Application() {}
	Application(const Application&) = delete;
	void operator=(const Application&) = delete;
public:
	// Application�̃V���O���g���C���X�^���X�𓾂�
	static Application& Instance()
	{
		static Application instance;
		return instance;
	}

	// ������
	HRESULT Init();

	// ���[�v�N��
	void Run();

	// �㏈��
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