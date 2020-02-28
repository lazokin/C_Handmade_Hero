#pragma once

#include <Windows.h>

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
	LPVOID Memory;
	INT SampleRate;
	INT BytesPerSample;
	INT SoundBufferSize;
	UINT32 RunningSampleIndex;
	FLOAT tSine;
	INT LatencySampleCount;
};