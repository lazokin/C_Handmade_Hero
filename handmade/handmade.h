#pragma once

#include <stdint.h>
#include <math.h>

constexpr auto PI32 = 3.14159265359f;

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

void GameUpdateAndRender(game_video_buffer* VideoBuffer, int32_t BlueOffset, int32_t GreenOffset, game_sound_buffer* SoundBuffer, int32_t Tone);