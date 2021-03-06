#include <windows.h>
#include <strsafe.h>
#include <stdint.h>
#include <stdio.h>
#include <xinput.h>
#include <dsound.h>
#include <math.h>

#define internal static
#define local_persist static
#define global_variable static

#define Pi32 3.14159265359

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef float real32;
typedef double real64;

typedef int32 bool32;

#include "handmade.cpp"

struct win32_offscreen_buffer
{
    BITMAPINFO Info;
    void *Memory;
    int Width;
    int Height;
    int BytesPerPixel;
};

struct win32_window_dimension
{
    int Width;
    int Height;
};

struct win32_sound_buffer_info
{
	bool32 SoundIsPlaying;
	int SamplesPerSecond;
	int BytesPerSample;
	int ToneHz;
	int16 ToneVolume;
	uint32 RunningSampleIndex;
	int WavePeriod;
	int BufferSize;
	real32 tSine;
	int LatencySampleCount;
	LPDIRECTSOUNDBUFFER DirectSoundBuffer;
};

global_variable int GlobalXOffset = 0;
global_variable int GlobalYOffset = 0;

global_variable bool32 GlobalRunning;
global_variable win32_offscreen_buffer GlobalBackbuffer;
global_variable win32_sound_buffer_info GlobalSoundBuffer;

#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);

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


#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

internal void
Win32LoadXInput(void)
{
    HMODULE XInputLibrary = LoadLibrary("xinput1_3.dll");
    if (!XInputLibrary)
    {
        XInputLibrary = LoadLibrary("xinput1_4.dll");
    }

    if(XInputLibrary)
    {
        XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
        XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
    }
}

void Win32ErrorExit(LPTSTR lpszFunction) 
{ 
    // Retrieve the system error message for the last-error code

    LPVOID lpMsgBuf;
    LPVOID lpDisplayBuf;
    DWORD dw = GetLastError(); 

    FormatMessage(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | 
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            dw,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPTSTR) &lpMsgBuf,
            0, NULL );

    // Display the error message and exit the process

lpDisplayBuf = (LPVOID)LocalAlloc(LMEM_ZEROINIT, 
        (lstrlen((LPCTSTR)lpMsgBuf) + lstrlen((LPCTSTR)lpszFunction) + 40) * sizeof(TCHAR)); 
    StringCchPrintf((LPTSTR)lpDisplayBuf, 
            LocalSize(lpDisplayBuf) / sizeof(TCHAR),
            TEXT("%s failed with error %d: %s"), 
            lpszFunction, dw, lpMsgBuf); 
    MessageBox(NULL, (LPCTSTR)lpDisplayBuf, TEXT("Error"), MB_OK); 

    LocalFree(lpMsgBuf);
    LocalFree(lpDisplayBuf);

    ExitProcess(dw); 
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
Win32ResizeDIBSection(win32_offscreen_buffer *Buffer,int Width, int Height)
{
    if (Buffer->Memory)
    {
        VirtualFree(Buffer->Memory, 0, MEM_RELEASE); 
    }

    
    Buffer->BytesPerPixel = 4;
    Buffer->Width = 1200;//Width;
    Buffer->Height = 700;//Height;

    Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
    Buffer->Info.bmiHeader.biWidth = Buffer->Width;
    Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
    Buffer->Info.bmiHeader.biPlanes = 1;
    Buffer->Info.bmiHeader.biBitCount = 32;
    Buffer->Info.bmiHeader.biCompression = BI_RGB;
    
    int BitmapMemorySize = Buffer->BytesPerPixel * (Buffer->Width * Buffer->Height);
    Buffer->Memory = VirtualAlloc(0,  BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
}

internal void Win32DisplayBufferInWindow(HWND WindowHandle,
										 win32_offscreen_buffer *Buffer)
{
	win32_window_dimension WindowDimension = Win32GetWindowDimension(WindowHandle);
	HDC DeviceContext = GetDC(WindowHandle);

    StretchDIBits(DeviceContext,
            0, 0, WindowDimension.Width, WindowDimension.Height,
            0, 0, Buffer->Width, Buffer->Height,
            Buffer->Memory,
            &Buffer->Info,
             DIB_RGB_COLORS,
            SRCCOPY);
}

internal void
Win32HandleMessages()
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
}

LRESULT CALLBACK 
Win32MainWindowCallback(
  HWND   Window,
  UINT   Message,
  WPARAM WParam,
  LPARAM LParam)
{
    LRESULT Result = 0;
    switch(Message)
    {
        case WM_SIZE:
        {
            win32_window_dimension WindowDimension = Win32GetWindowDimension(Window);
            Win32ResizeDIBSection(&GlobalBackbuffer, WindowDimension.Width, WindowDimension.Height);

            OutputDebugStringA("WM_SIZE\n");
        } break;
        
        case WM_DESTROY:
        {
            GlobalRunning = false;
            OutputDebugStringA("WM_DESTROY\n");
        } break;
        
        case WM_CLOSE:
        {
            GlobalRunning = false;
            PostQuitMessage(0);
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
            Win32DisplayBufferInWindow(Window, &GlobalBackbuffer);
        } break;

        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            uint32 VKCode = WParam;
            bool32 WasDown = ((LParam & (1<<30)) != 0);
            bool32 IsDown = ((LParam & (1<<31)) == 0);

			static bool32 AltWasDown = ((LParam & (1 << 29)) != 0);
			if ((VKCode == VK_F4) && AltWasDown)
			{
				GlobalRunning = false;
			} 

            //if (WasDown != IsDown)
            //{
                if (VKCode == 'W')
                {
                }

                if (VKCode == 'A')
                {
                }

                if (VKCode == 'S')
                {
                }

                if (VKCode == 'D')
                {
                }

                if (VKCode == 'Q')
                {
                }

                if (VKCode == 'E')
                {
                }

                if (VKCode == VK_UP)
                {
					GlobalYOffset -= 30;
                }

                if (VKCode == VK_DOWN)
                {
					GlobalYOffset += 30;
                }

                if (VKCode == VK_LEFT)
                {
					GlobalXOffset -= 30;

                }

                if (VKCode == VK_RIGHT)
                {
					GlobalXOffset += 30;
                }

                if (VKCode == VK_ESCAPE)
                {
                }

                if (VKCode == VK_SPACE)
                {
                }
            //}

        } break;
        
        default:
        {
            //OutputDebugStringA("Other message\n");
            Result = DefWindowProc(Window, Message, WParam, LParam);
        } break;

    }
    return Result;

}

internal void 
Win32HandleXInput()
{
	// Do XInput stuff:
	for (DWORD i = 0; i< XUSER_MAX_COUNT; i++)
	{
		XINPUT_STATE state = {};
		if (XInputGetState(i, &state) == ERROR_SUCCESS)
		{
			XINPUT_GAMEPAD *GamePad = &state.Gamepad;

			bool32 Up = GamePad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
			bool32 Down = GamePad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
			bool32 Left = GamePad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
			bool32 Right = GamePad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
			bool32 Start = GamePad->wButtons & XINPUT_GAMEPAD_START;
			bool32 Back = GamePad->wButtons & XINPUT_GAMEPAD_BACK;
			bool32 LeftThumb = GamePad->wButtons & XINPUT_GAMEPAD_LEFT_THUMB;
			bool32 RightThumb = GamePad->wButtons & XINPUT_GAMEPAD_RIGHT_THUMB;
			bool32 LeftShoulder = GamePad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER;
			bool32 RightShoulder = GamePad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER;
			bool32 ButtonA = GamePad->wButtons & XINPUT_GAMEPAD_A;
			bool32 ButtonB = GamePad->wButtons & XINPUT_GAMEPAD_B;
			bool32 ButtonX = GamePad->wButtons & XINPUT_GAMEPAD_X;
			bool32 ButtonY = GamePad->wButtons & XINPUT_GAMEPAD_Y;

			uint8 LeftAnalog = GamePad->bLeftTrigger;
			uint8 RightAnalog = GamePad->bRightTrigger;

			int16 LeftThumbX = GamePad->sThumbLX;
			int16 LeftThumbY = GamePad->sThumbLY;
			printf("Left X:%d Y:%d", LeftThumbX, LeftThumbY);

			GlobalXOffset += LeftThumbX / 4000;
			GlobalYOffset += -LeftThumbY / 4000;

			GlobalSoundBuffer.ToneHz = 512 + (int)(256.0f*((real32)LeftThumbY / 30000.0f));
			GlobalSoundBuffer.ToneHz = GlobalSoundBuffer.ToneHz == 0 ? 10 : GlobalSoundBuffer.ToneHz;
			GlobalSoundBuffer.WavePeriod = GlobalSoundBuffer.SamplesPerSecond / GlobalSoundBuffer.ToneHz;

			int16 RightThumbX = GamePad->sThumbRX;
			int16 RightThumbY = GamePad->sThumbRY;
			printf("Right X:%d Y:%d", RightThumbX, RightThumbY);

			if (Up)
			{
				OutputDebugString("Up");
			}

			if (Down)
			{
				OutputDebugString("Down");
			}

			if (Left)
			{
				OutputDebugString("Left");
			}

			if (Right)
			{
				OutputDebugString("Right");
			}

			if (Start)
			{
				OutputDebugString("Start");
			}

			if (Back)
			{
				OutputDebugString("Back");
			}

			if (LeftThumb)
			{
				OutputDebugString("LeftThumb");
			}

			if (RightThumb)
			{
				OutputDebugString("RightThumb");
			}

			if (LeftShoulder)
			{
				OutputDebugString("LeftShoulder");
			}

			if (RightShoulder)
			{
				OutputDebugString("RightShoulder");
			}

			if (ButtonA)
			{
				OutputDebugString("ButtonA");
			}

			if (ButtonB)
			{
				OutputDebugString("ButtonB");
			}

			if (ButtonX)
			{
				OutputDebugString("ButtonX");
			}

			if (ButtonY)
			{
				OutputDebugString("ButtonY");
			}
			// Controller is connected 
		}
		else
		{
			// Controller is not connected 
		}
	}
}

internal void
Win32InitDSound(HWND Window, win32_sound_buffer_info *SoundBuffer);
internal void
Win32FillSoundBuffer(win32_sound_buffer_info *SoundBuffer, DWORD ByteToLock, DWORD BytesToWrite, game_sound_output_buffer *SourceBuffer);

internal void
Win32ClearSoundBuffer(win32_sound_buffer_info *SoundBuffer)
{
	VOID *Region1;
	DWORD Region1Size;
	VOID *Region2;
	DWORD Region2Size;
	if (SUCCEEDED(SoundBuffer->DirectSoundBuffer->Lock(0, SoundBuffer->BufferSize, &Region1, &Region1Size, &Region2, &Region2Size, 0)))
	{
		uint8* DestSample = (uint8*)Region1;
		for (DWORD ByteIndex = 0; ByteIndex < Region1Size; ByteIndex++)
		{
			*DestSample++ = 0;
		}

		DestSample = (uint8*)Region2;
		for (DWORD ByteIndex = 0; ByteIndex < Region2Size; ByteIndex++)
		{
			*DestSample++ = 0;

		}
	}

	GlobalSoundBuffer.DirectSoundBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
}

internal void
Win32SetupSound(HWND WindowHandle)
{
	GlobalSoundBuffer = {};

	GlobalSoundBuffer.SamplesPerSecond = 48000;
	GlobalSoundBuffer.BytesPerSample = sizeof(int16) * 2;
	GlobalSoundBuffer.ToneHz = 512;
	GlobalSoundBuffer.ToneVolume = 5000;
	GlobalSoundBuffer.RunningSampleIndex = 0;
	GlobalSoundBuffer.WavePeriod = GlobalSoundBuffer.SamplesPerSecond / GlobalSoundBuffer.ToneHz;
	GlobalSoundBuffer.BufferSize = GlobalSoundBuffer.SamplesPerSecond * GlobalSoundBuffer.BytesPerSample;
	GlobalSoundBuffer.LatencySampleCount = (real32)GlobalSoundBuffer.SamplesPerSecond / (real32)20.0;

	Win32InitDSound(WindowHandle, &GlobalSoundBuffer);

	Win32ClearSoundBuffer(&GlobalSoundBuffer);

	GlobalSoundBuffer.DirectSoundBuffer->Play(0, 0, DSBPLAY_LOOPING);
	GlobalSoundBuffer.SoundIsPlaying = true;
}

internal void
Win32InitDSound(HWND Window, win32_sound_buffer_info *SoundBuffer)
{
	HMODULE DSoundLibrary = LoadLibrary("dsound.dll");
	if (!DSoundLibrary)
	{
		OutputDebugString("Could not load dsound.dll");
		return;
	}

	direct_sound_create *DirectSoundCreate = (direct_sound_create *)GetProcAddress(DSoundLibrary, "DirectSoundCreate");
	LPDIRECTSOUND DirectSound;
	if (!DirectSoundCreate || !SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
	{
		OutputDebugString("Function pointer not aquired or call failed.");
		return;
	}

	WAVEFORMATEX WaveFormat = {};
	WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
	WaveFormat.nChannels = 2;
	WaveFormat.nSamplesPerSec = SoundBuffer->SamplesPerSecond;
	WaveFormat.wBitsPerSample = 16;
	WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
	WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;
	WaveFormat.cbSize = 0;

	if (!SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
	{
		OutputDebugString("Could not set cooperative level.");
		return;
	}

	DSBUFFERDESC PrimaryBufferDescription = {};
	PrimaryBufferDescription.dwSize = sizeof(PrimaryBufferDescription);
	PrimaryBufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;

	LPDIRECTSOUNDBUFFER PrimaryBuffer;
	if (!SUCCEEDED(DirectSound->CreateSoundBuffer(&PrimaryBufferDescription, &PrimaryBuffer, 0)))
	{
		OutputDebugString("Could not create primary buffer.");
		return;
	}

	HRESULT SetFormatError = PrimaryBuffer->SetFormat(&WaveFormat);
	if (SUCCEEDED(SetFormatError))
	{
		OutputDebugString("Primary buffer format was set.");
	}
	else
	{
		OutputDebugString("Could not set buffer format.");
	}

	DSBUFFERDESC SecondaryBufferDescription = {};
	SecondaryBufferDescription.dwSize = sizeof(SecondaryBufferDescription);
	SecondaryBufferDescription.dwFlags = 0;
	SecondaryBufferDescription.dwBufferBytes = SoundBuffer->BufferSize;
	SecondaryBufferDescription.lpwfxFormat = &WaveFormat;

	LPDIRECTSOUNDBUFFER SecondaryBuffer;
	HRESULT CreateBufferError = DirectSound->CreateSoundBuffer(&SecondaryBufferDescription, &SecondaryBuffer, 0);
	if (SUCCEEDED(CreateBufferError))
	{
		OutputDebugString("Secondary buffer created successfully ");
		SoundBuffer->DirectSoundBuffer= SecondaryBuffer;
	}
	else
	{
		OutputDebugString("Could not create secondary buffer.");
	}
}

internal void
Win32CopyGameSoundBufferIntoGlobalSoundBuffer(game_sound_output_buffer *SoundBuffer)
{
	DWORD PlayCursor;
	DWORD WriteCursor;
	if (SUCCEEDED(GlobalSoundBuffer.DirectSoundBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor)))
	{
		DWORD ByteToLock = ((GlobalSoundBuffer.RunningSampleIndex*GlobalSoundBuffer.BytesPerSample) %
			GlobalSoundBuffer.BufferSize);


		DWORD TargetCursor =
			((PlayCursor +
			(GlobalSoundBuffer.LatencySampleCount*GlobalSoundBuffer.BytesPerSample)) %
				GlobalSoundBuffer.BufferSize);
		DWORD BytesToWrite;
		if (ByteToLock > TargetCursor)
		{
			BytesToWrite = (GlobalSoundBuffer.BufferSize - ByteToLock);
			BytesToWrite += TargetCursor;
		}
		else 
		{
			BytesToWrite = TargetCursor - ByteToLock;
		}

		Win32FillSoundBuffer(&GlobalSoundBuffer, ByteToLock, BytesToWrite, SoundBuffer);
	}
}

internal void
Win32FillSoundBuffer(win32_sound_buffer_info *SoundBuffer, DWORD ByteToLock, DWORD BytesToWrite, game_sound_output_buffer *SourceBuffer)
{
	VOID *Region1;
	DWORD Region1Size;
	VOID *Region2;
	DWORD Region2Size;

	if (SUCCEEDED(SoundBuffer->DirectSoundBuffer->Lock(ByteToLock, BytesToWrite, &Region1, &Region1Size, &Region2, &Region2Size, 0)))
	{
		DWORD Region1SampleCount = Region1Size / SoundBuffer->BytesPerSample;
		int16 *DestSample = (int16 *)Region1;
		int16 *SourceSample = SourceBuffer->Samples;
		for (DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; SampleIndex++)
		{
			*DestSample++ = *SourceSample++;
			*DestSample++ = *SourceSample++;
			SoundBuffer->RunningSampleIndex++;
		}

		DestSample = (int16 *)Region2;
		DWORD Region2SampleCount = Region2Size / SoundBuffer->BytesPerSample;
		for (DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; SampleIndex++)
		{
			*DestSample++ = *SourceSample++;
			*DestSample++ = *SourceSample++;
			SoundBuffer->RunningSampleIndex++;
		}
	}


	GlobalSoundBuffer.DirectSoundBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
}

int CALLBACK
WinMain(HINSTANCE Instance,
    HINSTANCE PrevInstance,
    LPSTR CommandLine,
    int ShowCode)
{
	LARGE_INTEGER PerfCountFrequencyResult;
	QueryPerformanceFrequency(&PerfCountFrequencyResult);
	int64 PerfCountFrequency = PerfCountFrequencyResult.QuadPart;

    WNDCLASS WindowClass = {};
    WindowClass.style = CS_HREDRAW|CS_VREDRAW;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance = Instance;
    WindowClass.lpszClassName = "HandmadeHeroWindowClass";
    
	if (!RegisterClass(&WindowClass))
	{
		OutputDebugStringA("Registering class failed\n");
		Win32ErrorExit(TEXT("RegisterClass"));
	}

    HWND WindowHandle = CreateWindowEx(0, WindowClass.lpszClassName, "Handmade Hero", WS_OVERLAPPEDWINDOW|WS_VISIBLE, 
																						CW_USEDEFAULT, CW_USEDEFAULT,
																						900, 700,
																						0, 0, Instance, 0);
	if (!WindowHandle)
	{
		OutputDebugStringA("Creating window failed\n");
		Win32ErrorExit(TEXT("CreateWindowEx"));
	}

	Win32LoadXInput();

	Win32SetupSound(WindowHandle);

    GlobalRunning = true;

	uint64 LastCycleCount = __rdtsc();

	LARGE_INTEGER LastCounter;
	QueryPerformanceCounter(&LastCounter);

    while(GlobalRunning)
	{   
		Win32HandleMessages();

		Win32HandleXInput();
        
		game_offscreen_buffer GraphicsBuffer = {};
		GraphicsBuffer.Memory = GlobalBackbuffer.Memory;
		GraphicsBuffer.Width = GlobalBackbuffer.Width;
		GraphicsBuffer.Height = GlobalBackbuffer.Height;
		GraphicsBuffer.Pitch = GlobalBackbuffer.Width * GlobalBackbuffer.BytesPerPixel;

		int16 Samples[ 48000 ];
		game_sound_output_buffer SoundBuffer = {};
		SoundBuffer.SamplesPerSecond = GlobalSoundBuffer.SamplesPerSecond;
		SoundBuffer.SampleCount = GlobalSoundBuffer.SamplesPerSecond / 30.0;
		SoundBuffer.Samples = Samples;

		GameUpdateAndRender(&GraphicsBuffer, &SoundBuffer);

		Win32DisplayBufferInWindow(WindowHandle, &GlobalBackbuffer);

		Win32CopyGameSoundBufferIntoGlobalSoundBuffer(&SoundBuffer);

		uint64 EndCycleCount = __rdtsc();

		LARGE_INTEGER EndCounter;
		QueryPerformanceCounter(&EndCounter);

		real32 CounterElapsed = real32(EndCounter.QuadPart - LastCounter.QuadPart);
		uint64 CyclesElapsed = EndCycleCount - LastCycleCount;

		real64 MSPerFrame = (((1000.0f*(real64)CounterElapsed) / (real64)PerfCountFrequency) );
		real64 FPS = (real64)PerfCountFrequency / (real64)CounterElapsed;
		real64 MCPF = ((real64)CyclesElapsed / (1000.0f * 1000.0f));

		char Buffer[256];
		sprintf(Buffer, "%.02fms/f,  %.02ff/s,  %.02fmc/f\n", MSPerFrame, FPS, MCPF);
		OutputDebugStringA(Buffer);

		LastCounter = EndCounter;
	}

    return 0;
}
