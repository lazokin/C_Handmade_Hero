#include "handmade.h"

uint32_t SafeTruncate64To32(uint64_t value)
{
	Assert(value <= 0xFFFFFF);
	return (uint32_t)value;
}

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
			uint8_t Blue = (uint8_t)(x + BlueOffset);
			uint8_t Green = (uint8_t)(y + GreenOffset);
			*Pixel++ = ((Green << 8) | Blue);
		}
		Row += Buffer->Pitch;
	}
}

void GameUpdateAndRender(game_memory* Memory, game_video_buffer* Video, game_sound_buffer* Sound, game_input* Input)
{
	Assert(&Input->Controllers[0].Sentinal - &Input->Controllers[0].Buttons[0] == ArrayCount(Input->Controllers[0].Buttons));
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

	for (int ControllerIndex = 0; ControllerIndex < ArrayCount(Input->Controllers); ControllerIndex++)
	{
		game_controller_input* Controller = &Input->Controllers[ControllerIndex];

		if (Controller->MoveUp.EndedDown)
		{
			GameState->GreenOffset--;
		}

		if (Controller->MoveDown.EndedDown)
		{
			GameState->GreenOffset++;
		}

		if (Controller->MoveLeft.EndedDown)
		{
			GameState->BlueOffset--;
		}

		if (Controller->MoveRight.EndedDown)
		{
			GameState->BlueOffset++;
		}

		if (Controller->IsAnalog)
		{
			if (Controller->StickAverageX > 0.5f)
			{
				GameState->Tone++;
			}
			if (Controller->StickAverageX < -0.5f)
			{
				GameState->Tone--;
			}
			if (Controller->StickAverageY > 0.5f)
			{
				GameState->Tone++;
			}
			if (Controller->StickAverageY < -0.5f)
			{
				GameState->Tone--;
			}
		}

	}

	UpdateSound(Sound, GameState->Tone);
	UpdateVideo(Video, GameState->BlueOffset, GameState->GreenOffset);
}