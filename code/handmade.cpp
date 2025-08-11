#include "handmade.h"

internal void
GameOutputSound(game_state *GameState, game_sound_output_buffer *SoundBuffer, int ToneHz)
{
    int16 ToneVolume = 4000;
    int WavePeriod = SoundBuffer->SamplesPerSecond / ToneHz; // samples per cycle

    int16 *SampleOut = (int16 *)SoundBuffer->Samples;
    for (int SampleIndex = 0;
        SampleIndex < SoundBuffer->SampleCount;
        ++SampleIndex)
    {
#if 0
        real32 SineValue = sinf(GameState->tSine);
        int16 SampleValue = (int16)(SineValue * ToneVolume);
#else
        int16 SampleValue = 0;
#endif
        *SampleOut++ = SampleValue;
        *SampleOut++ = SampleValue;

        GameState->tSine += 2.0f * Pi32 * 1.0f/(real32)WavePeriod;
        if (GameState->tSine > 2.0f * Pi32)
        {
            GameState->tSine -= 2.0f * Pi32;
        }
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

internal void
RenderPlayer(game_offscreen_buffer *Buffer, int PlayerX, int PlayerY)
{
    uint8 *EndOfBuffer = (uint8 *)Buffer->Memory + Buffer->Pitch*Buffer->Height;

    uint32 Color = 0xFFFFFFFF;
    int Top = PlayerY;
    int Bottom = PlayerY+10;
    for(int X = PlayerX; X < PlayerX+10; ++X)
    {
        uint8 *Pixel = ((uint8 *)Buffer->Memory +
                        X*Buffer->BytesPerPixel +
                        Top*Buffer->Pitch);
        for(int Y = Top; Y < Bottom; ++Y)
        {
            if((Pixel >= Buffer->Memory) &&
               ((Pixel + 4) <= EndOfBuffer))
            {
                *(uint32 *)Pixel = Color;
            }

            Pixel += Buffer->Pitch;
        }
    }
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    Assert((&Input->Controllers[0].Terminator - &Input->Controllers[0].Buttons[0]) == 
           ArrayCount(Input->Controllers[0].Buttons));
    Assert(sizeof(game_state) <= Memory->PermanentStorageSize);

    game_state *GameState = (game_state *)Memory->PermanentStorage;
    if (!Memory->IsInitialized)
    {
#if 0
        char *Filename = __FILE__;
        debug_read_file_result File = Memory->DEBUGPlatformReadEntireFile(Filename);
        if (File.Contents)
        {
            Memory->DEBUGPlatformWriteEntireFile("test.out", File.ContentsSize, File.Contents);
            Memory->DEBUGPlatformFreeFileMemory(File.Contents);
        }
#endif
        GameState->ToneHz = 256;
        GameState->tSine = 0.f;

        GameState->PlayerX = 100;
        GameState->PlayerY = 100;

        // TODO(Kevin): This may be more appropriate to do in the platform layer.
        Memory->IsInitialized = true;
    }

    GameState->ToneHz = 256;

    for (int ControllerIndex = 0;
         ControllerIndex < ArrayCount(Input->Controllers);
         ++ControllerIndex)
    {
        game_controller_input *Controller = GetController(Input, ControllerIndex);
        if (Controller->IsAnalog)
        {
            // NOTE(Kevin): Use analog movement tuning
            GameState->BlueOffset += (int)(4.0f*(Controller->StickAverageX));
            GameState->ToneHz = 256 + (int)(128.f*(Controller->StickAverageY));
        }
        else
        {
            // NOTE(Kevin): Use digital movement tuning
            if (Controller->MoveLeft.EndedDown)
            {
                GameState->BlueOffset -= 1;
            }
            if (Controller->MoveRight.EndedDown)
            {
                GameState->BlueOffset += 1;
            }
        }
    
        if (Controller->ActionDown.EndedDown)
        {
            GameState->GreenOffset += 1;
        }

        if (Controller->LeftShoulder.EndedDown)
        {
            GameState->ToneHz = 256 - 128;
        }

        if (Controller->RightShoulder.EndedDown)
        {
            GameState->ToneHz = 256 + 128;
        }

        // GameState->PlayerX += (int)(4.0f*(Controller->StickAverageX));
        GameState->PlayerY -= (int)(4.0f*(Controller->StickAverageY));
        if (Controller->MoveUp.EndedDown)
            GameState->PlayerY -= 4;
        if (Controller->MoveDown.EndedDown)
            GameState->PlayerY += 4;
        if (Controller->MoveLeft.EndedDown)
            GameState->PlayerX -= 4;
        if (Controller->MoveRight.EndedDown)
            GameState->PlayerX += 4;
        if (Controller->ActionDown.EndedDown)
        {
            GameState->tJump = 4.0f;
        }
        if (GameState->tJump > 0.f)
        {
            GameState->PlayerY += (int)(5.0f*sinf(0.5f*Pi32*GameState->tJump));
        }
        GameState->tJump -= 0.033f;
        if (GameState->tJump < 0.f)
            GameState->tJump = 0.f;
    }

    RenderWeirdGradient(Buffer, GameState->BlueOffset, GameState->GreenOffset);
    RenderPlayer(Buffer, GameState->PlayerX, GameState->PlayerY);
}


extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
    game_state *GameState = (game_state *)Memory->PermanentStorage;
    GameOutputSound(GameState, SoundBuffer, GameState->ToneHz);
}



