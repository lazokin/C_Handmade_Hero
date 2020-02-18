#include <Windows.h>
#include <stdint.h>

struct win32_offscreen_buffer
{
	BITMAPINFO info;
	LPVOID memory;
	INT width;
	INT height;
	INT pitch;
	INT bytesPerPixel;
};

static bool running = true;
static win32_offscreen_buffer GlobalBackBuffer;

struct win32_dimensions
{
	int width;
	int height;
};

win32_dimensions Win32GetClientDimensions(HWND hWnd)
{
	win32_dimensions result;

	RECT clientRect;
	GetClientRect(hWnd, &clientRect);
	result.width = clientRect.right - clientRect.left;
	result.height = clientRect.bottom - clientRect.top;

	return result;
};

static void FunRender(win32_offscreen_buffer buffer, int xOffset, int yOffset)
{
	PUINT8 row = (PUINT8) buffer.memory;
	for (int y = 0; y < buffer.height; y++)
	{
		PUINT32 pixel = (PUINT32) row;
		for (int x = 0; x < buffer.width; x++)
		{
			UINT8 blue = (x + xOffset);
			UINT8 green = (y + yOffset);
			*pixel++ = (green << 8 | blue);
		}
		row += buffer.pitch;
	}
}

static void Win32ResizeDIBSection(win32_offscreen_buffer* buffer, INT width, INT height)
{
	if (buffer->memory)
	{
		VirtualFree(buffer->memory, NULL, MEM_RELEASE);
	}

	buffer->width = width;
	buffer->height = height;
	buffer->bytesPerPixel = 4;
	buffer->pitch = buffer->width * buffer->bytesPerPixel;

	buffer->info.bmiHeader.biSize = sizeof(buffer->info.bmiHeader);
	buffer->info.bmiHeader.biWidth = buffer->width;
	buffer->info.bmiHeader.biHeight = -buffer->height;
	buffer->info.bmiHeader.biPlanes = 1;
	buffer->info.bmiHeader.biBitCount = 32;
	buffer->info.bmiHeader.biCompression = BI_RGB;

	buffer->memory = VirtualAlloc(NULL, buffer->width * buffer->height * buffer->bytesPerPixel, MEM_COMMIT, PAGE_READWRITE);
}

static void Win32DisplayBufferInWindow(
	HDC hdc, INT clientWidth, INT clientHeight,
	win32_offscreen_buffer buffer)
{
	StretchDIBits(
		hdc,
		0, 0, clientWidth, clientHeight,
		0, 0, buffer.width, buffer.height,
		buffer.memory,
		&buffer.info,
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
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);
			win32_dimensions dimensions = Win32GetClientDimensions(hWnd);
			Win32DisplayBufferInWindow(hdc, dimensions.width, dimensions.height, GlobalBackBuffer);
			EndPaint(hWnd, &ps);
		}
		return 0;
	case WM_SIZE:
		{
		}
		return 0;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
	WNDCLASS wc = {};

	Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);

	wc.style = CS_HREDRAW | CS_VREDRAW;
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

	int xOffset = 0;
	int yOffset = 0;

	while (running)
	{
		MSG msg = { };
		while (PeekMessage(&msg, hWnd, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
			{
				running = false;
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		FunRender(GlobalBackBuffer, xOffset++,  ++yOffset);

		HDC hdc = GetDC(hWnd);
		win32_dimensions dimensions = Win32GetClientDimensions(hWnd);
		Win32DisplayBufferInWindow(hdc, dimensions.width, dimensions.height, GlobalBackBuffer);
		ReleaseDC(hWnd, hdc);
	}

	return 0;
}