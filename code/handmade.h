#pragma once

/*
    NOTE(Kevin):

    HANDMADE_INTERNAL:
        0 - Build for public release
        1 - Build for developer only

    HANDMADE_SLOW:
        0 - No slow code allowed!
        1 - Slow code welcome.
*/

#if HANDMADE_SLOW
// TODO(Kevin): Complete assertion macro
#define Assert(Expression) if (!(Expression)) { *(int *)0=0; }
#else
#define Assert(Expression)
#endif

#define Kilobytes(Value) ((Value)*1024LL)
#define Megabytes(Value) (Kilobytes(Value)*1024LL)
#define Gigabytes(Value) (Megabytes(Value)*1024LL)
#define Terabytes(Value) (Gigabytes(Value)*1024LL)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

inline uint32
SafeTruncateUInt64(uint64 Value)
{
    // TODO(Kevin): Defines for maximum values UInt32Max
    Assert(Value <= 0xFFFFFFFF);
    uint32 Result = (uint32)Value;
    return Result;
}

// the game is the services to the operating system level to produce the graphics
// and sound necessary to play the game

// write the os code be tangled and weird and do all the os specific things it needs to do
// and all the game needs to do is respond to a few very specific requests e.g. give me the
// things you want to draw, give me the sound you want to play, and heres the user's input

// basically, isolate a few specific locations in the code where the platform layer wants services
// from the game, and isolate a few specific locations in the game where the game wants services
// from the platform layer.

/*
    NOTE(Kevin): Services that the platform layer provides to the game.
*/
#if HANDMADE_INTERNAL
/*  IMPORTANT(Kevin):
    These are NOT for doing anything in the shipping game - they are blocking
    and the write does not protect against lost data.
*/
struct debug_read_file_result
{
    uint32 ContentsSize;
    void *Contents;
};
internal debug_read_file_result DEBUGPlatformReadEntireFile(char *Filename);
internal void DEBUGPlatformFreeFileMemory(void *BitmapMemory);
internal bool32 DEBUGPlatformWriteEntireFile(char *Filename, uint32 MemorySize, void *Memory);
#endif

/*
    NOTE(Kevin): Services that the game provides to the platform layer.
    (this may expand in the future - sound on separate thread, etc.)
*/
// FOUR THINGS - timing, controller/keyboard input, bitmap buffer to use, sound buffer to use
// TODO(Kevin): In the future, rendering specifically will become a three-tiered abstraction!!!
struct game_offscreen_buffer
{
    void *Memory;
    int Width;
    int Height;
    int Pitch;
};

struct game_sound_output_buffer
{
    // 2 channel 16-bit int PCM 
    int SamplesPerSecond;
    int SampleCount;
    void *Samples;
};

struct game_button_state
{
    int HalfTransitionCount;
    bool32 EndedDown;
};

struct game_controller_input
{
    bool32 IsConnected;
    bool32 IsAnalog;
    real32 StickAverageX;
    real32 StickAverageY;

    union
    {
        game_button_state Buttons[12];
        struct
        {
            game_button_state MoveUp;
            game_button_state MoveDown;
            game_button_state MoveLeft;
            game_button_state MoveRight;

            game_button_state ActionUp; // double tap stick in direction
            game_button_state ActionDown;
            game_button_state ActionLeft;
            game_button_state ActionRight;

            game_button_state LeftShoulder;
            game_button_state RightShoulder;

            game_button_state Start;
            game_button_state Back;

            // NOTE(Kevin): All buttons must be added above this line
            game_button_state Terminator; 
        };
    };
};

struct game_input
{
    // TODO(Kevin): Insert clock values here cuz dT technically input.

    game_controller_input Controllers[5];
};
inline game_controller_input *GetController(game_input *Input, int unsigned ControllerIndex)
{
    Assert(ControllerIndex < ArrayCount(Input->Controllers));
    game_controller_input *Result = &Input->Controllers[ControllerIndex];
    return Result;
}

struct game_memory
{
    bool32 IsInitialized;

    uint64 PermanentStorageSize;
    void *PermanentStorage; // NOTE(Kevin): REQUIRED to be cleared to zero at startup

    uint64 TransientStorageSize;
    void *TransientStorage; // NOTE(Kevin): REQUIRED to be cleared to zero at startup
};

internal void GameUpdateAndRender(game_memory *Memory, 
                                  game_input *Input, game_offscreen_buffer *Buffer, 
                                  game_sound_output_buffer *SoundBuffer);

//
// platform layer does not need to know about these:
//

struct game_state
{
    int ToneHz;
    int GreenOffset;
    int BlueOffset;
};

