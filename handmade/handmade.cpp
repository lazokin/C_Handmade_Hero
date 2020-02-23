#include "handmade.h"

void RunderWierdGradient(game_offscreen_buffer* Buffer, int BlueOffset, int GreenOffset)
{
	uint8_t* Row = (uint8_t*)Buffer->Memory;
	for (int y = 0; y < Buffer->Height; y++)
	{
		uint32_t* Pixel = (uint32_t*)Row;
		for (int x = 0; x < Buffer->Width; x++)
		{
			uint8_t Blue = (x + BlueOffset);
			uint8_t Green = (y + GreenOffset);
			*Pixel++ = ((Green << 8) | Blue);
		}
		Row += Buffer->Pitch;
	}
}

void GameUpdateAndRender(game_offscreen_buffer* Buffer, int BlueOffset, int GreenOffset)
{
	RunderWierdGradient(Buffer, BlueOffset, GreenOffset);
}