#include "handmade.h"

#include <Windows.h>
#include <xinput.h>
#include <dsound.h>
#include <math.h>

struct win32_offscreen_buffer
{
	BITMAPINFO Info;
	LPVOID Memory;
	INT Width;
	INT Height;
	INT Pitch;
	INT BytesPerPixel;
};

struct win32_dimensions
{
	INT Width;
	INT Height;
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

static BOOL GlobalRunning;
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

static void Win32InitDSound(HWND HWnd, INT32 SamplesPerSecond, INT32 BufferSize)
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

			if (SUCCEEDED(DirectSound->SetCooperativeLevel(HWnd, DSSCL_PRIORITY)))
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

static win32_dimensions Win32GetClientDimensions(HWND HWnd)
{
	win32_dimensions result;

	RECT clientRect;
	GetClientRect(HWnd, &clientRect);
	result.Width = clientRect.right - clientRect.left;
	result.Height = clientRect.bottom - clientRect.top;

	return result;
};

static void Win32ResizeDIBSection(win32_offscreen_buffer* Buffer, INT Width, INT Height)
{
	if (Buffer->Memory)
	{
		VirtualFree(Buffer->Memory, NULL, MEM_RELEASE);
	}

	Buffer->Width = Width;
	Buffer->Height = Height;
	Buffer->BytesPerPixel = 4;
	Buffer->Pitch = Buffer->Width * Buffer->BytesPerPixel;

	Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
	Buffer->Info.bmiHeader.biWidth = Buffer->Width;
	Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
	Buffer->Info.bmiHeader.biPlanes = 1;
	Buffer->Info.bmiHeader.biBitCount = 32;
	Buffer->Info.bmiHeader.biCompression = BI_RGB;

	Buffer->Memory = VirtualAlloc(NULL, Buffer->Width * Buffer->Height * Buffer->BytesPerPixel, MEM_COMMIT, PAGE_READWRITE);
}

static void Win32DisplayBufferInWindow(HDC hdc, INT ClientWidth, INT ClientHeight, win32_offscreen_buffer* Buffer)
{
	StretchDIBits(
		hdc,
		0, 0, ClientWidth, ClientHeight,
		0, 0, Buffer->Width, Buffer->Height,
		Buffer->Memory,
		&Buffer->Info,
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
		Win32DisplayBufferInWindow(hdc, dimensions.Width, dimensions.Height, &GlobalScreenBuffer);
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
	LARGE_INTEGER Frequency;
	QueryPerformanceFrequency(&Frequency);
	INT64 CounterFrequency = Frequency.QuadPart;

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

	INT XOffset = 0;
	INT YOffset = 0;

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

	GlobalRunning = true;

	LARGE_INTEGER PreviousCountStruct;
	QueryPerformanceCounter(&PreviousCountStruct);
	UINT64 PreviousCounts = PreviousCountStruct.QuadPart;
	UINT64 PreviousCycles = __rdtsc();
	
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

				XOffset += StickX / XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;
				YOffset -= StickY / XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;

				SoundOutput.Tone = 512 + (INT)(256.0f * (FLOAT)StickY / 30000.0f);
				SoundOutput.WavePeriod = SoundOutput.SampleRate / SoundOutput.Tone;

			}
			else
			{
				// Controller is not connected 
			}
		}

		game_offscreen_buffer GameScreenBuffer = {};
		GameScreenBuffer.Memory = GlobalScreenBuffer.Memory;
		GameScreenBuffer.Width = GlobalScreenBuffer.Width;
		GameScreenBuffer.Height = GlobalScreenBuffer.Height;
		GameScreenBuffer.Pitch = GlobalScreenBuffer.Pitch;
		GameUpdateAndRender(&GameScreenBuffer, XOffset, YOffset);

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
		Win32DisplayBufferInWindow(hdc, dimensions.Width, dimensions.Height, &GlobalScreenBuffer);
		ReleaseDC(hWnd, hdc);

		LARGE_INTEGER CurrentCountStruct;
		QueryPerformanceCounter(&CurrentCountStruct);
		UINT64 CurrentCounts = CurrentCountStruct.QuadPart;
		UINT64 CurrentCycles = __rdtsc();

		UINT64 CountsElapsed = CurrentCounts - PreviousCounts;
		UINT64 CyclesElapsed = CurrentCycles - PreviousCycles;

		INT32 MillisPerFrame  = (INT32)((1000 * CountsElapsed) / CounterFrequency);
		INT32 FramesPerSecond = (INT32)(CounterFrequency / CountsElapsed);
		INT32 MCyclesPerFrame = (INT32)(CyclesElapsed / (1000 * 1000));

		WCHAR Buffer[256];
		wsprintf(Buffer, L"%d ms, %d FPS, %d MCycles\n", MillisPerFrame, FramesPerSecond, MCyclesPerFrame);
		OutputDebugString(Buffer);

		PreviousCounts = CurrentCounts;
		PreviousCycles = CurrentCycles;
	}

	return 0;
}