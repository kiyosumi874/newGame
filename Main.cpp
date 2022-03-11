#include"Application.h"

// デバッグでないとき
#ifndef _DEBUG
int main()
{
#else
#include<Windows.h>
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
#endif
	// シングルトンインスタンスを生成
	auto& app = Application::Instance();

	if (FAILED(app.Init()))
	{
		return -1;
	}
	app.Run();
	app.Terminate();
	return 0;
}