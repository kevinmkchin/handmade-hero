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
            uint32 VKCode = WParam;
            bool WasDown = (LParam & (1 << 30)) != 0;
            bool IsDown = (LParam & (1 << 31)) == 0;
            if (WasDown != IsDown)
            {
                if (VKCode == 'W')
                {
                }
                else if (VKCode == 'A')
                {
                }
                else if (VKCode == 'S')
                {
                }
                else if (VKCode == 'D')
                {
                }
                else if (VKCode == 'Q')
                {
                }
                else if (VKCode == 'E')
                {
                }
                else if (VKCode == VK_LEFT)
                {
                }
                else if (VKCode == VK_UP)
                {
                }
                else if (VKCode == VK_RIGHT)
                {
                }
                else if (VKCode == VK_DOWN)
                {
                }
                else if (VKCode == VK_ESCAPE)
                {
                    if (IsDown)
                        printf("escape is down");
                    if (WasDown)
                        printf("escape was down");
                    printf("\n");
                }
                else if (VKCode == VK_SPACE)
                {
                }
            }

            bool32 AltKeyDown = (LParam & (1 << 29));
            if (AltKeyDown && VKCode == VK_F4)
            {
                GlobalRunning = false;
            }
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
Win32ProcessXInputDigitalButton(DWORD XInputButtonState,
                                game_button_state *OldState, DWORD ButtonBit, 
                                game_button_state *NewState)
{
    NewState->EndedDown = ((XInputButtonState & ButtonBit) == ButtonBit);
    NewState->HalfTransitionCount = (OldState->EndedDown != NewState->EndedDown) ? 1 : 0;
}

// Win32 Entry Point: hInstance - handle to the current instance of the application
// Entered from C Runtime Library
int CALLBACK 
WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int ShowCode)
{
    LARGE_INTEGER PerfCountFrequencyResult;
    QueryPerformanceFrequency(&PerfCountFrequencyResult);
    int64 PerfCountFrequency = PerfCountFrequencyResult.QuadPart;

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
            SoundOutput.LatencySampleCount = SoundOutput.SamplesPerSecond / 15;
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

                LARGE_INTEGER LastCounter;
                QueryPerformanceCounter(&LastCounter);
                while (GlobalRunning)
                {
                    MSG Message;

                    while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
                    {
                        if (Message.message == WM_QUIT)
                        {
                            GlobalRunning = false;
                        }

                        TranslateMessage(&Message);
                        DispatchMessage(&Message);
                    }

                    // TODO(Kevin): Should we poll this more frequently?
                    // TODO(Kevin): apparently XInputGetState can stall for several ms if the
                    // controller of the index we are polling is not plugged in. should use HID
                    // notification to know how many controllers are plugged in?
                    int MaxControllerCount = XUSER_MAX_COUNT;
                    if (MaxControllerCount > ArrayCount(NewInput->Controllers))
                    {
                        MaxControllerCount = ArrayCount(NewInput->Controllers);
                    }

                    for (DWORD ControllerIndex = 0;
                        ControllerIndex < MaxControllerCount;
                        ++ControllerIndex)
                    {
                        game_controller_input *OldController = &OldInput->Controllers[ControllerIndex];
                        game_controller_input *NewController = &NewInput->Controllers[ControllerIndex];

                        XINPUT_STATE ControllerState;
                        if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
                        {
                            // NOTE(Kevin): The controller is plugged in
                            // see if ControllerState.dwPacketNumber increments too rapidly
                            XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

                            // TODO(Kevin): DPad
                            // bool Up = Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
                            // bool Down = Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
                            // bool Left = Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
                            // bool Right = Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;

                            NewController->IsAnalog = true;
                            NewController->StartX = OldController->EndX;
                            NewController->StartY = OldController->EndY;

                            // TODO(Kevin): Dead zone processing
                            // XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE
                            // XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE

                            // TODO(Kevin): Min/Max macros
                            real32 X;
                            if(Pad->sThumbLX < 0)
                                X = (real32)Pad->sThumbLX / 32768.f;
                            else
                                X = (real32)Pad->sThumbLX / 32767.f;
                            NewController->MinX = NewController->MaxX = NewController->EndX = X;

                            real32 Y;
                            if(Pad->sThumbLY < 0)
                                Y = (real32)Pad->sThumbLY / 32768.f;
                            else
                                Y = (real32)Pad->sThumbLY / 32767.f;
                            NewController->MinY = NewController->MaxY = NewController->EndY = Y;

                            Win32ProcessXInputDigitalButton(Pad->wButtons, 
                                &OldController->Down, XINPUT_GAMEPAD_A, 
                                &NewController->Down);
                            Win32ProcessXInputDigitalButton(Pad->wButtons, 
                                &OldController->Right, XINPUT_GAMEPAD_B, 
                                &NewController->Right);
                            Win32ProcessXInputDigitalButton(Pad->wButtons, 
                                &OldController->Left, XINPUT_GAMEPAD_X, 
                                &NewController->Left);
                            Win32ProcessXInputDigitalButton(Pad->wButtons, 
                                &OldController->Up, XINPUT_GAMEPAD_Y, 
                                &NewController->Up);
                            Win32ProcessXInputDigitalButton(Pad->wButtons, 
                                &OldController->LeftShoulder, XINPUT_GAMEPAD_LEFT_SHOULDER, 
                                &NewController->LeftShoulder);
                            Win32ProcessXInputDigitalButton(Pad->wButtons, 
                                &OldController->RightShoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER, 
                                &NewController->RightShoulder);

                            // bool Start = Pad->wButtons & XINPUT_GAMEPAD_START;
                            // bool Back = Pad->wButtons & XINPUT_GAMEPAD_BACK;
                            // bool LThumbButton = Pad->wButtons & XINPUT_GAMEPAD_LEFT_THUMB;
                            // bool RThumbButton = Pad->wButtons & XINPUT_GAMEPAD_RIGHT_THUMB;
                            // uint8 LTrigger = Pad->bLeftTrigger;
                            // uint8 RTrigger = Pad->bRightTrigger;
                            // int16 RThumbStickX = Pad->sThumbRX;
                            // int16 RThumbStickY = Pad->sThumbRY;
                            // if (AButton)
                            // {
                            //     XINPUT_VIBRATION ControllerVibration;
                            //     ControllerVibration.wLeftMotorSpeed = 60000;
                            //     ControllerVibration.wRightMotorSpeed = 60000;
                            //     XInputSetState(ControllerIndex, &ControllerVibration);
                            // }
                            // else
                            // {
                            //     XINPUT_VIBRATION ControllerVibration;
                            //     ControllerVibration.wLeftMotorSpeed = 0;
                            //     ControllerVibration.wRightMotorSpeed = 0;
                            //     XInputSetState(ControllerIndex, &ControllerVibration);
                            // }
                            // if (LTrigger)
                            //     YOffset += LTrigger / 50;
                            // if (RTrigger)
                            //     YOffset -= RTrigger / 50;
                        }
                        else
                        {
                            // NOTE(Kevin): The controller is not available
                        }
                    }

                    DWORD ByteToLock;
                    DWORD TargetCursor;
                    DWORD BytesToWrite = 0;
                    DWORD PlayCursor;
                    DWORD WriteCursor;
                    bool32 SoundIsValid = false;
                    // TODO(Kevin): Tighten up sound logic so that we know where we should be
                    // writing to and can anticipate the time spent in game update.
                    if (SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor)))
                    {
                        ByteToLock = 
                            (SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) 
                            % SoundOutput.SecondaryBufferSize;
                        TargetCursor = 
                            (PlayCursor + 
                                (SoundOutput.LatencySampleCount * SoundOutput.BytesPerSample)) 
                            % SoundOutput.SecondaryBufferSize;
                        if (ByteToLock > TargetCursor)
                        {
                            BytesToWrite = (SoundOutput.SecondaryBufferSize - ByteToLock);
                            BytesToWrite += TargetCursor;
                        }
                        else
                        {
                            BytesToWrite = TargetCursor - ByteToLock;
                        }

                        SoundIsValid = true;
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

                    win32_window_dimension Dimension = Win32GetWindowDimension(Window);
                    Win32DisplayBufferInWindow(&GlobalBackbuffer, DeviceContext,
                        Dimension.Width, Dimension.Height);

                    LARGE_INTEGER EndCounter;
                    QueryPerformanceCounter(&EndCounter);

                    int64 CounterElapsed = EndCounter.QuadPart - LastCounter.QuadPart;
                    real32 SecondsElapsed = ((real32)CounterElapsed / (real32)PerfCountFrequency);
                    real32 FPS = ((real32)PerfCountFrequency / (real32)CounterElapsed);

                    LastCounter = EndCounter;

                    game_input *Temp = NewInput;
                    NewInput = OldInput;
                    OldInput = Temp;
                    // TODO(Kevin): Should I clear these here?
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
