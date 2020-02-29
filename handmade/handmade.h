#pragma once

#include <stdint.h>
#include <math.h>

constexpr auto PI32 = 3.14159265359f;

#if _DEBUG
#define Assert(Expression) if(!(Expression)) {*(int*)0 = 0;}
#else
#define Assert(Expression)
#endif

#define Kilobytes(Value) (1024LL * Value)
#define Megabytes(Value) (1024LL * Kilobytes(Value))
#define Gigabytes(Value) (1024LL * Megabytes(Value))
#define Terabytes(Value) (1024LL * Gigabytes(Value))

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

uint32_t SafeTruncate64To32(uint64_t value);

#if _DEBUG
struct debug_real_file_result
{
	uint32_t Size;
	void* Bytes;
};
debug_real_file_result DEBUGPlatformReadEntireFile(wchar_t* Filename);
bool DEBUGPlatformWriteEntireFile(wchar_t* Filename, uint32_t FileSize, void* Bytes);
void DEBUGFreeFileFromMemory(void* Memory);
#endif

struct game_state
{
	int32_t BlueOffset;
	int32_t GreenOffset;
	int32_t Tone;
};

struct game_memory
{
	bool IsInitialised;
	uint64_t PermanentStorageSize;
	void* PermanentStorage;
	uint64_t TransientStorageSize;
	void* TransientStorage;
};


struct game_sound_buffer
{
	void* Memory;
	int32_t SampleRate;
	int32_t SampleCount;
};

struct game_video_buffer
{
	void* Memory;
	int32_t Width;
	int32_t Height;
	int32_t Pitch;
};

struct game_button_state
{
	int32_t HalfTransitionCount;
	bool EndedDown;
};

struct game_controller_input
{
	bool IsConnected;
	bool IsAnalog;
	float StickAverageX;
	float StickAverageY;

	union {
		game_button_state Buttons[12];
		struct {
			game_button_state MoveUp;
			game_button_state MoveDown;
			game_button_state MoveLeft;
			game_button_state MoveRight;
			game_button_state ActionUp;
			game_button_state ActionDown;
			game_button_state ActionLeft;
			game_button_state ActionRight;
			game_button_state LeftShoulder;
			game_button_state RightShoulder;
			game_button_state Start;
			game_button_state Back;

			game_button_state Sentinal;
		};
	};
};

struct game_input
{
	game_controller_input Controllers[5];
};

inline game_controller_input* GetController(game_input* Input, uint32_t ControllerIndex)
{
	Assert(ControllerIndex < ArrayCount(Input->Controllers));
	return &Input->Controllers[ControllerIndex];
}

void GameUpdateAndRender(game_memory* Memory, game_video_buffer* Video, game_sound_buffer* Sound, game_input* Input);