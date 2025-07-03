#pragma once

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

// the game is the services to the operating system level to produce the graphics
// and sound necessary to play the game

// write the os code be tangled and weird and do all the os specific things it needs to do
// and all the game needs to do is respond to a few very specific requests e.g. give me the
// things you want to draw, give me the sound you want to play, and heres the user's input

// basically, isolate a few specific locations in the code where the platform layer wants services
// from the game, and isolate a few specific locations in the game where the game wants services
// from the platform layer.

/*
    TODO(Kevin): Services that the platform layer provides to the game.
*/

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
    int SamplesPerSecond;
    int SampleCount;
    int16 *Samples;
};

struct game_button_state
{
    int HalfTransitionCount;
    bool32 EndedDown;
};

struct game_controller_input
{
    bool32 IsAnalog;

    real32 StartX;
    real32 StartY;

    real32 MinX;
    real32 MinY;

    real32 MaxX;
    real32 MaxY;

    real32 EndX;
    real32 EndY;

    union
    {
        game_button_state Buttons[6];
        struct
        {
            game_button_state Up;
            game_button_state Down;
            game_button_state Left;
            game_button_state Right;
            game_button_state LeftShoulder;
            game_button_state RightShoulder;
        };
    };
};

struct game_input
{
    game_controller_input Controllers[4];
};

void GameUpdateAndRender(game_input *Input, game_offscreen_buffer *Buffer, 
                         game_sound_output_buffer *SoundBuffer);
