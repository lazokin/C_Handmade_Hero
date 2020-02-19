#include <Windows.h>
#include <stdint.h>
#include <xinput.h>

struct win32_offscreen_buffer
{
	BITMAPINFO info;
	LPVOID memory;
	INT width;
	INT height;
	INT pitch;
	INT bytesPerPixel;
};

struct win32_dimensions
{
	INT width;
	INT height;
};

static bool GlobalRunning = true;
static win32_offscreen_buffer GlobalBackBuffer;

#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE* pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub) { return 0; }
static x_input_get_state* XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub) { return 0; }
static x_input_set_state* XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

static void Win32LoadXInput(void)
{
	HMODULE XInputLibrary = LoadLibrary(L"xinput1_3.dll");
	if (XInputLibrary)
	{
		XInputGetState = (x_input_get_state*) GetProcAddress(XInputLibrary, "XInputGetState");
		if (!XInputGetState) { XInputGetState_ = XInputGetStateStub; }
		XInputSetState = (x_input_set_state*) GetProcAddress(XInputLibrary, "XInputSetState");
		if (!XInputSetState) { XInputSetState_ = XInputSetStateStub; }
	}
}

static win32_dimensions Win32GetClientDimensions(HWND hWnd)
{
	win32_dimensions result;

	RECT clientRect;
	GetClientRect(hWnd, &clientRect);
	result.width = clientRect.right - clientRect.left;
	result.height = clientRect.bottom - clientRect.top;

	return result;
};

static void FunRender(win32_offscreen_buffer* buffer, int xOffset, int yOffset)
{
	PUINT8 row = (PUINT8) buffer->memory;
	for (int y = 0; y < buffer->height; y++)
	{
		PUINT32 pixel = (PUINT32) row;
		for (int x = 0; x < buffer->width; x++)
		{
			UINT8 blue = (x + xOffset);
			UINT8 green = (y + yOffset);
			*pixel++ = (green << 8 | blue);
		}
		row += buffer->pitch;
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
	win32_offscreen_buffer* buffer)
{
	StretchDIBits(
		hdc,
		0, 0, clientWidth, clientHeight,
		0, 0, buffer->width, buffer->height,
		buffer->memory,
		&buffer->info,
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
		GlobalRunning = false;
	}
	return 0;

	case WM_DESTROY:
	{
		GlobalRunning = false;
	}
	return 0;

	case WM_SYSKEYUP:
	case WM_SYSKEYDOWN:
	case WM_KEYUP:
	case WM_KEYDOWN:
	{
		WPARAM VKCode = wParam;
		bool WasDown = (lParam & (1 << 30)) != 0;
		bool IsDown = (lParam & (1 << 31)) == 0;

		if (WasDown == IsDown)
		{
			return 0;
		}

		if (VKCode == 'W')
		{
			if (WasDown) {
				OutputDebugString(L"W was_down");
			}
			if (IsDown) {
				OutputDebugString(L"W is_down");
			}
			OutputDebugString(L"\n");
		}
		else if (VKCode == 'A')
		{
			OutputDebugString(L"A");
		}
		else if (VKCode == 'S')
		{
			OutputDebugString(L"S");
		}
		else if (VKCode == 'D')
		{
			OutputDebugString(L"D");
		}
		else if (VKCode == 'Q')
		{
			OutputDebugString(L"Q");
		}
		else if (VKCode == 'E')
		{
			OutputDebugString(L"E");
		}
		else if (VKCode == VK_UP)
		{
			OutputDebugString(L"VK_UP");
		}
		else if (VKCode == VK_DOWN)
		{
			OutputDebugString(L"VK_DOWN");
		}
		else if (VKCode == VK_LEFT)
		{
			OutputDebugString(L"VK_LEFT");
		}
		else if (VKCode == VK_RIGHT)
		{
			OutputDebugString(L"VK_RIGHT");
		}
		else if (VKCode == VK_ESCAPE)
		{
			OutputDebugString(L"VK_ESCAPE");
		}
		else if (VKCode == VK_SPACE)
		{
			OutputDebugString(L"VK_SPACE");
		}
	}
	return 0;

	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);
		win32_dimensions dimensions = Win32GetClientDimensions(hWnd);
		Win32DisplayBufferInWindow(hdc, dimensions.width, dimensions.height, &GlobalBackBuffer);
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
	Win32LoadXInput();

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

	while (GlobalRunning)
	{
		// handle window messages
		MSG msg = { };
		while (PeekMessage(&msg, hWnd, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
			{
				GlobalRunning = false;
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		// handle controller input
		DWORD dwResult;
		for (DWORD i = 0; i < XUSER_MAX_COUNT; i++)
		{
			XINPUT_STATE state;
			ZeroMemory(&state, sizeof(XINPUT_STATE));

			dwResult = XInputGetState(i, &state);

			if (dwResult == ERROR_SUCCESS)
			{
				XINPUT_GAMEPAD* gamepad = &state.Gamepad;

				bool Up = (gamepad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
				bool Down = (gamepad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
				bool Left = (gamepad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
				bool Right = (gamepad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
				bool Start = (gamepad->wButtons & XINPUT_GAMEPAD_START);
				bool Back = (gamepad->wButtons & XINPUT_GAMEPAD_BACK);
				bool LeftShoulder = (gamepad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
				bool RightShoulder = (gamepad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
				bool AButton  = (gamepad->wButtons & XINPUT_GAMEPAD_A);
				bool BButton = (gamepad->wButtons & XINPUT_GAMEPAD_B);
				bool XButton = (gamepad->wButtons & XINPUT_GAMEPAD_X);
				bool YButton = (gamepad->wButtons & XINPUT_GAMEPAD_Y);
				int16_t StickX = gamepad->sThumbLX;
				int16_t StickY = gamepad->sThumbLY;

				xOffset += StickX >> 12;
				yOffset -= StickY >> 12;

			}
			else
			{
				// Controller is not connected 
			}
		}

		FunRender(&GlobalBackBuffer, xOffset, yOffset);

		HDC hdc = GetDC(hWnd);
		win32_dimensions dimensions = Win32GetClientDimensions(hWnd);
		Win32DisplayBufferInWindow(hdc, dimensions.width, dimensions.height, &GlobalBackBuffer);
		ReleaseDC(hWnd, hdc);
	}

	return 0;
}