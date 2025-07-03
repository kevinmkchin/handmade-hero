#include "handmade.h"

internal void
GameOutputSound(game_sound_output_buffer *SoundBuffer, int ToneHz)
{
    local_persist real32 tSine;
    int16 ToneVolume = 4000;
    int WavePeriod = SoundBuffer->SamplesPerSecond / ToneHz; // samples per cycle

    int16 *SampleOut = SoundBuffer->Samples;
    for (int SampleIndex = 0;
        SampleIndex < SoundBuffer->SampleCount;
        ++SampleIndex)
    {
        real32 SineValue = sinf(tSine);
        int16 SampleValue = (int16)(SineValue * ToneVolume);
        *SampleOut++ = SampleValue;
        *SampleOut++ = SampleValue;

        tSine += 2.0f * Pi32 * 1.f / (real32)WavePeriod;
    }
}

internal void
RenderWeirdGradient(game_offscreen_buffer *Buffer, int XOffset, int YOffset)
{
    uint8 *Row = (uint8 *)Buffer->Memory;
    for (int Y = 0; Y < Buffer->Height; ++Y)
    {
        uint32 *Pixel = (uint32 *)Row;
        for (int X = 0; X < Buffer->Width; ++X)
        {
            /*
                In Windows, RGBx is stored as BGRx
                Pixel in memory: BB GG RR xx
                Which, following LITTLE ENDIAN, becomes:
                0x xx RR GG BB
            */
            uint8 Green = (uint8)(Y + YOffset);
            uint8 Blue = (uint8)(X + XOffset);
            uint8 Red = 0;

            *Pixel++ = (0 << 24)
                | (Red << 16)
                | (Green << 8)
                | (Blue << 0);
        }

        Row += Buffer->Pitch;
    }
}


void
GameUpdateAndRender(game_input *Input, game_offscreen_buffer *Buffer, 
                    game_sound_output_buffer *SoundBuffer)
{
    // TODO(Kevin): Allow sample offsets here for more robust platform options
    local_persist int ToneHz = 256;
    local_persist int BlueOffset = 0;
    local_persist int GreenOffset = 0;

    game_controller_input *Input0 = &Input->Controllers[0];
    if (Input0->IsAnalog)
    {
        // NOTE(Kevin): Use analog movement tuning
        ToneHz = 256 + (int)(128.f*(Input0->EndY));
        BlueOffset += (int)(4.0f*(Input0->EndX));
    }
    else
    {
        // NOTE(Kevin): Use digital movement tuning
    }

    if (Input0->Down.EndedDown)
    {
        GreenOffset += 1;
    }

    GameOutputSound(SoundBuffer, ToneHz);
    RenderWeirdGradient(Buffer, BlueOffset, GreenOffset);
}





