#include <Windows.h>
#include <xinput.h>
#include <dsound.h>
#include <math.h>

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

struct win32_sound_output
{
	INT SampleRate;
	INT BytesPerSample;
	INT SoundBufferSize;
	INT Tone;
	INT Volume;
	INT WavePeriod;
	UINT32 RunningSampleIndex;
	FLOAT tSine;
	INT LatencySampleCount;
};

constexpr auto PI32 = 3.14159265359f;

static BOOL GlobalRunning = true;
static win32_offscreen_buffer GlobalScreenBuffer;
static LPDIRECTSOUNDBUFFER GlobalSoundBuffer;

#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE* pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub) { return ERROR_DEVICE_NOT_CONNECTED; }
static x_input_get_state* XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub) { return ERROR_DEVICE_NOT_CONNECTED; }
static x_input_set_state* XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND* ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

static void Win32LoadXInput(void)
{
	HMODULE XInputLibrary = NULL;

	if (!XInputLibrary)
	{
		XInputLibrary = LoadLibrary(L"xinput1_4.dll");
	}

	if (!XInputLibrary)
	{
		XInputLibrary = LoadLibrary(L"xinput9_1_0.dll");
	}

	if (!XInputLibrary)
	{
		XInputLibrary = LoadLibrary(L"xinput1_3.dll");
	}

	if (XInputLibrary)
	{
		XInputGetState = (x_input_get_state*) GetProcAddress(XInputLibrary, "XInputGetState");
		if (!XInputGetState) { XInputGetState_ = XInputGetStateStub; }
		XInputSetState = (x_input_set_state*) GetProcAddress(XInputLibrary, "XInputSetState");
		if (!XInputSetState) { XInputSetState_ = XInputSetStateStub; }
	}
	else
	{
		// log error
	}
}

static void Win32InitDSound(HWND hWnd, INT32 SamplesPerSecond, INT32 BufferSize)
{
	HMODULE DSoundLibrary = LoadLibrary(L"dsound.dll");;

	if (DSoundLibrary)
	{
		direct_sound_create* DirectSoundCreate = (direct_sound_create*) GetProcAddress(DSoundLibrary, "DirectSoundCreate");

		LPDIRECTSOUND DirectSound;
		if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
		{
			WAVEFORMATEX WaveFormat = {};
			WaveFormat.cbSize = 0;
			WaveFormat.nChannels = 2;
			WaveFormat.nSamplesPerSec = SamplesPerSecond;
			WaveFormat.wBitsPerSample = 16;
			WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
			WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
			WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;

			if (SUCCEEDED(DirectSound->SetCooperativeLevel(hWnd, DSSCL_PRIORITY)))
			{
				DSBUFFERDESC BufferDesc = {};
				BufferDesc.dwSize = sizeof(BufferDesc);
				BufferDesc.dwFlags = DSBCAPS_PRIMARYBUFFER;
				LPDIRECTSOUNDBUFFER PrimaryBuffer;

				if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDesc, &PrimaryBuffer, 0)))
				{
					if (SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat)))
					{
						OutputDebugString(L"Primary Buffer Created\n");
					}
					else
					{
						// log error
					}
				}
				else
				{
					// log error
				}
			}
			else
			{
				// log error
			}

			DSBUFFERDESC BufferDesc = {};
			BufferDesc.dwSize = sizeof(BufferDesc);
			BufferDesc.dwFlags = 0;
			BufferDesc.dwBufferBytes = BufferSize;
			BufferDesc.lpwfxFormat = &WaveFormat;

			if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDesc, &GlobalSoundBuffer, 0)))
			{
				OutputDebugString(L"Secondary Buffer Created\n");
			}
			else
			{
				// log error
			}
		}
		else
		{
			// log error
		}
	}
	else
	{
		// log error
	}
}

static void Win32FillSoundBuffer(win32_sound_output* SoundOutput, DWORD Offset, DWORD Bytes)
{
	LPVOID AudioPtr1;
	DWORD AudioBytes1;
	LPVOID AudioPtr2;
	DWORD AudioBytes2;

	if (SUCCEEDED(GlobalSoundBuffer->Lock(Offset, Bytes, &AudioPtr1, &AudioBytes1, &AudioPtr2, &AudioBytes2, NULL)))
	{
		INT16* SampleOut1 = (INT16*)AudioPtr1;
		DWORD SampleCount1 = AudioBytes1 / SoundOutput->BytesPerSample;
		for (DWORD SampleIndex = 0; SampleIndex < SampleCount1; SampleIndex++)
		{
			INT16 SampleValue = (INT16)(SoundOutput->Volume * sinf(SoundOutput->tSine));
			*SampleOut1++ = SampleValue;
			*SampleOut1++ = SampleValue;
			SoundOutput->tSine += 2.0f * PI32 * 1.0f / (FLOAT)SoundOutput->WavePeriod;
			SoundOutput->RunningSampleIndex++;
		}


		INT16* SampleOut2 = (INT16*)AudioPtr2;
		DWORD SampleCount2 = AudioBytes2 / SoundOutput->BytesPerSample;
		for (DWORD SampleIndex = 0; SampleIndex < SampleCount2; SampleIndex++)
		{
			INT16 SampleValue = (INT16)(SoundOutput->Volume * sinf(SoundOutput->tSine));
			*SampleOut2++ = SampleValue;
			*SampleOut2++ = SampleValue;
			SoundOutput->tSine += 2.0f * PI32 * 1.0f / (FLOAT)SoundOutput->WavePeriod;
			SoundOutput->RunningSampleIndex++;
		}

		GlobalSoundBuffer->Unlock(AudioPtr1, AudioBytes1, AudioPtr2, AudioBytes2);
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

static void FunRender(win32_offscreen_buffer* buffer, INT xOffset, INT yOffset)
{
	PUINT8 row = (PUINT8) buffer->memory;
	for (INT y = 0; y < buffer->height; y++)
	{
		PUINT32 pixel = (PUINT32) row;
		for (INT x = 0; x < buffer->width; x++)
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
		BOOL AltKeyWasDown = ((lParam & (1 << 29)) != 0);
		BOOL WasDown = (lParam & (1 << 30)) != 0;
		BOOL IsDown = (lParam & (1 << 31)) == 0;

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
		else if (VKCode == VK_F4 && AltKeyWasDown)
		{
			GlobalRunning = false;
		}

	}
	return 0;

	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);
		win32_dimensions dimensions = Win32GetClientDimensions(hWnd);
		Win32DisplayBufferInWindow(hdc, dimensions.width, dimensions.height, &GlobalScreenBuffer);
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

INT WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, INT nCmdShow)
{
	Win32LoadXInput();

	WNDCLASS wc = {};

	Win32ResizeDIBSection(&GlobalScreenBuffer, 1280, 720);

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

	INT xOffset = 0;
	INT yOffset = 0;

	win32_sound_output SoundOutput = {};
	SoundOutput.SampleRate = 48000;
	SoundOutput.BytesPerSample = sizeof(INT16) * 2;
	SoundOutput.SoundBufferSize = SoundOutput.SampleRate * SoundOutput.BytesPerSample;
	SoundOutput.Tone = 256;
	SoundOutput.Volume = 3000;
	SoundOutput.WavePeriod = SoundOutput.SampleRate / SoundOutput.Tone;
	SoundOutput.LatencySampleCount = SoundOutput.SampleRate / 15;

	Win32InitDSound(hWnd, SoundOutput.SampleRate, SoundOutput.SoundBufferSize);
	Win32FillSoundBuffer(&SoundOutput, 0, SoundOutput.LatencySampleCount * SoundOutput.BytesPerSample);
	GlobalSoundBuffer->Play(0, 0, DSBPLAY_LOOPING);

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

				BOOL Up = (gamepad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
				BOOL Down = (gamepad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
				BOOL Left = (gamepad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
				BOOL Right = (gamepad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
				BOOL Start = (gamepad->wButtons & XINPUT_GAMEPAD_START);
				BOOL Back = (gamepad->wButtons & XINPUT_GAMEPAD_BACK);
				BOOL LeftShoulder = (gamepad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
				BOOL RightShoulder = (gamepad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
				BOOL AButton  = (gamepad->wButtons & XINPUT_GAMEPAD_A);
				BOOL BButton = (gamepad->wButtons & XINPUT_GAMEPAD_B);
				BOOL XButton = (gamepad->wButtons & XINPUT_GAMEPAD_X);
				BOOL YButton = (gamepad->wButtons & XINPUT_GAMEPAD_Y);
				INT16 StickX = gamepad->sThumbLX;
				INT16 StickY = gamepad->sThumbLY;

				xOffset += StickX / XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;
				yOffset += StickY / XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;

				SoundOutput.Tone = 512 + (INT)(256.0f * (FLOAT)StickY / 30000.0f);
				SoundOutput.WavePeriod = SoundOutput.SampleRate / SoundOutput.Tone;

			}
			else
			{
				// Controller is not connected 
			}
		}

		FunRender(&GlobalScreenBuffer, xOffset, yOffset);

		DWORD CurrentPlayCursor;
		DWORD CurrentWriteCursor;

		if (SUCCEEDED(GlobalSoundBuffer->GetCurrentPosition(&CurrentPlayCursor, &CurrentWriteCursor)))
		{
			DWORD Offset = (SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) % SoundOutput.SoundBufferSize;
			DWORD CurrentTargetCursor = (CurrentPlayCursor + (SoundOutput.LatencySampleCount * SoundOutput.BytesPerSample)) % SoundOutput.SoundBufferSize;
			DWORD Bytes;
			if (Offset > CurrentTargetCursor)
			{
				Bytes = (SoundOutput.SoundBufferSize - Offset) + CurrentTargetCursor;
			}
			else
			{
				Bytes = CurrentTargetCursor - Offset;
			}

			Win32FillSoundBuffer(&SoundOutput, Offset, Bytes);
		
		}

		HDC hdc = GetDC(hWnd);
		win32_dimensions dimensions = Win32GetClientDimensions(hWnd);
		Win32DisplayBufferInWindow(hdc, dimensions.width, dimensions.height, &GlobalScreenBuffer);
		ReleaseDC(hWnd, hdc);
	}

	return 0;
}