#include "handmade.h"

internal void
GameOutputSound(game_sound_output_buffer *SoundBuffer)
{
	local_persist real32 tSine = 0;
	int16 ToneVolume = 3000;
	int ToneHz = 256;
	int WavePeriod = SoundBuffer->SamplesPerSecond / ToneHz;

	int16 *SampleOut = SoundBuffer->Samples;
	for (DWORD SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; SampleIndex++)
	{
		real32 SineValue = sinf(tSine);
		int16 SampleValue = (int16)(SineValue * ToneVolume);
		*SampleOut++ = SampleValue;
		*SampleOut++ = SampleValue;
		
		tSine += 2 * (real32)Pi32 * 1 / (real32)WavePeriod;
	}
}


internal void
DrawWeirdGradient(game_offscreen_buffer *Buffer, int XOffset, int YOffset)
{
	uint8 *Row = (uint8 *)Buffer->Memory;
	for (int Y = 0; Y < Buffer->Height; ++Y)
	{
		uint32 *Pixel = (uint32 *)Row;
		for (int X = 0; X < Buffer->Width; ++X)
		{
			uint8 B = 0;
			uint8 G = X + XOffset;
			uint8 R = Y + YOffset;
			*Pixel++ = (R | (G << 8) | (B << 16));
		}
		Row += Buffer->Pitch;
	}
}


internal void
GameUpdateAndRender(game_offscreen_buffer *Buffer, game_sound_output_buffer * SoundBuffer)
{
	GameOutputSound(SoundBuffer);
	int xOffset = 0;
	int yOffset = 0;
	DrawWeirdGradient(Buffer, xOffset, yOffset);
}