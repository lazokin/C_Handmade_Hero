#include "handmade.h"
#include "win32_handmade.h"

#include <xinput.h>
#include <dsound.h>
#include <malloc.h>

static BOOL GlobalRunning;
static win32_offscreen_buffer GlobalScreenBuffer;
static LPDIRECTSOUNDBUFFER GlobalSoundBuffer;
static INT64 PerformanceFrequency;

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

// timings

inline LARGE_INTEGER Win32GetPerformanceCounter()
{
	LARGE_INTEGER Result;
	QueryPerformanceCounter(&Result);
	return Result;
}

inline FLOAT Win32GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End)
{
	return (FLOAT)(End.QuadPart - Start.QuadPart) / (FLOAT)PerformanceFrequency;
}

// files

debug_real_file_result DEBUGPlatformReadEntireFile(wchar_t* Filename)
{
	debug_real_file_result Result = {};

	HANDLE FileHandle = CreateFile(Filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, NULL, NULL);
	if (FileHandle != INVALID_HANDLE_VALUE)
	{
		LARGE_INTEGER FileSize64;
		if (GetFileSizeEx(FileHandle, &FileSize64))
		{
			DWORD FileSize32 = SafeTruncate64To32(FileSize64.QuadPart);
			Result.Bytes = VirtualAlloc(NULL, FileSize32, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
			if (Result.Bytes)
			{
				DWORD BytesRead;
				if (ReadFile(FileHandle, Result.Bytes, FileSize32, &BytesRead, NULL) && (BytesRead == FileSize32))
				{
					// read from file succesfullly
					Result.Size = FileSize32;
				}
				else
				{
					// failed to read from file
					DEBUGFreeFileFromMemory(Result.Bytes);
					Result.Size = 0;
				}
			}
			else
			{
				// failed to allocate memory
			}
		}
		else
		{
			// failed to get file size
		}
		CloseHandle(FileHandle);
	}
	else
	{
		// failed to open file
	}
	return Result;
}

void DEBUGFreeFileFromMemory(void* Memory)
{
	if (Memory)
	{
		VirtualFree(Memory, NULL, MEM_RELEASE);
	}

}

bool DEBUGPlatformWriteEntireFile(wchar_t* Filename, uint32_t FileSize, void* Bytes)
{
	bool Result = false;

	HANDLE FileHandle = CreateFile(Filename, GENERIC_WRITE, NULL, NULL, CREATE_ALWAYS, NULL, NULL);
	if (FileHandle != INVALID_HANDLE_VALUE)
	{
		DWORD BytesWritten;
		if (WriteFile(FileHandle, Bytes, FileSize, &BytesWritten, NULL) && (BytesWritten == FileSize))
		{
			// wrote to file succesfullly
			Result = true;
		}
		else
		{
			// failed to write to file
		}
		CloseHandle(FileHandle);
	}
	else
	{
		// failed to open file
	}
	return Result;
}

// input

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

static FLOAT Win32ProcessXInputStickInput(SHORT Value, SHORT DeadZone)
{
	FLOAT Result = 0.0f;
	if (Value < -DeadZone)
	{
		Result = (FLOAT)((Value + DeadZone) / (MINSHORT - DeadZone));
	}

	if (Value > DeadZone)
	{
		Result = (FLOAT)((Value - DeadZone) / (MINSHORT - DeadZone));
	}
	return Result;
}

static void Win32ProcessXInputDigitalButton(game_button_state* NewState, game_button_state* OldState, WORD XInputButtonsState, DWORD XInputButtonMask)
{
	NewState->EndedDown = (XInputButtonsState & XInputButtonMask);
	NewState->HalfTransitionCount = OldState->EndedDown != NewState->EndedDown ? 1 : 0;
}

static void Win32ProcessKeyboardInput(game_button_state* NewState, BOOL IsDown)
{
	NewState->EndedDown = IsDown;
	NewState->HalfTransitionCount++;
}

// sound

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

static void Win32ClearSoundBuffer(win32_sound_output* SoundOutput)
{
	LPVOID AudioPtr1;
	DWORD AudioBytes1;
	LPVOID AudioPtr2;
	DWORD AudioBytes2;

	if (SUCCEEDED(GlobalSoundBuffer->Lock(0, SoundOutput->SoundBufferSize, &AudioPtr1, &AudioBytes1, &AudioPtr2, &AudioBytes2, NULL)))
	{
		UINT8* AudioByte1 = (UINT8*)AudioPtr1;
		UINT8* AudioByte2 = (UINT8*)AudioPtr2;
		for (DWORD ByteIndex1 = 0; ByteIndex1 < AudioBytes1; ByteIndex1++) *AudioByte1++ = 0;
		for (DWORD ByteIndex2 = 0; ByteIndex2 < AudioBytes2; ByteIndex2++) *AudioByte2++ = 0;

		GlobalSoundBuffer->Unlock(AudioPtr1, AudioBytes1, AudioPtr2, AudioBytes2);
	}
}

static void Win32FillSoundBuffer(win32_sound_output* SoundOutput, DWORD Offset, DWORD Bytes, game_sound_buffer* Buffer)
{
	LPVOID AudioPtr1;
	DWORD AudioBytes1;
	LPVOID AudioPtr2;
	DWORD AudioBytes2;

	if (SUCCEEDED(GlobalSoundBuffer->Lock(Offset, Bytes, &AudioPtr1, &AudioBytes1, &AudioPtr2, &AudioBytes2, NULL)))
	{
		INT16* SrcSample = (INT16*)Buffer->Memory;

		INT16* DstSample1 = (INT16*)AudioPtr1;
		DWORD SampleCount1 = AudioBytes1 / SoundOutput->BytesPerSample;
		for (DWORD SampleIndex = 0; SampleIndex < SampleCount1; SampleIndex++)
		{
			*DstSample1++ = *SrcSample++;
			*DstSample1++ = *SrcSample++;
			SoundOutput->RunningSampleIndex++;
		}


		INT16* DstSample2 = (INT16*)AudioPtr2;
		DWORD SampleCount2 = AudioBytes2 / SoundOutput->BytesPerSample;
		for (DWORD SampleIndex = 0; SampleIndex < SampleCount2; SampleIndex++)
		{
			*DstSample2++ = *SrcSample++;
			*DstSample2++ = *SrcSample++;
			SoundOutput->RunningSampleIndex++;
		}

		GlobalSoundBuffer->Unlock(AudioPtr1, AudioBytes1, AudioPtr2, AudioBytes2);
	}
}

// video

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

	Buffer->Memory = VirtualAlloc(NULL, Buffer->Width * Buffer->Height * Buffer->BytesPerPixel, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
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

// windows

static void Win32ProcessMessages(HWND HWnd, game_controller_input* KeyboardController)
{
	MSG msg = { };
	while (PeekMessage(&msg, HWnd, 0, 0, PM_REMOVE))
	{
		switch (msg.message)
		{
		case WM_QUIT:
		{
			GlobalRunning = false;
		}
		case WM_SYSKEYUP:
		case WM_SYSKEYDOWN:
		case WM_KEYUP:
		case WM_KEYDOWN:
		{
			UINT32 VKCode = (UINT32)msg.wParam;
			BOOL AltKeyWasDown = ((msg.lParam & (1 << 29)) != 0);
			BOOL WasDown = (msg.lParam & (1 << 30)) != 0;
			BOOL IsDown = (msg.lParam & (1 << 31)) == 0;

			if (WasDown != IsDown)
			{
				if (VKCode == 'W')
				{
					Win32ProcessKeyboardInput(&KeyboardController->MoveUp, IsDown);
				}
				else if (VKCode == 'S')
				{
					Win32ProcessKeyboardInput(&KeyboardController->MoveDown, IsDown);
				}
				else if (VKCode == 'A')
				{
					Win32ProcessKeyboardInput(&KeyboardController->MoveLeft, IsDown);
				}
				else if (VKCode == 'D')
				{
					Win32ProcessKeyboardInput(&KeyboardController->MoveRight, IsDown);
				}
				else if (VKCode == 'Q')
				{
					Win32ProcessKeyboardInput(&KeyboardController->LeftShoulder, IsDown);
				}
				else if (VKCode == 'E')
				{
					Win32ProcessKeyboardInput(&KeyboardController->RightShoulder, IsDown);
				}
				else if (VKCode == VK_UP)
				{
					Win32ProcessKeyboardInput(&KeyboardController->ActionUp, IsDown);
				}
				else if (VKCode == VK_DOWN)
				{
					Win32ProcessKeyboardInput(&KeyboardController->ActionDown, IsDown);
				}
				else if (VKCode == VK_LEFT)
				{
					Win32ProcessKeyboardInput(&KeyboardController->ActionLeft, IsDown);
				}
				else if (VKCode == VK_RIGHT)
				{
					Win32ProcessKeyboardInput(&KeyboardController->ActionRight, IsDown);
				}
				else if (VKCode == VK_RETURN)
				{
					Win32ProcessKeyboardInput(&KeyboardController->Start, IsDown);
				}
				else if (VKCode == VK_ESCAPE)
				{
					Win32ProcessKeyboardInput(&KeyboardController->Back, IsDown);
				}
				else if (VKCode == VK_F4 && AltKeyWasDown)
				{
					GlobalRunning = false;
				}
			}
			break;
		}
		default:
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		}
	}
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

// main

INT WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, INT nCmdShow)
{
	LARGE_INTEGER PerformanceFrequencyHolder;
	QueryPerformanceFrequency(&PerformanceFrequencyHolder);
	PerformanceFrequency = PerformanceFrequencyHolder.QuadPart;

	bool SleepIsGrandular = (timeBeginPeriod(1) == TIMERR_NOERROR);

	Win32LoadXInput();

	WNDCLASS wc = {};

	Win32ResizeDIBSection(&GlobalScreenBuffer, 1280, 720);

	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = L"Handmade Hero Window Class";

	ATOM clazz = RegisterClass(&wc);

	if (!clazz) return 1;

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

	if (!hWnd) return 1;

	INT MonitorFrequency = 60;
	INT TargetFrameFrequency = MonitorFrequency / 2;
	FLOAT TargetFramePeriod = 1.0f / (FLOAT)TargetFrameFrequency;

	win32_sound_output SoundOutput = {};
	SoundOutput.SampleRate = 48000;
	SoundOutput.BytesPerSample = sizeof(INT16) * 2;
	SoundOutput.SoundBufferSize = SoundOutput.SampleRate * SoundOutput.BytesPerSample;
	SoundOutput.LatencySampleCount = SoundOutput.SampleRate / 15;
	SoundOutput.Memory = VirtualAlloc(NULL, SoundOutput.SoundBufferSize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

#ifdef _DEBUG
	LPVOID BaseAddress = (LPVOID)Terabytes(2);
#else
	LPVOID BaseAddress = 0;
#endif

	game_memory GameMemory = {};
	GameMemory.PermanentStorageSize = Megabytes(64);
	GameMemory.TransientStorageSize = Gigabytes(1);
	GameMemory.PermanentStorage = VirtualAlloc(BaseAddress, (SIZE_T)(GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize), MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
	GameMemory.TransientStorage = ((UINT8*)GameMemory.PermanentStorage + GameMemory.PermanentStorageSize);

	if (!GameMemory.PermanentStorage || !GameMemory.TransientStorage || !SoundOutput.Memory || !GlobalScreenBuffer.Memory)
	{
		return 1;
	}

	Win32InitDSound(hWnd, SoundOutput.SampleRate, SoundOutput.SoundBufferSize);
	Win32ClearSoundBuffer(&SoundOutput);
	GlobalSoundBuffer->Play(0, 0, DSBPLAY_LOOPING);

	GlobalRunning = true;

	LARGE_INTEGER PreviousCounter = Win32GetPerformanceCounter();
	UINT64 PreviousCycles = __rdtsc();

	game_input Input[2] = {};
	game_input* NewInput = &Input[0];
	game_input* OldInput = &Input[1];

	while (GlobalRunning)
	{
		// keyboard input

		game_controller_input* OldKeyboardController = GetController(OldInput, 0);
		game_controller_input* NewKeyboardController = GetController(NewInput, 0);
		*NewKeyboardController = {};
		NewKeyboardController->IsConnected = true;
		for (int ButtonIndex = 0; ButtonIndex < ArrayCount(NewKeyboardController->Buttons); ButtonIndex++)
		{
			NewKeyboardController->Buttons[ButtonIndex].EndedDown = OldKeyboardController->Buttons[ButtonIndex].EndedDown;
		}

		Win32ProcessMessages(hWnd, NewKeyboardController);

		// controller input

		DWORD MaxControllerCount = XUSER_MAX_COUNT;
		if (MaxControllerCount > ArrayCount(NewInput->Controllers) - 1)
		{
			MaxControllerCount = ArrayCount(NewInput->Controllers) - 1;
		}
		for (DWORD ControllerIndex = 0; ControllerIndex < MaxControllerCount; ControllerIndex++)
		{
			DWORD ControllerIndexOffset = ControllerIndex + 1;

			game_controller_input* OldControllerInput = GetController(OldInput, ControllerIndexOffset);
			game_controller_input* NewControllerInput = GetController(NewInput, ControllerIndexOffset);

			XINPUT_STATE state;
			ZeroMemory(&state, sizeof(XINPUT_STATE));

			DWORD dwResult = XInputGetState(ControllerIndex, &state);

			if (dwResult == ERROR_SUCCESS)
			{
				NewControllerInput->IsConnected = true;

				XINPUT_GAMEPAD* gamepad = &state.Gamepad;

				NewControllerInput->StickAverageX = Win32ProcessXInputStickInput(gamepad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
				NewControllerInput->StickAverageY = Win32ProcessXInputStickInput(gamepad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);

				if (NewControllerInput->StickAverageX != 0.0f || NewControllerInput->StickAverageY != 0.0f)
				{
					NewControllerInput->IsAnalog = true;
				}

				FLOAT Threshold = 0.5f;
				WORD MoveUp = NewControllerInput->StickAverageY > Threshold;
				WORD MoveDown = NewControllerInput->StickAverageY < -Threshold;
				WORD MoveLeft = NewControllerInput->StickAverageX < -Threshold;
				WORD MoveRight = NewControllerInput->StickAverageX > Threshold;

				if (gamepad->wButtons & XINPUT_GAMEPAD_DPAD_UP)
				{
					NewControllerInput->IsAnalog = true;
					MoveUp = 1;
				}

				if (gamepad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
				{
					NewControllerInput->IsAnalog = true;
					MoveDown = 1;
				}

				if (gamepad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
				{
					NewControllerInput->IsAnalog = true;
					MoveLeft = 1;
				}

				if (gamepad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
				{
					NewControllerInput->IsAnalog = true;
					MoveRight = 1;
				}

				Win32ProcessXInputDigitalButton(&NewControllerInput->MoveUp, &OldControllerInput->MoveUp, MoveUp, 1);
				Win32ProcessXInputDigitalButton(&NewControllerInput->MoveDown, &OldControllerInput->MoveDown, MoveDown, 1);
				Win32ProcessXInputDigitalButton(&NewControllerInput->MoveLeft, &OldControllerInput->MoveLeft, MoveLeft, 1);
				Win32ProcessXInputDigitalButton(&NewControllerInput->MoveRight, &OldControllerInput->MoveRight, MoveRight, 1);

				Win32ProcessXInputDigitalButton(&NewControllerInput->ActionUp, &OldControllerInput->ActionUp, gamepad->wButtons, XINPUT_GAMEPAD_Y);
				Win32ProcessXInputDigitalButton(&NewControllerInput->ActionDown, &OldControllerInput->ActionDown, gamepad->wButtons, XINPUT_GAMEPAD_A);
				Win32ProcessXInputDigitalButton(&NewControllerInput->ActionLeft, &OldControllerInput->ActionLeft, gamepad->wButtons, XINPUT_GAMEPAD_X);
				Win32ProcessXInputDigitalButton(&NewControllerInput->ActionRight, &OldControllerInput->ActionRight, gamepad->wButtons, XINPUT_GAMEPAD_B);
				Win32ProcessXInputDigitalButton(&NewControllerInput->LeftShoulder, &OldControllerInput->LeftShoulder, gamepad->wButtons, XINPUT_GAMEPAD_LEFT_SHOULDER);
				Win32ProcessXInputDigitalButton(&NewControllerInput->RightShoulder, &OldControllerInput->RightShoulder, gamepad->wButtons, XINPUT_GAMEPAD_RIGHT_SHOULDER);

				Win32ProcessXInputDigitalButton(&NewControllerInput->Start, &OldControllerInput->Start, gamepad->wButtons, XINPUT_GAMEPAD_START);
				Win32ProcessXInputDigitalButton(&NewControllerInput->Back, &OldControllerInput->Back, gamepad->wButtons, XINPUT_GAMEPAD_BACK);
			}
			else
			{
				NewControllerInput->IsConnected = false;
			}
		}

		// sound

		BOOL SoundIsValid = false;
		DWORD SoundByteOffset = 0;
		DWORD SoundByteCount = 0;
		DWORD CurrentPlayCursor;
		DWORD CurrentWriteCursor;
		if (SUCCEEDED(GlobalSoundBuffer->GetCurrentPosition(&CurrentPlayCursor, &CurrentWriteCursor)))
		{
			SoundByteOffset = (SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) % SoundOutput.SoundBufferSize;
			DWORD CurrentTargetCursor = (CurrentPlayCursor + (SoundOutput.LatencySampleCount * SoundOutput.BytesPerSample)) % SoundOutput.SoundBufferSize;
			if (SoundByteOffset > CurrentTargetCursor)
			{
				SoundByteCount = (SoundOutput.SoundBufferSize - SoundByteOffset) + CurrentTargetCursor;
			}
			else
			{
				SoundByteCount = CurrentTargetCursor - SoundByteOffset;
			}
			SoundIsValid = true;
		}

		// game

		game_sound_buffer GameSoundBuffer = {};
		GameSoundBuffer.Memory = SoundOutput.Memory;
		GameSoundBuffer.SampleRate = SoundOutput.SampleRate;
		GameSoundBuffer.SampleCount = SoundByteCount / SoundOutput.BytesPerSample;

		game_video_buffer GameVideoBuffer = {};
		GameVideoBuffer.Memory = GlobalScreenBuffer.Memory;
		GameVideoBuffer.Width = GlobalScreenBuffer.Width;
		GameVideoBuffer.Height = GlobalScreenBuffer.Height;
		GameVideoBuffer.Pitch = GlobalScreenBuffer.Pitch;

		GameUpdateAndRender(&GameMemory, &GameVideoBuffer, &GameSoundBuffer, NewInput);

		// timing

		LARGE_INTEGER CurrentCounter = Win32GetPerformanceCounter();

		FLOAT WorkFramePeriod = Win32GetSecondsElapsed(PreviousCounter, CurrentCounter);
		if (WorkFramePeriod < TargetFramePeriod)
		{
			while (WorkFramePeriod < TargetFramePeriod)
			{
				if (SleepIsGrandular)
				{
					Sleep((DWORD)(1000 * (TargetFramePeriod - WorkFramePeriod)));
				}
				WorkFramePeriod = Win32GetSecondsElapsed(PreviousCounter, Win32GetPerformanceCounter());
			}
		}
		else
		{
			// missed target frame rate
		}

		// sound

		if (SoundIsValid)
		{
			Win32FillSoundBuffer(&SoundOutput, SoundByteOffset, SoundByteCount, &GameSoundBuffer);
		}

		// video

		HDC hdc = GetDC(hWnd);
		win32_dimensions dimensions = Win32GetClientDimensions(hWnd);
		Win32DisplayBufferInWindow(hdc, dimensions.Width, dimensions.Height, &GlobalScreenBuffer);
		ReleaseDC(hWnd, hdc);

		INT32 MillisPerFrame  = (INT32)(1000 * WorkFramePeriod);
		INT32 FramesPerSecond = (INT32)(1000 / MillisPerFrame);

		WCHAR Buffer[256];
		wsprintf(Buffer, L"%d ms, %d FPS\n", MillisPerFrame, FramesPerSecond);
		OutputDebugString(Buffer);

		PreviousCounter = Win32GetPerformanceCounter();
		PreviousCycles = __rdtsc();

		game_input* Temp = NewInput;
		NewInput = OldInput;
		OldInput = Temp;

		//UINT64 CurrentCycles = __rdtsc();
		//UINT64 CyclesElapsed = CurrentCycles - PreviousCycles;
		//INT32 MCyclesPerFrame = (INT32)(CyclesElapsed / (1000 * 1000));
	}

	return 0;
}