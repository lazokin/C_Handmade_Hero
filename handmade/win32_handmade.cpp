#include <Windows.h>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    MessageBox(0, L"This is Handmade Hero", L"Handmade Hero", MB_OK|MB_ICONINFORMATION);
	return 0;
}