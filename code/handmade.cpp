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
#if 0
        GameState->tSine += 2.0f * Pi32 * 1.0f/(real32)WavePeriod;
        if (GameState->tSine > 2.0f * Pi32)
        {
            GameState->tSine -= 2.0f * Pi32;
        }
#endif
    }
}

internal int32
RoundReal32ToInt32(real32 Real32)
{
    // TODO(Kevin): Intrinsic?
    if (Real32 > 0.f)
        return (int32)(Real32 + 0.5f);
    else if (Real32 < 0.f)
        return (int32)(Real32 - 0.5f);
    else
        return 0;
}

internal void
DrawRectangle(game_offscreen_buffer *Buffer,
              real32 RealMinX, real32 RealMinY, 
              real32 RealMaxX, real32 RealMaxY,
              real32 R, real32 G, real32 B)
{
    // TODO(Kevin): Floating point color tomorrow
    // Rounds parameters to integers and fills up to but not including rnd(MaxX), rnd(MaxY)

    int32 MinX = RoundReal32ToInt32(RealMinX);
    int32 MinY = RoundReal32ToInt32(RealMinY);
    int32 MaxX = RoundReal32ToInt32(RealMaxX);
    int32 MaxY = RoundReal32ToInt32(RealMaxY);

    if (MinX < 0)
        MinX = 0;
    if (MinY < 0)
        MinY = 0;
    if (MaxX > Buffer->Width)
        MaxX = Buffer->Width;
    if (MaxY > Buffer->Height)
        MaxY = Buffer->Height;

    uint32 Color = 0xFF000000 |
                   ((uint32)RoundReal32ToInt32(R * 255.0f)) << 16 |
                   ((uint32)RoundReal32ToInt32(G * 255.0f)) << 8 |
                   ((uint32)RoundReal32ToInt32(B * 255.0f)) << 0;

    uint8 *Row = ((uint8 *)Buffer->Memory +
                  MinX*Buffer->BytesPerPixel +
                  MinY*Buffer->Pitch);

    for(int Y = MinY; Y < MaxY; ++Y)
    {
        uint32 *Pixel = (uint32 *)Row;
        for(int X = MinX; X < MaxX; ++X)
        {
            *Pixel++ = Color;
        }
        Row += Buffer->Pitch;
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
        GameState->PlayerX = 100.f;
        GameState->PlayerY = 100.f;

        Memory->IsInitialized = true;
    }

    for (int ControllerIndex = 0;
         ControllerIndex < ArrayCount(Input->Controllers);
         ++ControllerIndex)
    {
        game_controller_input *Controller = GetController(Input, ControllerIndex);
        if (Controller->IsAnalog)
        {
            // NOTE(Kevin): Use analog movement tuning
        }
        else
        {
            // NOTE(Kevin): Use digital movement tuning
            real32 dPlayerX = 0.0f;
            real32 dPlayerY = 0.0f;

            if (Controller->MoveUp.EndedDown)
            {
                dPlayerY = -1.0f;
            }
            if (Controller->MoveDown.EndedDown)
            {
                dPlayerY = 1.0f;
            }
            if (Controller->MoveLeft.EndedDown)
            {
                dPlayerX = -1.0f;
            }
            if (Controller->MoveRight.EndedDown)
            {
                dPlayerX = 1.0f;
            }
            dPlayerX *= 64.0f;
            dPlayerY *= 64.0f; 

            GameState->PlayerX += Input->dtForFrame*dPlayerX;
            GameState->PlayerY += Input->dtForFrame*dPlayerY;
        }
    }

    uint32 TileMap[9][17] =
    {
        { 1, 1, 1, 1,  1, 1, 1, 1,  0,  1, 1, 1, 1,  1, 1, 1, 1 },
        { 1, 1, 0, 0,  0, 1, 0, 0,  0,  0, 0, 0, 0,  1, 0, 0, 1 },
        { 1, 1, 0, 0,  0, 0, 0, 0,  0,  0, 0, 0, 0,  0, 1, 0, 1 },
        { 1, 0, 0, 0,  0, 0, 0, 0,  1,  0, 0, 0, 0,  0, 0, 0, 1 },
        { 0, 0, 1, 0,  0, 1, 0, 0,  1,  0, 0, 0, 0,  0, 1, 0, 0 },
        { 1, 1, 1, 0,  0, 1, 0, 0,  1,  0, 0, 0, 0,  1, 0, 0, 1 },
        { 1, 0, 0, 0,  0, 1, 0, 0,  0,  0, 0, 0, 1,  0, 0, 0, 1 },
        { 1, 1, 1, 1,  1, 0, 0, 0,  0,  0, 0, 0, 0,  1, 0, 0, 1 },
        { 1, 1, 1, 1,  1, 1, 1, 1,  0,  1, 1, 1, 1,  1, 1, 1, 1 },
    };

    DrawRectangle(Buffer, 0.f, 0.f, (real32)Buffer->Width, (real32)Buffer->Height,
                  1.f, 0.f, 1.f);

    real32 UpperLeftX = -30;
    real32 UpperLeftY = 0;
    real32 TileWidth = 60;
    real32 TileHeight = 60;
    for (int Row = 0; Row < 9; ++Row)
    {
        for (int Column = 0; Column < 17; ++Column)
        {
            uint32 TileID = TileMap[Row][Column];
            real32 Gray = 0.5f;
            if (TileID == 1)
            {
                Gray = 1.0f;
            }

            real32 MinX = UpperLeftX + ((real32)Column) * TileWidth;
            real32 MinY = UpperLeftY + ((real32)Row) * TileHeight;
            real32 MaxX = MinX + TileWidth;
            real32 MaxY = MinY + TileHeight;
            DrawRectangle(Buffer, MinX, MinY, MaxX, MaxY, Gray, Gray, Gray);
        }
    }

    real32 PlayerR = 1.0f;
    real32 PlayerG = 1.0f;
    real32 PlayerB = 0.0f;
    real32 PlayerWidth = TileWidth * 0.75f;
    real32 PlayerHeight = TileHeight;
    real32 PlayerLeft = GameState->PlayerX - (PlayerWidth / 2.f);
    real32 PlayerTop = GameState->PlayerY - PlayerHeight;
    DrawRectangle(Buffer, PlayerLeft, PlayerTop, PlayerLeft + PlayerWidth, PlayerTop + PlayerHeight, 
                  PlayerR, PlayerG, PlayerB);
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
    game_state *GameState = (game_state *)Memory->PermanentStorage;
    GameOutputSound(GameState, SoundBuffer, 256);
}

/*
internal void
RenderWeirdGradient(game_offscreen_buffer *Buffer, int XOffset, int YOffset)
{
    uint8 *Row = (uint8 *)Buffer->Memory;
    for (int Y = 0; Y < Buffer->Height; ++Y)
    {
        uint32 *Pixel = (uint32 *)Row;
        for (int X = 0; X < Buffer->Width; ++X)
        {
            // In Windows, RGBx is stored as BGRx
            // Pixel in memory: BB GG RR xx
            // Which, following LITTLE ENDIAN, becomes:
            // 0x xx RR GG BB

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
*/

