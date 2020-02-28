#include "handmade.h"

void UpdateSound(game_sound_buffer* Buffer, int32_t Tone)
{
	static float tSine;
	int16_t ToneVolume = 3000;
	int32_t WavePeriod = Buffer->SampleRate / Tone;

	int16_t* SampleOut = (int16_t*)Buffer->Memory;
	for (int SampleIndex = 0; SampleIndex < Buffer->SampleCount; SampleIndex++)
	{
		int16_t SampleValue = (int16_t)(ToneVolume * sinf(tSine));
		*SampleOut++ = SampleValue;
		*SampleOut++ = SampleValue;
		tSine += 2.0f * PI32 * 1.0f / (float)WavePeriod;
	}
}

void UpdateVideo(game_video_buffer* Buffer, int32_t BlueOffset, int32_t GreenOffset)
{
	uint8_t* Row = (uint8_t*)Buffer->Memory;
	for (int32_t y = 0; y < Buffer->Height; y++)
	{
		uint32_t* Pixel = (uint32_t*)Row;
		for (int32_t x = 0; x < Buffer->Width; x++)
		{
			uint8_t Blue = (x + BlueOffset);
			uint8_t Green = (y + GreenOffset);
			*Pixel++ = ((Green << 8) | Blue);
		}
		Row += Buffer->Pitch;
	}
}

void GameUpdateAndRender(game_memory* Memory, game_video_buffer* Video, game_sound_buffer* Sound, game_input* Input)
{
	Assert(sizeof(game_state) <= Memory->PermanentStorageSize);

	game_state* GameState = (game_state*)Memory->PermanentStorage;
	if (!Memory->IsInitialised)
	{
		wchar_t SrcFileName[] = L"data\\file_src.txt";
		wchar_t DstFileName[] = L"data\\file_dst.txt";

		debug_real_file_result File = DEBUGPlatformReadEntireFile(SrcFileName);
		
		if (File.Bytes)
		{
			DEBUGPlatformWriteEntireFile(DstFileName, File.Size, File.Bytes);
			DEBUGFreeFileFromMemory(File.Bytes);
		}


		GameState->Tone = 256;
		Memory->IsInitialised = true;
	}

	game_controller_input* Input0 = &Input->Controllers[0];

	if (Input0->IsAnalog)
	{
		GameState->BlueOffset += (int32_t)(4.0f * Input0->EndX);
		GameState->GreenOffset += (int32_t)(4.0f * Input0->EndY);
	}
	else
	{

	}

	if (Input0->Up.EndedDown)
	{
		GameState->Tone++;
	}

	if (Input0->Down.EndedDown)
	{
		GameState->Tone--;
	}

	UpdateSound(Sound, GameState->Tone);
	UpdateVideo(Video, GameState->BlueOffset, GameState->GreenOffset);
}