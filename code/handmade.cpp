#include "handmade.h"
#include "handmade_tile.cpp"
#include "handmade_random.h"

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

internal void
DrawRectangle(game_offscreen_buffer *Buffer,
              real32 RealMinX, real32 RealMinY, 
              real32 RealMaxX, real32 RealMaxY,
              real32 R, real32 G, real32 B)
{
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

    for (int Y = MinY; Y < MaxY; ++Y)
    {
        uint32 *Pixel = (uint32 *)Row;
        for (int X = MinX; X < MaxX; ++X)
        {
            *Pixel++ = Color;
        }
        Row += Buffer->Pitch;
    }
}

internal void
DrawBitmap(game_offscreen_buffer *Buffer, loaded_bitmap *Bitmap, real32 RealX, real32 RealY)
{
    int32 MinX = RoundReal32ToInt32(RealX);
    int32 MinY = RoundReal32ToInt32(RealY);
    int32 MaxX = RoundReal32ToInt32(RealX + (real32)Bitmap->Width);
    int32 MaxY = RoundReal32ToInt32(RealY + (real32)Bitmap->Height);

    if (MinX < 0)
        MinX = 0;
    if (MinY < 0)
        MinY = 0;
    if (MaxX > Buffer->Width)
        MaxX = Buffer->Width;
    if (MaxY > Buffer->Height)
        MaxY = Buffer->Height;

    // uint32 Color = 0xFF000000 |
    //                ((uint32)RoundReal32ToInt32(R * 255.0f)) << 16 |
    //                ((uint32)RoundReal32ToInt32(G * 255.0f)) << 8 |
    //                ((uint32)RoundReal32ToInt32(B * 255.0f)) << 0;

    // uint8 *Row = ((uint8 *)Buffer->Memory +
    //               MinX*Buffer->BytesPerPixel +
    //               MinY*Buffer->Pitch);

    // TODO(Kevin): SourceRow needs to be changed based on clipping.
    uint32 *SourceRow = Bitmap->Pixels + (Bitmap->Width * (Bitmap->Height - 1));
    uint8 *DestRow = (uint8 *)Buffer->Memory +
                     MinX * Buffer->BytesPerPixel +
                     MinY * Buffer->Pitch;
    for (int Y = MinY; Y < MaxY; ++Y)
    {
        uint32 *Source = SourceRow;
        uint32 *Dest = (uint32 *)DestRow;
        for (int X = MinX; X < MaxX; ++X)
        {
#if 0
            // real32 DstA = (real32)((*Dest >> 24) & 0xFF) / 255.f;
            // real32 DstR = (real32)((*Dest >> 16) & 0xFF) / 255.f;
            // real32 DstG = (real32)((*Dest >> 8) & 0xFF) / 255.f;
            // real32 DstB = (real32)((*Dest >> 0) & 0xFF) / 255.f;
            // real32 SrcA = (real32)((*Source >> 24) & 0xFF) / 255.f;
            // real32 SrcR = (real32)((*Source >> 16) & 0xFF) / 255.f;
            // real32 SrcG = (real32)((*Source >> 8) & 0xFF) / 255.f;
            // real32 SrcB = (real32)((*Source >> 0) & 0xFF) / 255.f;
            // ((uint8 *)Dest)[3] = (uint8)((SrcA * SrcA + DstA * (1.0f - SrcA)) * 255.f);
            // ((uint8 *)Dest)[2] = (uint8)((SrcR * SrcA + DstR * (1.0f - SrcA)) * 255.f);
            // ((uint8 *)Dest)[1] = (uint8)((SrcG * SrcA + DstG * (1.0f - SrcA)) * 255.f);
            // ((uint8 *)Dest)[0] = (uint8)((SrcB * SrcA + DstB * (1.0f - SrcA)) * 255.f);
            // Dest++;
            // Source++;
#else
            *Dest++ = *Source++;
#endif
        }
        SourceRow -= Bitmap->Width;
        DestRow += Buffer->Pitch;
    }
}

#pragma pack(push, 1)
struct bitmap_header
{
    uint16  FileType;
    uint32  FileSize;
    uint16  Reserved1;
    uint16  Reserved2;
    uint32  BitmapOffset;
    uint32  Size;
    int32   Width;
    int32   Height;
    uint16  Planes;
    uint16  BitsPerPixel;
    uint32  Compression;
    uint32  SizeOfBitmap;
    int32   HorzResolution;
    int32   VertResolution;
    uint32  ColorsUsed;
    uint32  ColorsImportant;
    uint32  RedMask;
    uint32  GreenMask;
    uint32  BlueMask;
};
#pragma pack(pop)

internal loaded_bitmap
DEBUGLoadBMP(thread_context *Thread, debug_platform_read_entire_file *ReadEntireFile, char *FileName)
{
    loaded_bitmap Result = {};

    // NOTE(Kevin): Casey's bitmaps are saved as AA BB GG RR, bottom up.
    // In little endian -> 0xRRGGBBAA
    // ACTUALLY, his hero bitmaps are BB GG RR AA...
    debug_read_file_result ReadResult = ReadEntireFile(Thread, FileName);

    if (ReadResult.ContentsSize != 0)
    {
        bitmap_header *Header = (bitmap_header *)ReadResult.Contents;
        uint32 *Pixels = (uint32 *)((uint8 *)ReadResult.Contents + Header->BitmapOffset);
        
        // NOTE(Kevin): If you are using this generically for some reason,
        // please remember that BMP files can go in either direction and
        // the height can be negative for top-down.
        // (Also, there can be compression, etc... DON'T think this is
        // compelete BMP loading code because it isnt!)
        uint32 *SourceDest = Pixels;
        for (int32 Y = 0; Y < Header->Height; ++Y)
        {
            for (int32 X = 0; X < Header->Width; ++X)
            {
                *SourceDest = (*SourceDest >> 8) | (*SourceDest << 24);
                ++SourceDest;
            }
        }

        Result.Pixels = Pixels;
        Result.Width = Header->Width;
        Result.Height = Header->Height;
    }

    return Result;
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    Assert((&Input->Controllers[0].Terminator - &Input->Controllers[0].Buttons[0]) == 
           ArrayCount(Input->Controllers[0].Buttons));
    Assert(sizeof(game_state) <= Memory->PermanentStorageSize);

    real32 PlayerHeight = 1.4f;
    real32 PlayerWidth = 0.75f*PlayerHeight;

    game_state *GameState = (game_state *)Memory->PermanentStorage;
    if (!Memory->IsInitialized)
    {
        // DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "structured_art.bmp");
        GameState->Backdrop =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_background.bmp");
        GameState->HeroHead =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_front_head.bmp");
        GameState->HeroCape =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_front_cape.bmp");
        GameState->HeroTorso =
            DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_hero_front_torso.bmp");

        GameState->PlayerP.AbsTileX = 1;
        GameState->PlayerP.AbsTileY = 3;
        GameState->PlayerP.OffsetX = 5.0f;
        GameState->PlayerP.OffsetY = 5.0f;
        InitializeArena(&GameState->WorldArena, Memory->PermanentStorageSize - sizeof(game_state),
                        (uint8 *)Memory->PermanentStorage + sizeof(game_state));

        GameState->World = PushStruct(&GameState->WorldArena, world);
        world *World = GameState->World;
        World->TileMap = PushStruct(&GameState->WorldArena, tile_map);
        tile_map *TileMap = World->TileMap;

        TileMap->ChunkShift = 4;
        TileMap->ChunkMask = (1 << TileMap->ChunkShift) - 1;
        TileMap->ChunkDim = (1 << TileMap->ChunkShift);

        TileMap->TileChunkCountX = 128;
        TileMap->TileChunkCountY = 128;
        TileMap->TileChunkCountZ = 2;

        TileMap->TileChunks = PushArray(&GameState->WorldArena, 
                                        TileMap->TileChunkCountX * 
                                        TileMap->TileChunkCountY * 
                                        TileMap->TileChunkCountZ, 
                                        tile_chunk);

        TileMap->TileSideInMeters = 1.4f;

        uint32 RandomNumberIndex = 0;
        uint32 TilesPerWidth = 17;
        uint32 TilesPerHeight = 9;
        uint32 ScreenX = 0;
        uint32 ScreenY = 0;

        bool32 DoorLeft = false;
        bool32 DoorRight = false;
        bool32 DoorTop = false;
        bool32 DoorBottom = false;
        bool32 DoorUp = false;
        bool32 DoorDown = false;
        uint32 AbsTileZ = 0;
        for (uint32 ScreenIndex = 0; ScreenIndex < 100; ++ScreenIndex)
        {
            // TODO(Kevin): Random number generator!
            Assert(RandomNumberIndex < ArrayCount(RandomNumberTable));

            uint32 RandomChoices = DoorUp || DoorDown ? 2 : 3;
            uint32 RandomChoice = RandomNumberTable[RandomNumberIndex++] % RandomChoices;

            bool32 CreatedZDoor = false;
            if (RandomChoice == 2)
            {
                CreatedZDoor = true;
                if (AbsTileZ == 0)
                {
                    DoorUp = true;
                }
                else
                {
                    DoorDown = true;
                }
            }
            else if (RandomChoice == 1)
            {
                DoorRight = true;
            }
            else
            {
                DoorTop = true;
            }

            for (uint32 TileY = 0; TileY < TilesPerHeight; ++TileY)
            {
                for (uint32 TileX = 0; TileX < TilesPerWidth; ++TileX)
                {
                    uint32 TileValue = 1;

                    if (TileX == 0 && (!DoorLeft || TileY != (TilesPerHeight/2)))
                    {
                        TileValue = 2;
                    }

                    if (TileX == (TilesPerWidth - 1) && (!DoorRight || TileY != (TilesPerHeight/2)))
                    {
                        TileValue = 2;
                    }

                    if (TileY == 0 && (!DoorBottom || TileX != (TilesPerWidth/2)))
                    {
                        TileValue = 2;
                    }

                    if (TileY == (TilesPerHeight - 1) && (!DoorTop || TileX != (TilesPerWidth/2)))
                    {
                        TileValue = 2;
                    }

                    if (TileX == 10 && TileY == 6)
                    {
                        if (DoorUp)
                        {
                            TileValue = 3;
                        }
                        if (DoorDown)
                        {
                            TileValue = 4;
                        }
                    }

                    uint32 AbsTileX = ScreenX * TilesPerWidth + TileX;
                    uint32 AbsTileY = ScreenY * TilesPerHeight + TileY;
                    SetTileValue(&GameState->WorldArena, World->TileMap,
                                 AbsTileX, AbsTileY, AbsTileZ, TileValue);
                }
            }

            DoorLeft = DoorRight;
            DoorBottom = DoorTop;

            if (CreatedZDoor)
            {
                DoorUp = !DoorUp;
                DoorDown = !DoorDown;
            }
            else
            {
                DoorUp = false;
                DoorDown = false;
            }

            DoorRight = false;
            DoorTop = false;

            if (RandomChoice == 2)
            {
                if(AbsTileZ == 0)
                {
                    AbsTileZ = 1;
                }
                else
                {
                    AbsTileZ = 0;
                }
            }
            else if (RandomChoice == 1)
            {
                ScreenX += 1;
            }
            else
            {
                ScreenY += 1;
            }
        }

        Memory->IsInitialized = true;
    }

    world *World = GameState->World;
    tile_map *TileMap = World->TileMap;

    int32 TileSideInPixels = 60;
    real32 PixelsPerMeter = (real32)TileSideInPixels / (real32)TileMap->TileSideInMeters;
    // real32 LowerLeftX = -(real32)TileSideInPixels/2;
    // real32 LowerLeftY = (real32)Buffer->Height;

    for (int ControllerIndex = 0;
         ControllerIndex < ArrayCount(Input->Controllers);
         ++ControllerIndex)
    {
        game_controller_input *Controller = GetController(Input, ControllerIndex);

        if (!Controller->IsConnected)
        {
            continue;
        }

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
                dPlayerY = 1.0f;
            }
            if (Controller->MoveDown.EndedDown)
            {
                dPlayerY = -1.0f;
            }
            if (Controller->MoveLeft.EndedDown)
            {
                dPlayerX = -1.0f;
            }
            if (Controller->MoveRight.EndedDown)
            {
                dPlayerX = 1.0f;
            }
            real32 PlayerSpeed = 2.0f;
            if (Controller->ActionUp.EndedDown)
            {
                PlayerSpeed = 10.0f;
            }
            dPlayerX *= PlayerSpeed;
            dPlayerY *= PlayerSpeed; 

            tile_map_position NewPlayerP = GameState->PlayerP;
            NewPlayerP.OffsetX += Input->dtForFrame*dPlayerX;
            NewPlayerP.OffsetY += Input->dtForFrame*dPlayerY;
            NewPlayerP = RecanonicalizePosition(TileMap, NewPlayerP);

            tile_map_position NewPlayerLeft = NewPlayerP;
            NewPlayerLeft.OffsetX -= 0.5f * PlayerWidth;
            NewPlayerLeft = RecanonicalizePosition(TileMap, NewPlayerLeft);

            tile_map_position NewPlayerRight = NewPlayerP;
            NewPlayerRight.OffsetX += 0.5f * PlayerWidth;
            NewPlayerRight = RecanonicalizePosition(TileMap, NewPlayerRight);

            if (IsTileMapPointEmpty(TileMap, NewPlayerP) &&
                IsTileMapPointEmpty(TileMap, NewPlayerLeft) &&
                IsTileMapPointEmpty(TileMap, NewPlayerRight))
            {
                if (!AreOnSameTile(&GameState->PlayerP, &NewPlayerP))
                {
                    uint32 NewTileValue = GetTileValue(TileMap, NewPlayerP);

                    if (NewTileValue == 3)
                    {
                        ++NewPlayerP.AbsTileZ;
                    }
                    else if (NewTileValue == 4)
                    {
                        --NewPlayerP.AbsTileZ;
                    }
                }
                GameState->PlayerP = NewPlayerP;
            }
        }
    }


#if 0
    DrawRectangle(Buffer, 0.f, 0.f, (real32)Buffer->Width, (real32)Buffer->Height,
                  1.f, 0.f, 1.f);
#endif
    DrawBitmap(Buffer, &GameState->Backdrop, 0.f, 0.f);

    real32 ScreenCenterX = 0.5f*(real32)Buffer->Width;
    real32 ScreenCenterY = 0.5f*(real32)Buffer->Height;

    for (int32 RelRow = -10; 
         RelRow < 10;
         ++RelRow)
    {
        for (int32 RelColumn = -20; 
             RelColumn < 20;
             ++RelColumn)
        {
            uint32 Column = GameState->PlayerP.AbsTileX + RelColumn;
            uint32 Row = GameState->PlayerP.AbsTileY + RelRow;
            uint32 Height = GameState->PlayerP.AbsTileZ;
            uint32 TileID = GetTileValue(TileMap, Column, Row, Height);
         
            if (TileID > 1)
            {
                real32 Gray = 0.5f;

                if (TileID == 2)
                {
                    Gray = 1.0f;
                }

                if (TileID > 2)
                {
                    Gray = 0.25f;
                }

                if (Column == GameState->PlayerP.AbsTileX && Row == GameState->PlayerP.AbsTileY)
                {
                    Gray = 0.0f;
                }

                real32 CenX = ScreenCenterX - PixelsPerMeter * GameState->PlayerP.OffsetX + ((real32)RelColumn) * TileSideInPixels;
                real32 CenY = ScreenCenterY + PixelsPerMeter * GameState->PlayerP.OffsetY - ((real32)RelRow) * TileSideInPixels;
                real32 MinX = CenX - 0.5f*TileSideInPixels;
                real32 MinY = CenY - 0.5f*TileSideInPixels;
                real32 MaxX = CenX + 0.5f*TileSideInPixels;
                real32 MaxY = CenY + 0.5f*TileSideInPixels;
                DrawRectangle(Buffer, MinX, MinY, MaxX, MaxY, Gray, Gray, Gray);
            }
        }
    }

    real32 PlayerR = 1.0f;
    real32 PlayerG = 1.0f;
    real32 PlayerB = 0.0f;
    real32 PlayerLeft = ScreenCenterX - 0.5f*PixelsPerMeter*PlayerWidth;
    real32 PlayerTop = ScreenCenterY - PixelsPerMeter*PlayerHeight;
#if 1
    DrawBitmap(Buffer, &GameState->HeroHead, PlayerLeft, PlayerTop);
#else
    DrawRectangle(Buffer,
                  PlayerLeft, PlayerTop, 
                  PlayerLeft + PixelsPerMeter * PlayerWidth, 
                  PlayerTop + PixelsPerMeter * PlayerHeight, 
                  PlayerR, PlayerG, PlayerB);
#endif
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

