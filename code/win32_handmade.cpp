#include <windows.h>
#include <stdint.h>
#include <stdio.h>

#define internal static         // static functions are internal to the translation unit
#define local_persist static    // local static variable
#define global_static static    // global static variable

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

struct win32_offscreen_buffer
{
    BITMAPINFO Info;
    void* Memory;
    int Width;
    int Height;
    int Pitch;
    int BytesPerPixel;
};

global_static bool GlobalRunning;
global_static win32_offscreen_buffer GlobalBackbuffer;

void OpenConsole()
{
    AllocConsole();                          // Allocates a new console
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);  // Redirect stdout to the new console
    freopen_s(&fp, "CONOUT$", "w", stderr);  // Redirect stderr (optional)
    freopen_s(&fp, "CONIN$", "r", stdin);    // Redirect stdin (optional)
}

struct win32_window_dimension
{
    int Width;
    int Height;
};

win32_window_dimension
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
RenderWeirdGradient(win32_offscreen_buffer Buffer, int XOffset, int YOffset)
{
    uint8 *Row = (uint8 *)Buffer.Memory;
    for (int Y = 0; Y < Buffer.Height; ++Y)
    {
        uint32 *Pixel = (uint32 *)Row;
        for (int X = 0; X < Buffer.Width; ++X)
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

        Row += Buffer.Pitch;
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
    Buffer->Memory = VirtualAlloc(NULL, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);

    Buffer->Pitch = Width * Buffer->BytesPerPixel;

    // TODO(Kevin): Probably clear to black
}

internal void
Win32DisplayBufferInWindow(HDC DeviceContext, int WindowWidth, int WindowHeight, 
    win32_offscreen_buffer Buffer, int X, int Y, int Width, int Height)
{
    // TODO(Kevin): Aspect ratio correction
    StretchDIBits(DeviceContext,
        /*
        X, Y, Width, Height, // dest
        X, Y, Width, Height, // src
        */
        0, 0, WindowWidth, WindowHeight,
        0, 0, Buffer.Width, Buffer.Height,
        Buffer.Memory,
        &Buffer.Info,
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

        case WM_PAINT:
        {
            PAINTSTRUCT Paint;
            HDC DeviceContext = BeginPaint(Window, &Paint);
            int X = Paint.rcPaint.left;
            int Y = Paint.rcPaint.top;
            int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;
            int Width = Paint.rcPaint.right - Paint.rcPaint.left;

            win32_window_dimension Dimension = Win32GetWindowDimension(Window);
            Win32DisplayBufferInWindow(DeviceContext, Dimension.Width, Dimension.Height, 
                GlobalBackbuffer, X, Y, Width, Height);
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
    Win32ResizeDIBSection(&GlobalBackbuffer, 1280, 720);

    WNDCLASS WindowClass = {};
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
        if(Window)
        {
            // NOTE(Kevin): Since we specified CS_OWNDC, we can just
            // get one device context and use it forever because we
            // are not sharing it with anyone.
            HDC DeviceContext = GetDC(Window);

            int XOffset = 0;
            int YOffset = 0;

            GlobalRunning = true;
            while(GlobalRunning)
            {
                MSG Message;
                while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
                {
                    if (Message.message == WM_QUIT)
                    {
                        GlobalRunning = false;
                    }

                    TranslateMessage(&Message);
                    DispatchMessage(&Message);
                }

                RenderWeirdGradient(GlobalBackbuffer, XOffset, YOffset);

                win32_window_dimension Dimension = Win32GetWindowDimension(Window);
                Win32DisplayBufferInWindow(DeviceContext, Dimension.Width, Dimension.Height, 
                    GlobalBackbuffer, 0, 0, Dimension.Width, Dimension.Height);
                ++XOffset;
                ++YOffset;
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