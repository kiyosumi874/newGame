#pragma once
//#include <Xinput.h>

class Input
{
public:
    enum ButtonID
    {
        BUTTON_ID_LEFT,     // �\���L�[
        BUTTON_ID_RIGHT,
        BUTTON_ID_UP,
        BUTTON_ID_DOWN,
        BUTTON_ID_0,        // ���̑��{�^��
        /*BUTTON_ID_1,
        BUTTON_ID_2,
        BUTTON_ID_3,
        BUTTON_ID_4,
        BUTTON_ID_5,
        BUTTON_ID_6,
        BUTTON_ID_7,*/

        BUTTON_ID_MAX
    };
private:
    struct KeyInformation
    {
        char keyCode; // VK_SHIFT�Ƃ�'A'�̂��
        int pressCount; // ���
    };
    KeyInformation m_keys[BUTTON_ID_MAX];

public:

    Input();
	~Input() {}

	void Update();
    // �������u��
    bool IsDown(int buttonID)    const { return m_keys[buttonID].pressCount == 1; } 
    // �����Ă���Ƃ�
    bool IsPress(int buttonID)   const { return m_keys[buttonID].pressCount > 1;  } 
    // �������u��
    bool IsUp(int buttonID)      const { return m_keys[buttonID].pressCount == 0; } 
    // �����Ă���Ƃ�
    bool IsRelease(int buttonID) const { return m_keys[buttonID].pressCount < 0;  } 
};