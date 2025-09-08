/*
Platform Layer TODO

- Saved game locations
- Getting a handle to our own executable file
- Asset loading path
- Threading (launch a thread)
- Raw Input (support for multiple keyboards)
- ClipCursor (multimonitor support)
- Fullscreen support
- WM_SETCURSOR (control cursor visibility)
- QueryCancelAutoplay
- WM_ACTIVATEAPP (for when we are not the active application)
- Blit speed improvements (BitBlt)
- Hardware acceleration (OpenGL or Direct3D)
- GetKeyboardLayout (for French keyboards, international WASD support)

  Just a partial list of stuff!!

*/

#pragma comment(lib, "ole32.lib")
#define HANDMADE_WIN32_OPEN_CONSOLE 0
#define HANDMADE_WIN32_FORCE_OFF_DWM_DPI_SCALING 1

#include "handmade.h"

#include <windows.h>
#include <stdio.h>
#include <malloc.h>
#if HANDMADE_WIN32_FORCE_OFF_DWM_DPI_SCALING
#include <shellscalingapi.h>
#endif
#include <xinput.h>
#include <mmdeviceapi.h>
#include <audioclient.h>

#include "win32_handmade.h"

global_variable bool GlobalRunning;
global_variable win32_offscreen_buffer GlobalBackbuffer;
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

internal void
CatStrings(size_t SourceACount, char *SourceA,
           size_t SourceBCount, char *SourceB,
           size_t DestCount, char *Dest)
{
    for (size_t Index = 0; Index < SourceACount; ++Index)
    {
        *Dest++ = *SourceA++;
    }
    for (size_t Index = 0; Index < SourceBCount; ++Index)
    {
        *Dest++ = *SourceB++;
    }
    *Dest++ = 0;
}

internal int
StringLength(char *String)
{
    int Count = 0;
    while (*String++)
    {
        ++Count;
    }
    return Count;
}

internal void
Win32GetEXEFileName(win32_state *State)
{
    DWORD SizeOfFilePath = GetModuleFileNameA(0, State->EXEFileName, 
                                              sizeof(State->EXEFileName));
    State->OnePastLastEXEFileNameSlash = State->EXEFileName;
    for (char *Scan = State->EXEFileName; *Scan; ++Scan)
    {
        if (*Scan == '\\')
        {
            State->OnePastLastEXEFileNameSlash = Scan + 1;
        }
    }
}

internal void
Win32BuildEXEPathFileName(win32_state *State, char *FileName,
                          int DestCount, char *Dest)
{
    CatStrings(State->OnePastLastEXEFileNameSlash - State->EXEFileName, State->EXEFileName,
               StringLength(FileName), FileName, DestCount, Dest);
}

DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory)
{
    if (BitmapMemory)
    {
        VirtualFree(BitmapMemory, 0, MEM_RELEASE);
    }
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile)
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
                    DEBUGPlatformFreeFileMemory(Thread, Result.Contents);
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

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile)
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

inline FILETIME
Win32GetLastWriteTime(char *Filepath)
{
    FILETIME LastWriteTime = {};

    WIN32_FILE_ATTRIBUTE_DATA Data;
    if(GetFileAttributesEx(Filepath, GetFileExInfoStandard, &Data))
    {
        LastWriteTime = Data.ftLastWriteTime;
    }

    return LastWriteTime;
}

internal win32_game_code
Win32LoadGameCode(char *SourceDLLName, char *TempDLLName, char *LockFileName)
{
    win32_game_code Result = {};

    WIN32_FILE_ATTRIBUTE_DATA Ignored;
    if(!GetFileAttributesEx(LockFileName, GetFileExInfoStandard, &Ignored))
    {
        if (CopyFile(SourceDLLName, TempDLLName, FALSE))
        {
            Result.GameCodeDLL = LoadLibraryA(TempDLLName);
            if (Result.GameCodeDLL)
            {
                Result.DLLLastWriteTime = Win32GetLastWriteTime(SourceDLLName);
                Result.UpdateAndRender = (game_update_and_render *)
                    GetProcAddress(Result.GameCodeDLL, "GameUpdateAndRender");
                Result.GetSoundSamples = (game_get_sound_samples *)
                    GetProcAddress(Result.GameCodeDLL, "GameGetSoundSamples");

                Result.IsValid  = (Result.UpdateAndRender && Result.GetSoundSamples);
            }
        }
    }

    if (!Result.IsValid)
    {
        Result.UpdateAndRender = 0;
        Result.GetSoundSamples = 0;
    }

    return Result;
}

internal void
Win32UnloadGameCode(win32_game_code *GameCode)
{
    if (GameCode->GameCodeDLL)
    {
        FreeLibrary(GameCode->GameCodeDLL);
    }

    GameCode->GameCodeDLL = 0;
    GameCode->IsValid = false;
    GameCode->UpdateAndRender = 0;
    GameCode->GetSoundSamples = 0;
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
        if (!XInputGetState) { XInputGetState = XInputGetStateStub; }
        XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
        if (!XInputSetState) { XInputSetState = XInputSetStateStub; }
    }
}

#if HANDMADE_WIN32_OPEN_CONSOLE
internal void
OpenConsole()
{
    AllocConsole();                          // Allocates a new console
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);  // Redirect stdout to the new console
    freopen_s(&fp, "CONOUT$", "w", stderr);  // Redirect stderr (optional)
    freopen_s(&fp, "CONIN$", "r", stdin);    // Redirect stdin (optional)
}
#endif

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

    int BytesPerPixel = 4;
    Buffer->BytesPerPixel = BytesPerPixel;

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

    int BitmapMemorySize = (Buffer->Width * Buffer->Height) * Buffer->BytesPerPixel;
    Buffer->Memory = VirtualAlloc(NULL, BitmapMemorySize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    Buffer->Pitch = Width * Buffer->BytesPerPixel;

    // TODO(Kevin): Probably clear to black
}

internal void
Win32DisplayBufferInWindow(win32_offscreen_buffer *Buffer, 
    HDC DeviceContext, int WindowWidth, int WindowHeight)
{
    int IntScale = 1;
#if HANDMADE_WIN32_FORCE_OFF_DWM_DPI_SCALING
    IntScale = 2;
#endif
    int OffsetX = 10;
    int OffsetY = 10;

    PatBlt(DeviceContext, 0, 0, WindowWidth, OffsetY, BLACKNESS);
    PatBlt(DeviceContext, 0, OffsetY + Buffer->Height * IntScale, WindowWidth, WindowHeight, BLACKNESS);
    PatBlt(DeviceContext, 0, 0, OffsetX, WindowHeight, BLACKNESS);
    PatBlt(DeviceContext, OffsetX + Buffer->Width * IntScale, 0, WindowWidth, WindowHeight, BLACKNESS);

    // NOTE(Kevin): For prototyping purposes, we're going to always blit
    // 1-to-1 pixels to make sure we don't introduce artifacts with
    // stretching while we are learning to code the renderer.
    StretchDIBits(DeviceContext,
        OffsetX, OffsetY, Buffer->Width * IntScale, Buffer->Height * IntScale,
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
            // NOTE(Kevin): Queue nonqueued message to handle later.
            PostMessageA(Window, Message, WParam, LParam);
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
Win32ProcessKeyboardMessage(game_button_state *NewState, bool32 IsDown)
{
    if (NewState->EndedDown != IsDown)
    {
        NewState->EndedDown = IsDown;
        ++NewState->HalfTransitionCount;   
    }
}

internal void
Win32CopyKeyboardEndedDownStates(game_controller_input *Dest, 
                                 game_controller_input *Src)
{
    for (int ButtonIndex = 0;
        ButtonIndex < ArrayCount(Dest->Buttons);
        ++ButtonIndex)
    {
        Dest->Buttons[ButtonIndex].EndedDown = Src->Buttons[ButtonIndex].EndedDown;
    }
}

internal void
Win32ClearKeyboardEndedDownStates(game_controller_input *KeyboardController)
{
    game_controller_input BlankButtonState = {};
    Win32CopyKeyboardEndedDownStates(KeyboardController, &BlankButtonState);
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
Win32GetInputFileLocation(win32_state *State, int SlotIndex,
                          int DestCount, char *Dest)
{
    Assert(SlotIndex == 1);
    Win32BuildEXEPathFileName(State, "loop_edit.hmi", DestCount, Dest);
}

internal void
Win32BeginRecordingInput(win32_state *State, int InputRecordingIndex)
{
    State->InputRecordingIndex = InputRecordingIndex;

    char Filename[WIN32_STATE_FILE_NAME_COUNT];
    Win32GetInputFileLocation(State, InputRecordingIndex, sizeof(Filename), Filename);

    State->RecordingHandle = 
        CreateFileA(Filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    DWORD BytesToWrite = (DWORD)State->GameMemoryTotalSize;
    Assert(State->GameMemoryTotalSize == BytesToWrite);
    DWORD BytesWritten;
    WriteFile(State->RecordingHandle, State->GameMemoryBlock, 
        BytesToWrite, &BytesWritten, 0);
}

internal void
Win32EndRecordingInput(win32_state *State)
{
    CloseHandle(State->RecordingHandle);
    State->InputRecordingIndex = 0;
}

internal void
Win32BeginInputPlayBack(win32_state *State, int InputPlayingIndex)
{
    State->InputPlayingIndex = InputPlayingIndex;

    char Filename[WIN32_STATE_FILE_NAME_COUNT];
    Win32GetInputFileLocation(State, InputPlayingIndex, sizeof(Filename), Filename);

    State->PlaybackHandle =
        CreateFileA(Filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    
    DWORD BytesToRead = (DWORD)State->GameMemoryTotalSize;
    Assert(State->GameMemoryTotalSize == BytesToRead);
    DWORD BytesRead;
    ReadFile(State->PlaybackHandle, State->GameMemoryBlock,
        BytesToRead, &BytesRead, 0);
}

internal void
Win32EndInputPlayBack(win32_state *State)
{
    CloseHandle(State->PlaybackHandle);
    State->InputPlayingIndex = 0;
}

internal void
Win32RecordInput(win32_state *State, game_input *NewInput)
{
    DWORD BytesWritten;
    WriteFile(State->RecordingHandle, NewInput, sizeof(*NewInput), &BytesWritten, 0);
}

internal void
Win32PlayBackInput(win32_state *State, game_input *NewInput)
{
    DWORD BytesRead;
    if (ReadFile(State->PlaybackHandle, NewInput, sizeof(*NewInput), &BytesRead, 0))
    {
        if (BytesRead == 0)
        {
            int PlayingIndex = State->InputPlayingIndex;
            Win32EndInputPlayBack(State);
            Win32BeginInputPlayBack(State, PlayingIndex);
            ReadFile(State->PlaybackHandle, NewInput, sizeof(*NewInput), &BytesRead, 0);
        }
    }
    else
    {
        // TODO(Kevin): Logging
        Win32EndInputPlayBack(State);
    }
}

internal void 
Win32ProcessPendingMessages(win32_state *State, game_controller_input *KeyboardController)
{
    // NOTE(Kevin): Nonqueued messages are sent immediately to the main 
    // window callback and bypass the message queue.

    MSG Message;
    while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
    {
        switch (Message.message)
        {
            case WM_QUIT:
            {
                GlobalRunning = false;
            } break;

            case WM_ACTIVATEAPP:
            {
                if (Message.wParam == TRUE)
                {

                }
                else
                {
                    Win32ClearKeyboardEndedDownStates(KeyboardController);
                }
            } break;

            case WM_MOUSEWHEEL:
            {

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
#if HANDMADE_INTERNAL
                    else if (VKCode == 'L')
                    {
                        if (IsDown)
                        {
                            if (State->InputPlayingIndex == 0)
                            {                                
                                if (State->InputRecordingIndex == 0)
                                {
                                    Win32BeginRecordingInput(State, 1);
                                }
                                else
                                {
                                    Win32EndRecordingInput(State);
                                    Win32BeginInputPlayBack(State, 1);
                                }
                            }
                            else
                            {
                                Win32EndInputPlayBack(State);
                                Win32ClearKeyboardEndedDownStates(KeyboardController);
                            }
                        }
                    }
#endif

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

// Win32 Entry Point: hInstance - handle to the current instance of the application
// Entered from C Runtime Library
int CALLBACK 
WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int ShowCode)
{
    // NOTE(Kevin): https://learn.microsoft.com/en-us/windows/win32/learnwin32/initializing-the-com-library
    Assert(SUCCEEDED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)));

    win32_state Win32State = {};

    LARGE_INTEGER PerfCountFrequencyResult;
    QueryPerformanceFrequency(&PerfCountFrequencyResult);
    GlobalPerfCountFrequency = PerfCountFrequencyResult.QuadPart;

    Win32GetEXEFileName(&Win32State);

    char SourceGameCodeDLLFullPath[WIN32_STATE_FILE_NAME_COUNT];
    Win32BuildEXEPathFileName(&Win32State, "handmade.dll",
        sizeof(SourceGameCodeDLLFullPath), SourceGameCodeDLLFullPath);

    char TempGameCodeDLLFullPath[WIN32_STATE_FILE_NAME_COUNT];
    Win32BuildEXEPathFileName(&Win32State, "handmade_temp.dll",
        sizeof(TempGameCodeDLLFullPath), TempGameCodeDLLFullPath);

    char GameCodeLockFullPath[WIN32_STATE_FILE_NAME_COUNT];
    Win32BuildEXEPathFileName(&Win32State, "lock.tmp",
        sizeof(GameCodeLockFullPath), GameCodeLockFullPath);

    // NOTE(Kevin): Set the Windows scheduler granularity to 1ms
    // so that our Sleep() can be more granular.
    UINT DesiredSchedulerMS = 1;
    TIMECAPS TimerDeviceResolution;
    if (timeGetDevCaps(&TimerDeviceResolution, sizeof(TIMECAPS)) == MMSYSERR_NOERROR)
    {
        DesiredSchedulerMS = TimerDeviceResolution.wPeriodMin;
    }
    bool32 SleepIsGranular = (timeBeginPeriod(DesiredSchedulerMS) == TIMERR_NOERROR);

#if HANDMADE_WIN32_OPEN_CONSOLE
    OpenConsole();
#endif
    // printf("Handmade Hero\n");

    Win32LoadXInput();

#if HANDMADE_WIN32_FORCE_OFF_DWM_DPI_SCALING
    // NOTE(Kevin): For writing debug rendering code atm, turn off DWM compositor's
    // DPI scaling! Otherwise not pixel perfect.
    SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
#endif
    // NOTE(Kevin): 1080p display mode is 1920x1080
    //  1920 -> 2048 = 2048 - 1920 -> 128
    //  1080 -> 2048 = 2048 - 1080 -> 968
    //  1024 + 128 = 1152
    Win32ResizeDIBSection(&GlobalBackbuffer, 960, 540);

    WNDCLASSA WindowClass = {};
    WindowClass.style = CS_HREDRAW|CS_VREDRAW;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance = Instance;
    //WindowClass.hIcon = ;
    WindowClass.lpszClassName = "HandmadeHeroWindowClass";

    if(RegisterClass(&WindowClass))
    {
        HWND Window = CreateWindowExA(
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
            int MonitorRefreshHz = 60;
            HDC RefreshDC = GetDC(Window);
            int Win32RefreshRate = GetDeviceCaps(RefreshDC, VREFRESH);
            ReleaseDC(Window, RefreshDC);
            if (Win32RefreshRate > 1)
            {
                MonitorRefreshHz = Win32RefreshRate;
            }
            real32 GameUpdateHz = (MonitorRefreshHz / 2.f);
            real32 TargetSecondsPerFrame = 1.0f / GameUpdateHz;

            IMMDeviceEnumerator *DeviceEnumerator = NULL;
            IMMDevice *DefaultAudioDevice = NULL;
            IAudioClient *AudioClient = NULL;
            IAudioRenderClient *AudioRenderClient = NULL;

            CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
                             __uuidof(IMMDeviceEnumerator), (void**)(&DeviceEnumerator));

            Assert(DeviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &DefaultAudioDevice) == S_OK);

            Assert(DefaultAudioDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&AudioClient) == S_OK);

            DWORD NumChannels = 2;
            DWORD SamplesPerSecond = 48000;
            WORD BitsPerMonoSample = 16;

            WAVEFORMATEX WaveFormat = {};
            WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
            WaveFormat.nChannels = (WORD)NumChannels;
            WaveFormat.nSamplesPerSec = SamplesPerSecond;
            WaveFormat.wBitsPerSample = BitsPerMonoSample;
            WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
            WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;
            WaveFormat.cbSize = 0;

#define REFERENCE_TIME_TO_MS(ReferenceTime) ((ReferenceTime) / 10000)
#define REFERENCE_TIME_FROM_MS(Milliseconds) ((Milliseconds) * 10000)
            REFERENCE_TIME SoundOutputBufferDuration = REFERENCE_TIME_FROM_MS(1000);

            WAVEFORMATEX *ClosestFormat;
            HRESULT DesiredFormatSupported = AudioClient->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, 
                                                                              &WaveFormat, &ClosestFormat);
            if (DesiredFormatSupported == S_OK)
            {
                Assert(AudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
                    0,
                    SoundOutputBufferDuration, 0, &WaveFormat, NULL) == S_OK);
            }
            else
            {
                // https://matthewvaneerde.wordpress.com/2017/10/17/how-to-negotiate-an-audio-format-for-a-windows-audio-session-api-wasapi-client/
                Assert(AudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 
                    AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY, 
                    SoundOutputBufferDuration, 0, &WaveFormat, NULL) == S_OK);
            }

            Assert(AudioClient->GetService(__uuidof(IAudioRenderClient), (void**)&AudioRenderClient) == S_OK);

            UINT32 SoundOutputBufferFrameCount;
            AudioClient->GetBufferSize(&SoundOutputBufferFrameCount);
            DWORD SoundOutputBufferBytesPerFrame = NumChannels * (BitsPerMonoSample / 8);
            DWORD SoundOutputBufferByteSize = SoundOutputBufferFrameCount * SoundOutputBufferBytesPerFrame;

            Assert(AudioClient->Start() == S_OK);

            GlobalRunning = true;

            // TODO(Kevin): Pool with bitmap VirtualAlloc
            int16 *Samples = (int16 *)VirtualAlloc(0, SoundOutputBufferByteSize, 
                                                   MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

#if HANDMADE_INTERNAL
            LPVOID BaseAddress = (LPVOID)Terabytes(2);
#else
            LPVOID BaseAddress = 0;
#endif
            game_memory GameMemory = {};
            GameMemory.PermanentStorageSize = Megabytes(64);
            GameMemory.TransientStorageSize = Gigabytes(1);
            GameMemory.DEBUGPlatformFreeFileMemory = DEBUGPlatformFreeFileMemory;
            GameMemory.DEBUGPlatformReadEntireFile = DEBUGPlatformReadEntireFile;
            GameMemory.DEBUGPlatformWriteEntireFile = DEBUGPlatformWriteEntireFile;

            // TODO(Kevin): Use MEM_LARGE_PAGES and call AdjustTokenPrivileges
            // TODO(Kevin): TransientStorage needs to be broken up into game
            // transient and cache transient, and only the former needs to be
            // saved for state playback.
            Win32State.GameMemoryTotalSize = GameMemory.PermanentStorageSize + GameMemory.TransientStorageSize;
            Win32State.GameMemoryBlock = VirtualAlloc(BaseAddress, (size_t)Win32State.GameMemoryTotalSize, 
                                                      MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
            GameMemory.PermanentStorage = Win32State.GameMemoryBlock;
            GameMemory.TransientStorage = ((uint8 *)GameMemory.PermanentStorage + 
                                           GameMemory.PermanentStorageSize);

            if (Samples && GameMemory.PermanentStorage)
            {
                game_input Input[2] = {};
                game_input *NewInput = &Input[0];
                game_input *OldInput = &Input[1];

                LARGE_INTEGER LastCounter = Win32GetWallClock();

                win32_game_code Game = Win32LoadGameCode(SourceGameCodeDLLFullPath,
                                                         TempGameCodeDLLFullPath,
                                                         GameCodeLockFullPath);

                while (GlobalRunning)
                {
                    FILETIME NewDLLWriteTime =  Win32GetLastWriteTime(SourceGameCodeDLLFullPath);
                    if (CompareFileTime(&NewDLLWriteTime, &Game.DLLLastWriteTime) > 0)
                    {
                        Win32UnloadGameCode(&Game);
                        Game = Win32LoadGameCode(SourceGameCodeDLLFullPath,
                                                 TempGameCodeDLLFullPath,
                                                 GameCodeLockFullPath);
                    }

                    NewInput->dtForFrame = TargetSecondsPerFrame;

                    // TODO(Kevin): Zeroing macro
                    game_controller_input *OldKeyboardController = GetController(OldInput, 0);
                    game_controller_input *NewKeyboardController = GetController(NewInput, 0);
                    *NewKeyboardController = {};
                    NewKeyboardController->IsConnected = true;
                    Win32CopyKeyboardEndedDownStates(NewKeyboardController, 
                                                     OldKeyboardController);

                    Win32ProcessPendingMessages(&Win32State, NewKeyboardController);

                    POINT MouseP;
                    // TODO(Kevin): Handle failure case
                    GetCursorPos(&MouseP);
                    // TODO(Kevin): Handle failure case
                    ScreenToClient(Window, &MouseP);
                    NewInput->MouseX = MouseP.x;
                    NewInput->MouseY = MouseP.y;
                    NewInput->MouseZ = 0; // TODO(Kevin): Support mousewheel
                    Win32ProcessKeyboardMessage(&NewInput->MouseButtons[0],
                        GetKeyState(VK_LBUTTON) & (1 << 15));
                    Win32ProcessKeyboardMessage(&NewInput->MouseButtons[1],
                        GetKeyState(VK_RBUTTON) & (1 << 15));
                    Win32ProcessKeyboardMessage(&NewInput->MouseButtons[2],
                        GetKeyState(VK_MBUTTON) & (1 << 15));
                    Win32ProcessKeyboardMessage(&NewInput->MouseButtons[3],
                        GetKeyState(VK_XBUTTON1) & (1 << 15));
                    Win32ProcessKeyboardMessage(&NewInput->MouseButtons[4],
                        GetKeyState(VK_XBUTTON2) & (1 << 15));

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
                            NewController->IsAnalog = OldController->IsAnalog;

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

                    thread_context Thread = {};

                    game_offscreen_buffer BackBuffer = {};
                    BackBuffer.Memory = GlobalBackbuffer.Memory;
                    BackBuffer.Width = GlobalBackbuffer.Width;
                    BackBuffer.Height = GlobalBackbuffer.Height;
                    BackBuffer.Pitch = GlobalBackbuffer.Pitch;
                    BackBuffer.BytesPerPixel = GlobalBackbuffer.BytesPerPixel;
                    
                    if (Win32State.InputRecordingIndex)
                    {
                        Win32RecordInput(&Win32State, NewInput);
                    }

                    if (Win32State.InputPlayingIndex)
                    {
                        Win32PlayBackInput(&Win32State, NewInput);
                    }

                    if (Game.UpdateAndRender)
                    {
                        Game.UpdateAndRender(&Thread, &GameMemory, NewInput, &BackBuffer);
                    }

                    // TODO(Kevin): Put audio sampling on a separate dedicated thread
                    // - runs independently of game thread
                    // - more frequent updates (lower latency)
                    UINT32 TargetSamplesPerFrame = (UINT32)((real32)SamplesPerSecond / GameUpdateHz);
                    // UINT32 DesiredLatencyInSamples = TargetSamplesPerFrame * FramesOfAudioLatency;
                    // UINT32 DesiredLatencyInSamples = UINT32((real32)TargetSamplesPerFrame * 1.2f);
                    UINT32 DesiredLatencyInSamples = (UINT32)(SamplesPerSecond / 20); // Samples/50ms
                    UINT32 SamplesLeftToPlay;
                    AudioClient->GetCurrentPadding(&SamplesLeftToPlay);

                    if (SamplesLeftToPlay < DesiredLatencyInSamples)
                    {
                        UINT32 AudioFramesToRequest = DesiredLatencyInSamples - SamplesLeftToPlay;

                        game_sound_output_buffer SoundBuffer = {};
                        SoundBuffer.SamplesPerSecond = SamplesPerSecond;
                        SoundBuffer.SampleCount = (int)AudioFramesToRequest;
                        SoundBuffer.Samples = Samples;

                        if (Game.GetSoundSamples)
                        {
                            Game.GetSoundSamples(&Thread, &GameMemory, &SoundBuffer);
                        }

                        // TODO(Kevin): Check results of Get/ReleaseBuffer
                        BYTE *WasapiSoundBuffer;
                        AudioRenderClient->GetBuffer(AudioFramesToRequest, &WasapiSoundBuffer);
                        memcpy(WasapiSoundBuffer, Samples, AudioFramesToRequest * SoundOutputBufferBytesPerFrame);
                        AudioRenderClient->ReleaseBuffer(AudioFramesToRequest, 0);
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
                        // TODO(Kevin): Logging what took so long
                        // printf("Missed frame rate!\n");
                    }

                    LARGE_INTEGER EndCounter = Win32GetWallClock();                    
                    real32 MSPerFrame = 1000.f*Win32GetSecondsElapsed(LastCounter, EndCounter);
                    LastCounter = EndCounter;

                    win32_window_dimension Dimension = Win32GetWindowDimension(Window);
#if HANDMADE_INTERNAL
                    if (Win32State.InputRecordingIndex > 0)
                    {
                        for (int X = 10; X < 20; ++X)
                        {
                            for (int Y = 10; Y < 20; ++Y)
                            {
                                uint32 *Pixel = (uint32 *)((uint8 *)GlobalBackbuffer.Memory +
                                    X*GlobalBackbuffer.BytesPerPixel +
                                    Y*GlobalBackbuffer.Pitch);
                                *Pixel = 0xFFFF0000;
                            }
                        }
                    }
                    if (Win32State.InputPlayingIndex > 0)
                    {
                        for (int X = 20; X < 30; ++X)
                        {
                            for (int Y = 10; Y < 20; ++Y)
                            {
                                uint32 *Pixel = (uint32 *)((uint8 *)GlobalBackbuffer.Memory +
                                    X*GlobalBackbuffer.BytesPerPixel +
                                    Y*GlobalBackbuffer.Pitch);
                                *Pixel = 0xFF00FF00;
                            }
                        }
                    }
#endif
                    HDC DeviceContext = GetDC(Window);
                    Win32DisplayBufferInWindow(&GlobalBackbuffer, DeviceContext,
                                               Dimension.Width, Dimension.Height);
                    ReleaseDC(Window, DeviceContext);

                    game_input *Temp = NewInput;
                    NewInput = OldInput;
                    OldInput = Temp;
                    *NewInput = {};

#if 0
                    real64 FPS = 1000.f / MSPerFrame;
                    char PrintBuffer[256];
                    sprintf(PrintBuffer, "%.02fms/f, %.02ff/s\n", MSPerFrame, FPS);
                    OutputDebugStringA(PrintBuffer);
                    // printf(PrintBuffer);
#endif
                }

                AudioRenderClient->Release();
                AudioClient->Release();
                DefaultAudioDevice->Release();
                DeviceEnumerator->Release();
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
    }
    else
    {
        // TODO(Kevin): Logging
    }

    CoUninitialize();

    return 0;
}
