#include <windows.h>

#define internal static			// static functions are internal to the translation unit
#define local_persist static 	// local static variable
#define global_static static 	// global static variable

global_static bool Running;

global_static BITMAPINFO BitmapInfo;
global_static void* BitmapMemory;
global_static HBITMAP BitmapHandle;
global_static HDC BitmapDeviceContext;

internal void
Win32ResizeDIBSection(int Width, int Height) // DIB: Device Independent Bitmap
{
	if(BitmapHandle)
	{
		DeleteObject(BitmapHandle);
	}

	if(!BitmapDeviceContext)
	{
		// TODO Should we recreate these under cretain special circumstances
		BitmapDeviceContext = CreateCompatibleDC(0);
	}

	BitmapInfo.bmiHeader.biSize = sizeof(BitmapInfo.bmiHeader);
	BitmapInfo.bmiHeader.biWidth = Width;
	BitmapInfo.bmiHeader.biHeight = Height;
	BitmapInfo.bmiHeader.biPlanes = 1;
	BitmapInfo.bmiHeader.biBitCount = 32;
	BitmapInfo.bmiHeader.biCompression = BI_RGB;
	
	BitmapHandle = CreateDIBSection(
	  BitmapDeviceContext, &BitmapInfo,
	  DIB_RGB_COLORS,
	  &BitmapMemory,
	  0, 0);
}

internal void
Win32UpdateWindow(HDC DeviceContext, int X, int Y, int Width, int Height)
{
	StretchDIBits(DeviceContext,
		X, Y, Width, Height, // to
		X, Y, Width, Height, // from
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
			Win32UpdateWindow(DeviceContext, X, Y, Width, Height);
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
  		HWND WindowHandle = CreateWindowEx(
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
	    if(WindowHandle)
	    {
	    	Running = true;
	    	while(Running)
	    	{
	    		MSG Message;
	    		BOOL MessageResult = GetMessage(&Message, 0, 0, 0);
	    		if(MessageResult > 0)
	    		{
	    			TranslateMessage(&Message);
	    			DispatchMessage(&Message);
	    		}
	    		else
	    		{
	    			break;
	    		}
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