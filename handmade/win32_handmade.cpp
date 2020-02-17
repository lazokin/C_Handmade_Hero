#include <Windows.h>
#include <stdint.h>

static bool running = true;
static BITMAPINFO bitmapInfo;
static LPVOID bitmapBuffer;
static LONG bitmapWidth;
static LONG bitmapHeight;

static void funRender(int xOffset, int yOffset)
{
	int pitch = bitmapWidth * 4;
	PUINT8 buffer = (PUINT8)bitmapBuffer;
	for (int y = 0; y < bitmapHeight; y++)
	{
		PUINT32 pixel = (PUINT32)buffer;
		for (int x = 0; x < bitmapWidth; x++)
		{
			UINT8 blue = (x + xOffset);
			UINT8 green = (y + yOffset);
			*pixel++ = (green << 8 | blue);
		}
		buffer += pitch;
	}
}

static void ResizeDIBSection(LONG width, LONG height)
{
	if (bitmapBuffer)
	{
		VirtualFree(bitmapBuffer, NULL, MEM_RELEASE);
	}

	bitmapWidth = width;
	bitmapHeight = height;

	bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo.bmiHeader);
	bitmapInfo.bmiHeader.biWidth = bitmapWidth;
	bitmapInfo.bmiHeader.biHeight = -bitmapHeight;
	bitmapInfo.bmiHeader.biPlanes = 1;
	bitmapInfo.bmiHeader.biBitCount = 32;
	bitmapInfo.bmiHeader.biCompression = BI_RGB;

	bitmapBuffer = VirtualAlloc(NULL, bitmapWidth * bitmapHeight * 4, MEM_COMMIT, PAGE_READWRITE);

}

static void PaintWindow(HDC hdc, LPRECT lpClientRect, LONG x, LONG y, LONG width, LONG height)
{
	int clientWidth = lpClientRect->right - lpClientRect->left;
	int clientHeight = lpClientRect->bottom - lpClientRect->top;

	StretchDIBits(
		hdc,
		0, 0, bitmapWidth, bitmapHeight,
		0, 0, bitmapWidth, bitmapHeight,
		bitmapBuffer,
		&bitmapInfo,
		DIB_RGB_COLORS,
		SRCCOPY);

}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_ACTIVATEAPP:
		{
		}
		return 0;
	case WM_CLOSE:
		{
			running = false;
		}
		return 0;
	case WM_DESTROY:
		{
			running = false;
		}
		return 0;
	case WM_PAINT:
		{
			OutputDebugString(L"WM_PAINT\n");
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);
			LONG x = ps.rcPaint.left;
			LONG y = ps.rcPaint.top;
			LONG width = ps.rcPaint.right - ps.rcPaint.left;
			LONG height = ps.rcPaint.bottom - ps.rcPaint.top;
			RECT clientRect;
			GetClientRect(hWnd, &clientRect);
			PaintWindow(hdc, &clientRect, x, y, width, height);
			EndPaint(hWnd, &ps);
		}
		return 0;
	case WM_SIZE:
		{
		OutputDebugString(L"WM_SIZE\n");
			RECT clientRect;
			GetClientRect(hWnd, &clientRect);
			LONG width = clientRect.right - clientRect.left;
			LONG height = clientRect.bottom - clientRect.top;
			ResizeDIBSection(width, height);
		}
		return 0;
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

	int xOffset = 0;
	int yOffset = 0;

	while (running)
	{
		while (PeekMessage(&msg, hWnd, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
			{
				running = false;
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		funRender(xOffset++,  ++yOffset);

		HDC hdc = GetDC(hWnd);
		RECT clientRect;
		GetClientRect(hWnd, &clientRect);
		int clientWidth = clientRect.right - clientRect.left;
		int clientHeight = clientRect.bottom - clientRect.top;
		PaintWindow(hdc, &clientRect, 0, 0, clientWidth, clientHeight);
		ReleaseDC(hWnd, hdc);
	}

	return 0;
}