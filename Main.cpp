#include"Application.h"

// �f�o�b�O�łȂ��Ƃ�
#ifndef _DEBUG
int main()
{
#else
#include<Windows.h>
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
#endif
	// �V���O���g���C���X�^���X�𐶐�
	auto& app = Application::Instance();

	if (FAILED(app.Init()))
	{
		return -1;
	}
	app.Run();
	app.Terminate();
	return 0;
}