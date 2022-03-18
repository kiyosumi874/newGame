#include "Input.h"
#include <Windows.h>
#include <vector>

Input::Input()
{
	std::vector<char> initKeyCode;
	initKeyCode.push_back(VK_LEFT);
	initKeyCode.push_back(VK_RIGHT);
	initKeyCode.push_back(VK_UP);
	initKeyCode.push_back(VK_DOWN);
	initKeyCode.push_back('0');
	
	for (int i = 0; i < BUTTON_ID_MAX; i++)
	{
		m_keys[i].keyCode = initKeyCode[i];
		m_keys[i].pressCount = -1;
	}
}

void Input::Update()
{
	for (int i = 0; i < BUTTON_ID_MAX; i++)
	{
		// GetAsyncKeyStateは押しているとき最上位ビットが立つ
		if (GetAsyncKeyState(m_keys[i].keyCode) & 0x8000) // 0x8000はshortの最上位ビットが立っていることを表す
		{
			m_keys[i].pressCount = max(++m_keys[i].pressCount, 1);
		}
		else
		{
			m_keys[i].pressCount = min(--m_keys[i].pressCount, 0);
		}
	}
}
