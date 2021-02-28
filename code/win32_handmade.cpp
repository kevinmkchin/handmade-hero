#include <windows.h>
#include <stdint.h>

#define internal static			// static functions are internal to the translation unit
#define local_persist static 	// local static variable
#define global_static static 	// global static variable

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

global_static bool Running;

global_static BITMAPINFO BitmapInfo;
global_static void* BitmapMemory;
global_static int BitmapWidth;
global_static int BitmapHeight;
global_static int BytesPerPixel = 4;

internal void
RenderWeirdGradient(int XOffset, int YOffset)
{
	int Pitch = BitmapWidth * BytesPerPixel;
	uint8* Row = (uint8*)BitmapMemory;
	for (int Y = 0; Y < BitmapHeight; ++Y)
	{
		uint32* Pixel = (uint32*)Row;
		for (int X = 0; X < BitmapWidth; ++X)
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

		Row += Pitch;
	}
}


internal void
Win32ResizeDIBSection(int Width, int Height) // DIB: Device Independent Bitmap
{
	if(BitmapMemory)
	{
		VirtualFree(BitmapMemory, 0, MEM_RELEASE);
	}

	BitmapWidth = Width;
	BitmapHeight = Height;

	BitmapInfo.bmiHeader.biSize = sizeof(BitmapInfo.bmiHeader);
	BitmapInfo.bmiHeader.biWidth = BitmapWidth;
	BitmapInfo.bmiHeader.biHeight = -BitmapHeight;
	BitmapInfo.bmiHeader.biPlanes = 1;
	BitmapInfo.bmiHeader.biBitCount = 32;
	BitmapInfo.bmiHeader.biCompression = BI_RGB;
	
	int BitmapMemorySize = (Width * Height) * BytesPerPixel;
	BitmapMemory = VirtualAlloc(NULL, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);

	// TODO Clear to black
}

internal void
Win32UpdateWindow(HDC DeviceContext, RECT* ClientRect, int X, int Y, int Width, int Height)
{
	int WindowWidth = ClientRect->right - ClientRect->left;
	int WindowHeight = ClientRect->bottom - ClientRect->top;
	StretchDIBits(DeviceContext,
		/*
		X, Y, Width, Height, // to
		X, Y, Width, Height, // from
		*/
		0, 0, BitmapWidth, BitmapHeight,
		0, 0, WindowWidth, WindowHeight,
		BitmapMemory,
		&BitmapInfo,
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
			RECT ClientRect;
			GetClientRect(Window, &ClientRect);
			int Height = ClientRect.bottom - ClientRect.top;
			int Width = ClientRect.right - ClientRect.left;
			Win32ResizeDIBSection(Width, Height);
		} break;

		case WM_DESTROY:
		{
			Running = false;
		} break;

		case WM_CLOSE:
		{
			Running = false;
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

			RECT ClientRect;
			GetClientRect(Window, &ClientRect);

			Win32UpdateWindow(DeviceContext, &ClientRect, X, Y, Width, Height);
			EndPaint(Window, &Paint);
		} break;

		default:
		{
//			OutputDebugStringA("default\n");
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
	WNDCLASS WindowClass = {};
	WindowClass.style = CS_OWNDC|CS_HREDRAW|CS_VREDRAW;
	WindowClass.lpfnWndProc = Win32MainWindowCallback;
 	WindowClass.hInstance = Instance;
  	//WindowClass.hIcon = ;
  	WindowClass.lpszClassName = "HandmadeHeroWindowClass";

  	if(RegisterClass(&WindowClass))
  	{
  		HWND Window = CreateWindowEx(
	  		0,                              // Optional window styles.
		    WindowClass.lpszClassName,      // Window class
		    "Handmade Hero",    			// Window text
		    WS_OVERLAPPEDWINDOW|WS_VISIBLE, // Window style
		    // Size and position
		    CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		    NULL,       // Parent window    
		    NULL,       // Menu
		    Instance,  	// Instance handle
		    NULL);      // Additional application data
	    if(Window)
	    {
			int XOffset = 0;
			int YOffset = 0;

	    	Running = true;
	    	while(Running)
	    	{
				MSG Message;
				while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
				{
					if (Message.message == WM_QUIT)
					{
						Running = false;
					}

					TranslateMessage(&Message);
					DispatchMessage(&Message);
				}

				RenderWeirdGradient(XOffset, YOffset);

				HDC DeviceContext = GetDC(Window);
				RECT ClientRect;
				GetClientRect(Window, &ClientRect);
				int WindowWidth = ClientRect.right - ClientRect.left;
				int WindowHeight = ClientRect.bottom - ClientRect.top;
				Win32UpdateWindow(DeviceContext, &ClientRect, 0, 0, WindowWidth, WindowHeight);
				ReleaseDC(Window, DeviceContext);
				++XOffset;
	    	}
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