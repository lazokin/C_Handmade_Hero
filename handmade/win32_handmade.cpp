#include <Windows.h>

static bool running = true;
static BITMAPINFO bmi;
static void* bits;
static HBITMAP dibSection;
static HDC hdc;

static void ResizeDIBSection(int width, int height)
{
	if (dibSection)
	{
		DeleteObject(dibSection);
	}
	
	if (!hdc)
	{
		hdc = CreateCompatibleDC(NULL);
	}

	bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
	bmi.bmiHeader.biWidth = width;
	bmi.bmiHeader.biHeight = height;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	dibSection = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, NULL, NULL);
}

static void PaintWindow(HDC hdc, int x, int y, int width, int height)
{
	StretchDIBits(hdc, x, y, width, height, x, y, width, height, &bits, &bmi, DIB_RGB_COLORS, SRCCOPY);

}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_ACTIVATEAPP:
		{
		} return 0;
	case WM_CLOSE:
		{
			running = false;
		} return 0;
	case WM_DESTROY:
		{
			running = false;
		} return 0;

	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);
			int x = ps.rcPaint.left;
			int y = ps.rcPaint.top;
			int width = ps.rcPaint.right - ps.rcPaint.left;
			int height = ps.rcPaint.bottom - ps.rcPaint.top;
			PaintWindow(hdc, x, y, width, height);
			EndPaint(hWnd, &ps);
		} return 0;
	case WM_SIZE:
		{
			RECT clientRect;
			GetClientRect(hWnd, &clientRect);
			int width = clientRect.right - clientRect.left;
			int height = clientRect.bottom - clientRect.top;
			ResizeDIBSection(width, height);
		} return 0;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
	WNDCLASS wc = {};

	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = L"Handmade Hero Window Class";

	RegisterClass(&wc);

	HWND hWnd = CreateWindowEx(
		0,
		wc.lpszClassName,
		L"Handmade Hero",
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		NULL,
		NULL,
		hInstance,
		NULL
	);

	if (hWnd == NULL)
	{
		return 0;
	}

	MSG msg = { };

	while (running)
	{
		if (GetMessage(&msg, hWnd, 0, 0) > 0)
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return 0;
}