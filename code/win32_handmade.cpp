#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <xinput.h>
#include <dsound.h>

#define internal static         // static functions are internal to the translation unit
#define local_persist static    // local static variable
#define global_variable static    // global static variable

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;
typedef int32 bool32;

struct win32_offscreen_buffer
{
    BITMAPINFO Info;
    void* Memory;
    int Width;
    int Height;
    int Pitch;
    int BytesPerPixel;
};

struct win32_window_dimension
{
    int Width;
    int Height;
};

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
RenderWeirdGradient(win32_offscreen_buffer *Buffer, int XOffset, int YOffset)
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
//          OutputDebugStringA("default\n");
            Result = DefWindowProc(Window, Message, WParam, LParam);
        } break;
    }

    return Result;
}

// Win32 Entry Point: hInstance - handle to the current instance of the application
// Entered from C Runtime Library
int CALLBACK 
WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int ShowCode)
{
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

            // NOTE(Kevin): Graphics test
            int XOffset = 0;
            int YOffset = 0;

            // NOTE(Kevin): Sound test
            int SamplesPerSecond = 48000;
            int ToneHz = 256;
            int16 ToneVolume = 2000;
            uint32 RunningSampleIndex = 0;
            int SquareWavePeriod = SamplesPerSecond/ToneHz; // samples per cycle
            int HalfSquareWavePeriod = SquareWavePeriod/2;
            int BytesPerSample = sizeof(int16)*2;
            int SecondaryBufferSize = SamplesPerSecond*BytesPerSample;

            Win32InitDirectSound(Window, SamplesPerSecond, SecondaryBufferSize);
            GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

            GlobalRunning = true;
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
                for (DWORD ControllerIndex = 0;
                    ControllerIndex < XUSER_MAX_COUNT;
                    ++ControllerIndex)
                {
                    XINPUT_STATE ControllerState;
                    if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
                    {
                        // NOTE(Kevin): The controller is plugged in
                        // see if ControllerState.dwPacketNumber increments too rapidly
                        XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

                        bool Up = Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
                        bool Down = Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
                        bool Left = Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
                        bool Right = Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
                        bool Start = Pad->wButtons & XINPUT_GAMEPAD_START;
                        bool Back = Pad->wButtons & XINPUT_GAMEPAD_BACK;
                        bool LThumbButton = Pad->wButtons & XINPUT_GAMEPAD_LEFT_THUMB;
                        bool RThumbButton = Pad->wButtons & XINPUT_GAMEPAD_RIGHT_THUMB;
                        bool LShoulderButton = Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER;
                        bool RShoulderButton = Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER;
                        bool AButton = Pad->wButtons & XINPUT_GAMEPAD_A;
                        bool BButton = Pad->wButtons & XINPUT_GAMEPAD_B;
                        bool XButton = Pad->wButtons & XINPUT_GAMEPAD_X;
                        bool YButton = Pad->wButtons & XINPUT_GAMEPAD_Y;

                        uint8 LTrigger = Pad->bLeftTrigger;
                        uint8 RTrigger = Pad->bRightTrigger;

                        int16 LThumbStickX = Pad->sThumbLX;
                        int16 LThumbStickY = Pad->sThumbLY;
                        int16 RThumbStickX = Pad->sThumbRX;
                        int16 RThumbStickY = Pad->sThumbRY;

                        if (AButton)
                        {
                            ++YOffset;
                            XINPUT_VIBRATION ControllerVibration;
                            ControllerVibration.wLeftMotorSpeed = 60000;
                            ControllerVibration.wRightMotorSpeed = 60000;
                            XInputSetState(ControllerIndex, &ControllerVibration);
                        }
                        else
                        {
                            XINPUT_VIBRATION ControllerVibration;
                            ControllerVibration.wLeftMotorSpeed = 0;
                            ControllerVibration.wRightMotorSpeed = 0;
                            XInputSetState(ControllerIndex, &ControllerVibration);
                        }
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

                RenderWeirdGradient(&GlobalBackbuffer, XOffset, YOffset);

                // NOTE(Kevin): DirectSound output test
                DWORD PlayCursor;
                DWORD WriteCursor;
                if (SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor)))
                {
                    DWORD ByteToLock = (RunningSampleIndex * BytesPerSample) % SecondaryBufferSize;
                    DWORD BytesToWrite;
                    if (ByteToLock > PlayCursor)
                    {
                        BytesToWrite = (SecondaryBufferSize - ByteToLock);
                        BytesToWrite += PlayCursor;
                    }
                    else
                    {
                        BytesToWrite = PlayCursor - ByteToLock;
                    }

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
                        DWORD Region1SampleCount = Region1Size/BytesPerSample;
                        int16 *SampleOut = (int16 *)Region1;
                        for (DWORD SampleIndex = 0;
                            SampleIndex < Region1SampleCount;
                            ++SampleIndex)
                        {
                            int16 SampleValue = ((RunningSampleIndex++ / HalfSquareWavePeriod) % 2) ? ToneVolume : -ToneVolume;
                            *SampleOut++ = SampleValue;
                            *SampleOut++ = SampleValue;
                        }

                        DWORD Region2SampleCount = Region2Size/BytesPerSample;
                        SampleOut = (int16 *)Region2;
                        for (DWORD SampleIndex = 0;
                            SampleIndex < Region2SampleCount;
                            ++SampleIndex)
                        {
                            int16 SampleValue = ((RunningSampleIndex++ / HalfSquareWavePeriod) % 2) ? ToneVolume : -ToneVolume;
                            *SampleOut++ = SampleValue;
                            *SampleOut++ = SampleValue;
                        }

                        GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
                    }
                }

                win32_window_dimension Dimension = Win32GetWindowDimension(Window);
                Win32DisplayBufferInWindow(&GlobalBackbuffer, DeviceContext,
                    Dimension.Width, Dimension.Height);

                ++XOffset;
            }

            ReleaseDC(Window, DeviceContext);
        }
        else
        {
            // TODO: error logging
        }
    }
    else
    {
        // TODO: error logging
    }

    return 0;
}
