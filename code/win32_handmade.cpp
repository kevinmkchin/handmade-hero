/*
Platform Layer TODO

- Saved game locations
- Getting a handle to our own executable file
- Asset loading path
- Threading (launch a thread)
- Raw Input (support for multiple keyboards)
- Sleep/timeBeginPeriod
- ClipCursor (multimonitor support)
- Fullscreen support
- WM_SETCURSOR (control cursor visibility)
- WM_ACTIVATEAPP (for when we are not the active application)
- Hardware acceleration (OpenGL or Direct3D)
- GetKeyboardLayout (for French keyboards, international WASD support)

*/

#include <stdint.h>
#include <math.h>

#define internal static             // static functions are internal to the translation unit
#define local_persist static        // local static variable
#define global_variable static      // global static variable

#define Pi32 3.14159265359f

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;
typedef int32 bool32;
typedef float real32;
typedef double real64;

#include <windows.h>
#include <stdio.h>
#include <malloc.h>
#include <xinput.h>
#include <dsound.h>

#include "handmade.h"
#include "handmade.cpp"
#include "win32_handmade.h"
 
global_variable bool GlobalRunning;
global_variable win32_offscreen_buffer GlobalBackbuffer;
global_variable IDirectSoundBuffer *GlobalSecondaryBuffer;
global_variable int64 GlobalPerfCountFrequency; 

// NOTE(Kevin): This is our support for XInputGetState
// define function prototype once (so I can change it once here and changes everywhere)
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
// define a type of that function protoype so I can use it later on as a pointer
typedef X_INPUT_GET_STATE(x_input_get_state);
// define a stub with it as well
X_INPUT_GET_STATE(XInputGetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state); 
X_INPUT_SET_STATE(XInputSetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPGUID lpGuid, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter);
typedef DIRECT_SOUND_CREATE(direct_sound_create);

internal debug_read_file_result
DEBUGPlatformReadEntireFile(char *Filename)
{
    debug_read_file_result Result = {};

    HANDLE FileHandle = CreateFileA(Filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (FileHandle != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER FileSize;
        if(GetFileSizeEx(FileHandle, &FileSize))
        {
            uint32 FileSize32 = SafeTruncateUInt64(FileSize.QuadPart);
            Result.Contents = VirtualAlloc(0, FileSize32, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
            if (Result.Contents)
            {
                DWORD BytesRead;
                if (ReadFile(FileHandle, Result.Contents, FileSize32, &BytesRead, 0) &&
                    FileSize32 == BytesRead)
                {
                    // NOTE(Kevin): File read successfully
                    Result.ContentsSize = FileSize32;
                }
                else
                {
                    // TODO(Kevin): Logging
                    DEBUGPlatformFreeFileMemory(Result.Contents);
                    Result.Contents = 0;
                }
            }
            else
            {
                // TODO(Kevin): Logging
            }
        }

        CloseHandle(FileHandle);
    }

    return Result;
}

internal void 
DEBUGPlatformFreeFileMemory(void *BitmapMemory)
{
    if (BitmapMemory)
    {
        VirtualFree(BitmapMemory, 0, MEM_RELEASE);
    }
}

internal bool32 
DEBUGPlatformWriteEntireFile(char *Filename, uint32 MemorySize, void *Memory)
{
    bool Result = false;

    HANDLE FileHandle = CreateFileA(Filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    if (FileHandle != INVALID_HANDLE_VALUE)
    {
        DWORD BytesWritten;
        if (WriteFile(FileHandle, Memory, MemorySize, &BytesWritten, 0))
        {
            // NOTE(Kevin): File write successfully
            Result = (BytesWritten == MemorySize);
        }
        else
        {
            // TODO(Kevin): Logging
        }

        CloseHandle(FileHandle);
    }
    else
    {
        // TODO(Kevin): Logging
    }

    return Result;
}

internal void
Win32LoadXInput()
{
    HMODULE XInputLibrary = LoadLibraryA(XINPUT_DLL_A);
    if (!XInputLibrary)
    {
        XInputLibrary = LoadLibraryA("xinput1_3.dll");
    }
    if (XInputLibrary)
    {
        XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
        XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
    }
}

internal void
Win32InitDirectSound(HWND Window, int32 SamplesPerSecond, int32 BufferSize)
{
    // NOTE(Kevin): Load the library
    HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");

    if (DSoundLibrary)
    {
        // NOTE(Kevin): Get a DirectSound object
        direct_sound_create *DirectSoundCreate = (direct_sound_create *)
            GetProcAddress(DSoundLibrary, "DirectSoundCreate");

        IDirectSound *DirectSound;
        if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
        {
            WAVEFORMATEX WaveFormat = {};
            WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
            WaveFormat.nChannels = 2;
            WaveFormat.nSamplesPerSec = SamplesPerSecond;
            WaveFormat.wBitsPerSample = 16;
            WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
            WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;
            WaveFormat.cbSize = 0;

            if (SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
            {
                DSBUFFERDESC BufferDescription = {};
                BufferDescription.dwSize = sizeof(DSBUFFERDESC);
                BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;

                // NOTE(Kevin): Create a primary buffer
                // TODO(Kevin): DSBCAPS_GLOBALFOCUS?
                IDirectSoundBuffer *PrimaryBuffer;
                if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
                {
                    if (SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat)))
                    {
                        printf("DSound primary buffer format was set.\n");
                    }
                    else
                    {
                        // TODO Diagnostic
                    }
                }
                else
                {
                    // TODO Diagnostic
                }
            }
            else
            {
                // TODO Diagnostic
            }

            // NOTE(Kevin): Create a secondary buffer
            // TODO(Kevin): DSBCAPS_GETCURRENTPOSITION2
            DSBUFFERDESC BufferDescription = {};
            BufferDescription.dwSize = sizeof(DSBUFFERDESC);
            BufferDescription.dwFlags = 0;
            BufferDescription.dwBufferBytes = BufferSize;
            BufferDescription.lpwfxFormat = &WaveFormat;

            if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSecondaryBuffer, 0)))
            {
                printf("DSound secondary buffer created.\n");
                // NOTE(Kevin): start it playing
            }
        }
        else
        {
            // TODO Diagnostic
        }
    }
    else
    {
        // TODO Diagnostic
    }
}

internal void
OpenConsole()
{
    AllocConsole();                          // Allocates a new console
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);  // Redirect stdout to the new console
    freopen_s(&fp, "CONOUT$", "w", stderr);  // Redirect stderr (optional)
    freopen_s(&fp, "CONIN$", "r", stdin);    // Redirect stdin (optional)
}

internal win32_window_dimension
Win32GetWindowDimension(HWND Window)
{
    win32_window_dimension Result;

    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    Result.Width = ClientRect.right - ClientRect.left;
    Result.Height = ClientRect.bottom - ClientRect.top;

    return Result;
}

internal void
Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height) // DIB: Device Independent Bitmap
{
    if(Buffer->Memory)
    {
        VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
    }

    Buffer->Width = Width;
    Buffer->Height = Height;
    Buffer->BytesPerPixel = 4;

    // NOTE(Kevin): When biHeight field is negative, this is the clue to
    // Windows to treat this bitmap as top-down, not bottom-up. Meaning that
    // the first three bytes of the image are the color for the top-left pixel
    // in the bitmap.
    Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
    Buffer->Info.bmiHeader.biWidth = Width;
    Buffer->Info.bmiHeader.biHeight = -Height;
    Buffer->Info.bmiHeader.biPlanes = 1;
    Buffer->Info.bmiHeader.biBitCount = 32;
    Buffer->Info.bmiHeader.biCompression = BI_RGB;

    int BitmapMemorySize = (Width * Height) * Buffer->BytesPerPixel;
    Buffer->Memory = VirtualAlloc(NULL, BitmapMemorySize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

    Buffer->Pitch = Width * Buffer->BytesPerPixel;

    // TODO(Kevin): Probably clear to black
}

internal void
Win32DisplayBufferInWindow(win32_offscreen_buffer *Buffer, 
    HDC DeviceContext, int WindowWidth, int WindowHeight)
{
    // TODO(Kevin): Aspect ratio correction
    StretchDIBits(DeviceContext,
        /*
        X, Y, Width, Height, // dest
        X, Y, Width, Height, // src
        */
        0, 0, WindowWidth, WindowHeight,
        0, 0, Buffer->Width, Buffer->Height,
        Buffer->Memory,
        &Buffer->Info,
        DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK 
Win32MainWindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    LRESULT Result = 0;

    switch(Message)
    {
        case WM_SIZE:
        {
        } break;

        case WM_DESTROY:
        {
            GlobalRunning = false;
        } break;

        case WM_CLOSE:
        {
            GlobalRunning = false;
        } break;

        case WM_ACTIVATEAPP:
        {

        } break;

        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            Assert(!"Keyboard input came in through a non-dispatch message!");
        } break;

        case WM_PAINT:
        {
            PAINTSTRUCT Paint;
            HDC DeviceContext = BeginPaint(Window, &Paint);
            win32_window_dimension Dimension = Win32GetWindowDimension(Window);
            Win32DisplayBufferInWindow(&GlobalBackbuffer, DeviceContext,
                Dimension.Width, Dimension.Height);
            EndPaint(Window, &Paint);
        } break;

        default:
        {
            Result = DefWindowProcA(Window, Message, WParam, LParam);
        } break;
    }

    return Result;
}

internal void
Win32ClearSoundBuffer(win32_sound_output *SoundOutput)
{
    VOID *Region1;
    DWORD Region1Size;
    VOID *Region2;
    DWORD Region2Size;
    if (SUCCEEDED(GlobalSecondaryBuffer->Lock(0, SoundOutput->SecondaryBufferSize,
                                              &Region1, &Region1Size,
                                              &Region2, &Region2Size,
                                              0)))
    {
        uint8 *DestSample = (uint8 *)Region1;
        for (DWORD ByteIndex = 0;
            ByteIndex < Region1Size;
            ++ByteIndex)
        {
            *DestSample++ = 0;
        }
        DestSample = (uint8 *)Region2;
        for (DWORD ByteIndex = 0;
            ByteIndex < Region2Size;
            ++ByteIndex)
        {
            *DestSample++ = 0;
        }

        GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
    }
}

internal void
Win32FillSoundBuffer(win32_sound_output *SoundOutput, DWORD ByteToLock, DWORD BytesToWrite,
                     game_sound_output_buffer *SourceBuffer)
{
    VOID *Region1;
    DWORD Region1Size;
    VOID *Region2;
    DWORD Region2Size;
    if (SUCCEEDED(GlobalSecondaryBuffer->Lock(ByteToLock, BytesToWrite,
                                              &Region1, &Region1Size,
                                              &Region2, &Region2Size,
                                              0)))
    {
        // TODO(Kevin): assert that Region1Size/Region2Size is valid

        // TODO(Kevin): collapse these two regions
        DWORD Region1SampleCount = Region1Size/SoundOutput->BytesPerSample;
        int16 *DestSample = (int16 *)Region1;
        int16 *SourceSample = SourceBuffer->Samples;
        for (DWORD SampleIndex = 0;
            SampleIndex < Region1SampleCount;
            ++SampleIndex)
        {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;
            ++SoundOutput->RunningSampleIndex;
        }

        DWORD Region2SampleCount = Region2Size/SoundOutput->BytesPerSample;
        DestSample = (int16 *)Region2;
        for (DWORD SampleIndex = 0;
            SampleIndex < Region2SampleCount;
            ++SampleIndex)
        {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;
            ++SoundOutput->RunningSampleIndex;
        }

        GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
    }
}

internal void
Win32ProcessKeyboardMessage(game_button_state *NewState, bool32 IsDown)
{
    Assert(NewState->EndedDown != IsDown);
    NewState->EndedDown = IsDown;
    ++NewState->HalfTransitionCount;
}

internal void
Win32ProcessXInputDigitalButton(DWORD XInputButtonState,
                                game_button_state *OldState, DWORD ButtonBit, 
                                game_button_state *NewState)
{
    NewState->EndedDown = ((XInputButtonState & ButtonBit) == ButtonBit);
    NewState->HalfTransitionCount = (OldState->EndedDown != NewState->EndedDown) ? 1 : 0;
}

internal real32
Win32ProcessXInputStickValue(SHORT StickValue, SHORT DeadZoneThreshold)
{
    real32 Result = 0;
    if (StickValue < -DeadZoneThreshold)
        Result = (real32)StickValue / 32768.f;
    else if (StickValue > DeadZoneThreshold)
        Result = (real32)StickValue / 32767.f;
    return Result;
}

internal void 
Win32ProcessPendingMessages(game_controller_input *KeyboardController)
{
    MSG Message;
    while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
    {
        switch (Message.message)
        {
            case WM_QUIT:
            {
                GlobalRunning = false;
            } break;

            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN:
            case WM_KEYUP:
            {
                uint32 VKCode = (uint32)Message.wParam;
                bool WasDown = (Message.lParam & (1 << 30)) != 0;
                bool IsDown = (Message.lParam & (1 << 31)) == 0;
                if (WasDown != IsDown)
                {
                    if (VKCode == 'W')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->MoveUp, IsDown);
                    }
                    else if (VKCode == 'A')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->MoveLeft, IsDown);
                    }
                    else if (VKCode == 'S')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->MoveDown, IsDown);
                    }
                    else if (VKCode == 'D')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->MoveRight, IsDown);
                    }
                    else if (VKCode == 'Q')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->LeftShoulder, IsDown);
                    }
                    else if (VKCode == 'E')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->RightShoulder, IsDown);
                    }
                    else if (VKCode == VK_LEFT)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->ActionLeft, IsDown);
                    }
                    else if (VKCode == VK_UP)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->ActionUp, IsDown);
                    }
                    else if (VKCode == VK_RIGHT)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->ActionRight, IsDown);
                    }
                    else if (VKCode == VK_DOWN)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->ActionDown, IsDown);
                    }
                    else if (VKCode == VK_ESCAPE)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->Back, IsDown);
                    }
                    else if (VKCode == VK_SPACE)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->Start, IsDown);
                    }
                }

                bool32 AltKeyDown = (Message.lParam & (1 << 29));
                if (AltKeyDown && VKCode == VK_F4)
                {
                    GlobalRunning = false;
                }
            } break;

            default:
            {
                TranslateMessage(&Message);
                DispatchMessage(&Message);
            } break;
        }
    }
}

inline LARGE_INTEGER 
Win32GetWallClock()
{
    LARGE_INTEGER Result;
    QueryPerformanceCounter(&Result);
    return Result;
}

inline real32 
Win32GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End)
{
    real32 Result = ((real32)(End.QuadPart - Start.QuadPart) / 
                     (real32)GlobalPerfCountFrequency);
    return Result;
}

internal void
Win32DebugDrawVertical(win32_offscreen_buffer *Backbuffer, 
                       int X, int Top, int Bottom, uint32 Color)
{
    uint8 *Pixel = (uint8 *) Backbuffer->Memory + 
                   X*Backbuffer->BytesPerPixel + 
                   Top*Backbuffer->Pitch;
    for (int Y = Top; Y < Bottom; ++Y)
    {
        *(uint32 *)Pixel = Color;
        Pixel += Backbuffer->Pitch;
    }
}

inline void
Win32DrawSoundBufferMarker(win32_offscreen_buffer *Backbuffer, 
                           win32_sound_output *SoundOutput,
                           real32 C, int PadX, int Top, int Bottom,
                           DWORD Value, uint32 Color)
{
    Assert(Value < SoundOutput->SecondaryBufferSize);
    int X = PadX + (int)(C * (real32)Value);
    Win32DebugDrawVertical(Backbuffer, X, Top, Bottom, Color);
}

internal void
Win32DebugSyncDisplay(win32_offscreen_buffer *Backbuffer, 
                      int MarkerCount, win32_debug_time_marker *Markers,
                      win32_sound_output *SoundOutput, real32 TargetSecondsPerFrame)
{
    // TODO(Kevin): Draw where we're writing our sound

    int PadX = 16;
    int PadY = 16;

    int Top = PadY;
    int Bottom = Backbuffer->Height - PadY;

    real32 C = (real32)(Backbuffer->Width - 2*PadX) / (real32)SoundOutput->SecondaryBufferSize;
    for (int MarkerIndex = 0;
         MarkerIndex < MarkerCount;
         ++MarkerIndex)
    {
        win32_debug_time_marker *ThisMarker = &Markers[MarkerIndex];
        Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom,
                                   ThisMarker->PlayCursor, 0xFFFFFFFF);
        Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom,
                                   ThisMarker->WriteCursor, 0xFFFF0000);
    }
}


// Win32 Entry Point: hInstance - handle to the current instance of the application
// Entered from C Runtime Library
int CALLBACK 
WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int ShowCode)
{
    LARGE_INTEGER PerfCountFrequencyResult;
    QueryPerformanceFrequency(&PerfCountFrequencyResult);
    GlobalPerfCountFrequency = PerfCountFrequencyResult.QuadPart;

    // NOTE(Kevin): Set the Windows scheduler granularity to 1ms
    // so that our Sleep() can be more granular.
    UINT DesiredSchedulerMS = 1;
    bool32 SleepIsGranular = (timeBeginPeriod(DesiredSchedulerMS) == TIMERR_NOERROR);

    OpenConsole();
    printf("Handmade Hero\n");

    Win32LoadXInput();

    Win32ResizeDIBSection(&GlobalBackbuffer, 1280, 720);

    WNDCLASSA WindowClass = {};
    WindowClass.style = CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance = Instance;
    //WindowClass.hIcon = ;
    WindowClass.lpszClassName = "HandmadeHeroWindowClass";

#define FramesOfAudioLatency 3
#define MonitorRefreshHz 60
#define GameUpdateHz (MonitorRefreshHz / 2)
    real32 TargetSecondsPerFrame = 1.0f / (real32)GameUpdateHz;

    if(RegisterClass(&WindowClass))
    {
        HWND Window = CreateWindowEx(
            0,                              // Optional window styles.
            WindowClass.lpszClassName,      // Window class
            "Handmade Hero",                // Window text
            WS_OVERLAPPEDWINDOW|WS_VISIBLE, // Window style
            // Size and position
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            NULL,       // Parent window    
            NULL,       // Menu
            Instance,   // Instance handle
            NULL);      // Additional application data
        if (Window)
        {
            // NOTE(Kevin): Since we specified CS_OWNDC, we can just
            // get one device context and use it forever because we
            // are not sharing it with anyone.
            HDC DeviceContext = GetDC(Window);

            win32_sound_output SoundOutput = {};

            SoundOutput.SamplesPerSecond = 48000;
            SoundOutput.RunningSampleIndex = 0;
            SoundOutput.BytesPerSample = sizeof(int16)*2;
            SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond*SoundOutput.BytesPerSample;
            SoundOutput.LatencySampleCount = FramesOfAudioLatency*(SoundOutput.SamplesPerSecond / GameUpdateHz);
            Win32InitDirectSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
            Win32ClearSoundBuffer(&SoundOutput);
            GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

            GlobalRunning = true;

            // TODO(Kevin): Pool with bitmap VirtualAlloc
            int16 *Samples = (int16 *)VirtualAlloc(0, SoundOutput.SecondaryBufferSize, 
                                                   MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

#if HANDMADE_INTERNAL
            LPVOID BaseAddress = (LPVOID)Terabytes(2);
#else
            LPVOID BaseAddress = 0;
#endif
            game_memory GameMemory = {};
            GameMemory.PermanentStorageSize = Megabytes(64);
            GameMemory.TransientStorageSize = Gigabytes(4);

            uint64 TotalSize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;
            GameMemory.PermanentStorage = VirtualAlloc(BaseAddress, TotalSize, 
                                                       MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
            GameMemory.TransientStorage = ((uint8 *)GameMemory.PermanentStorage + 
                                           GameMemory.PermanentStorageSize);

            if (Samples && GameMemory.PermanentStorage)
            {
                game_input Input[2] = {};
                game_input *NewInput = &Input[0];
                game_input *OldInput = &Input[1];

                LARGE_INTEGER LastCounter = Win32GetWallClock();

                int DebugTimeMarkersIndex = 0;
                win32_debug_time_marker DebugTimeMarkers[GameUpdateHz/2] = {0};

                DWORD LastPlayCursor = 0;
                bool32 SoundIsValid = false;

                while (GlobalRunning)
                {
                    // TODO(Kevin): Zeroing macro
                    game_controller_input *OldKeyboardController = GetController(OldInput, 0);
                    game_controller_input *NewKeyboardController = GetController(NewInput, 0);
                    *NewKeyboardController = {};
                    NewKeyboardController->IsConnected = true;
                    for (int ButtonIndex = 0;
                         ButtonIndex < ArrayCount(NewKeyboardController->Buttons);
                         ++ButtonIndex)
                    {
                        NewKeyboardController->Buttons[ButtonIndex].EndedDown =
                            OldKeyboardController->Buttons[ButtonIndex].EndedDown;
                    }

                    Win32ProcessPendingMessages(NewKeyboardController);

                    // TODO(Kevin): Should we poll this more frequently?
                    // TODO(Kevin): apparently XInputGetState can stall for several ms if the
                    // controller of the index we are polling is not plugged in. should use HID
                    // notification to know how many controllers are plugged in?
                    DWORD MaxControllerCount = XUSER_MAX_COUNT;
                    if (MaxControllerCount > (ArrayCount(NewInput->Controllers) - 1))
                    {
                        MaxControllerCount = (ArrayCount(NewInput->Controllers) - 1);
                    }

                    for (DWORD ControllerIndex = 0;
                        ControllerIndex < MaxControllerCount;
                        ++ControllerIndex)
                    {
                        DWORD OurControllerIndex = ControllerIndex + 1;
                        game_controller_input *OldController = GetController(OldInput, OurControllerIndex);
                        game_controller_input *NewController = GetController(NewInput, OurControllerIndex);

                        XINPUT_STATE ControllerState;
                        if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
                        {
                            NewController->IsConnected = true;

                            // NOTE(Kevin): The controller is plugged in
                            // see if ControllerState.dwPacketNumber increments too rapidly
                            XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

                            NewController->StickAverageX = Win32ProcessXInputStickValue(
                                Pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                            NewController->StickAverageY = Win32ProcessXInputStickValue(
                                Pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                            if ((NewController->StickAverageX != 0.f) || 
                                (NewController->StickAverageY != 0.f))
                            {
                                NewController->IsAnalog = true;
                            }

                            if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP)
                            {
                                NewController->StickAverageY = 1.0f;
                                NewController->IsAnalog = false;
                            }
                            if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
                            {
                                NewController->StickAverageY = -1.0f;
                                NewController->IsAnalog = false;
                            }
                            if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
                            {
                                NewController->StickAverageX = -1.0f;
                                NewController->IsAnalog = false;
                            }
                            if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
                            {
                                NewController->StickAverageX = 1.0f;
                                NewController->IsAnalog = false;
                            }

                            real32 Threshold = 0.5f;
                            Win32ProcessXInputDigitalButton(
                                (NewController->StickAverageY < -Threshold) ? 1 : 0, 
                                &OldController->MoveDown, 1,
                                &NewController->MoveDown);
                            Win32ProcessXInputDigitalButton(
                                (NewController->StickAverageX > Threshold) ? 1 : 0, 
                                &OldController->MoveRight, 1,
                                &NewController->MoveRight);
                            Win32ProcessXInputDigitalButton(
                                (NewController->StickAverageX < -Threshold) ? 1 : 0, 
                                &OldController->MoveLeft, 1,
                                &NewController->MoveLeft);
                            Win32ProcessXInputDigitalButton(
                                (NewController->StickAverageY > Threshold) ? 1 : 0, 
                                &OldController->MoveUp, 1,
                                &NewController->MoveUp);

                            Win32ProcessXInputDigitalButton(Pad->wButtons, 
                                &OldController->ActionDown, XINPUT_GAMEPAD_A, 
                                &NewController->ActionDown);
                            Win32ProcessXInputDigitalButton(Pad->wButtons, 
                                &OldController->ActionRight, XINPUT_GAMEPAD_B, 
                                &NewController->ActionRight);
                            Win32ProcessXInputDigitalButton(Pad->wButtons, 
                                &OldController->ActionLeft, XINPUT_GAMEPAD_X,
                                &NewController->ActionLeft);
                            Win32ProcessXInputDigitalButton(Pad->wButtons, 
                                &OldController->ActionUp, XINPUT_GAMEPAD_Y, 
                                &NewController->ActionUp);

                            Win32ProcessXInputDigitalButton(Pad->wButtons, 
                                &OldController->LeftShoulder, XINPUT_GAMEPAD_LEFT_SHOULDER, 
                                &NewController->LeftShoulder);
                            Win32ProcessXInputDigitalButton(Pad->wButtons, 
                                &OldController->RightShoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER, 
                                &NewController->RightShoulder);
                            Win32ProcessXInputDigitalButton(Pad->wButtons, 
                                &OldController->Start, XINPUT_GAMEPAD_START, 
                                &NewController->Start);
                            Win32ProcessXInputDigitalButton(Pad->wButtons, 
                                &OldController->Back, XINPUT_GAMEPAD_BACK, 
                                &NewController->Back);
                        }
                        else
                        {
                            // NOTE(Kevin): The controller is not available
                            NewController->IsConnected = false;
                        }
                    }

                    // NOTE(Kevin): Compute how much sound to write and where
                    DWORD ByteToLock = 0;
                    DWORD TargetCursor = 0;
                    DWORD BytesToWrite = 0;
                    if (SoundIsValid)
                    {
                        ByteToLock = ((SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) % 
                                      SoundOutput.SecondaryBufferSize);

                        // wherever sound play cursor was when we flipped last frame
                        // advance by however many samples of latency we want * bytes per sample
                        // that is the location that we want; we map (%) that into the buffer size 
                        // so both ByteToLock and TargetCursor are mapped into our sound buffer
                        TargetCursor = 
                            ((LastPlayCursor + 
                              (SoundOutput.LatencySampleCount * SoundOutput.BytesPerSample)) % 
                             SoundOutput.SecondaryBufferSize);

                        if (ByteToLock > TargetCursor)
                        {
                            BytesToWrite = (SoundOutput.SecondaryBufferSize - ByteToLock);
                            BytesToWrite += TargetCursor;
                        }
                        else
                        {
                            BytesToWrite = TargetCursor - ByteToLock;
                        }

                        printf("PC:%u BTL:%u TC:%u BTW:%u\n", 
                            LastPlayCursor, ByteToLock, TargetCursor, BytesToWrite);
                    }

                    game_sound_output_buffer SoundBuffer = {};
                    SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
                    SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
                    SoundBuffer.Samples = Samples;

                    game_offscreen_buffer Buffer = {};
                    Buffer.Memory = GlobalBackbuffer.Memory;
                    Buffer.Width = GlobalBackbuffer.Width;
                    Buffer.Height = GlobalBackbuffer.Height;
                    Buffer.Pitch = GlobalBackbuffer.Pitch;
                    GameUpdateAndRender(&GameMemory, NewInput, &Buffer, &SoundBuffer);

                    // NOTE(Kevin): DirectSound output test
                    if (SoundIsValid)
                    {
                        Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
                    }

                    LARGE_INTEGER WorkCounter = Win32GetWallClock();
                    real32 WorkSecondsElapsed = Win32GetSecondsElapsed(LastCounter, WorkCounter);

                    // TODO(Kevin): NOT TESTED YET PROBABLY BUGGY
                    real32 SecondsElapsedForFrame = WorkSecondsElapsed;
                    if (SecondsElapsedForFrame < TargetSecondsPerFrame)
                    {
                        if (SleepIsGranular)
                        {
                            DWORD SleepMS = (DWORD)(1000.0f*(TargetSecondsPerFrame - SecondsElapsedForFrame));
                            if (SleepMS > 0)
                            {
                                Sleep(SleepMS);
                            }
                        }

                        while (SecondsElapsedForFrame < TargetSecondsPerFrame)
                        {
                            SecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
                        }
                    }
                    else
                    {
                        // TODO(Kevin): MISSED FRAME RATE!
                        // TODO(Kevin): Logging
                    }

                    LARGE_INTEGER EndCounter = Win32GetWallClock();                    
                    real32 MSPerFrame = 1000.f*Win32GetSecondsElapsed(LastCounter, EndCounter);
                    LastCounter = EndCounter;

                    win32_window_dimension Dimension = Win32GetWindowDimension(Window);
#if HANDMADE_INTERNAL
                    Win32DebugSyncDisplay(&GlobalBackbuffer,
                        ArrayCount(DebugTimeMarkers), DebugTimeMarkers,
                        &SoundOutput, TargetSecondsPerFrame);
#endif
                    Win32DisplayBufferInWindow(&GlobalBackbuffer, DeviceContext,
                        Dimension.Width, Dimension.Height);

                    DWORD PlayCursor;  // What we care about is the play cursor
                    DWORD WriteCursor; // WriteCursor just says don't write before this cursor
                                       // just something that says your latency must be past this
                    if (SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor)))
                    {
                        LastPlayCursor = PlayCursor;
                        if (!SoundIsValid)
                        {
                            SoundOutput.RunningSampleIndex = WriteCursor / SoundOutput.BytesPerSample;
                            SoundIsValid = true;
                        }
                    }
                    else
                    {
                        SoundIsValid = false;
                    }

#if HANDMADE_INTERNAL
                    {
                        win32_debug_time_marker *Marker = &DebugTimeMarkers[DebugTimeMarkersIndex++];
                        if (DebugTimeMarkersIndex > ArrayCount(DebugTimeMarkers))
                        {
                            DebugTimeMarkersIndex = 0;
                        }
                        Marker->PlayCursor = PlayCursor;
                        Marker->WriteCursor = WriteCursor;
                    }
#endif

                    game_input *Temp = NewInput;
                    NewInput = OldInput;
                    OldInput = Temp;
                    // TODO(Kevin): Should I clear these here?

                    real64 FPS = 1000.f / MSPerFrame;
                    char PrintBuffer[256];
                    sprintf(PrintBuffer, "%.02fms/f, %.02ff/s\n", MSPerFrame, FPS);
                    // OutputDebugStringA(PrintBuffer);
                    printf(PrintBuffer);
                }
            }
            else
            {
                // TODO(Kevin): Logging
            }

            ReleaseDC(Window, DeviceContext);
        }
        else
        {
            // TODO(Kevin): Logging
        }
    }
    else
    {
        // TODO(Kevin): Logging
    }

    return 0;
}
