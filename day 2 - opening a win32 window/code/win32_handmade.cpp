
#include <windows.h>

LRESULT CALLBACK 
MainWindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
	LRESULT Result = 0;

	switch(Message)
	{
		case WM_SIZE:
		{
			OutputDebugStringA("WM_SIZE\n");
		} break;

		case WM_DESTROY:
		{
			OutputDebugStringA("WM_DESTROY\n");
		} break;

		case WM_CLOSE:
		{
			OutputDebugStringA("WM_CLOSE\n");
		} break;

		case WM_ACTIVATEAPP:
		{
			OutputDebugStringA("WM_ACTIVATEAPP\n");
		} break;

		case WM_PAINT:
		{
			PAINTSTRUCT Paint;
			HDC DeviceContext = BeginPaint(Window, &Paint);
			int X = Paint.rcPaint.left;
			int Y = Paint.rcPaint.top;
			int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;
			int Width = Paint.rcPaint.right - Paint.rcPaint.left;
			PatBlt(DeviceContext, X, Y, Width, Height, BLACKNESS);
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
	WindowClass.lpfnWndProc = MainWindowCallback;
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
	    	MSG Message;
	    	for(;;)
	    	{
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