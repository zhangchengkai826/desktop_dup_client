// windup.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "windup.h"

#define MAX_LOADSTRING 100

// Global Variables:
WCHAR     szTitle[MAX_LOADSTRING];       // The title bar text
WCHAR     szWindowClass[MAX_LOADSTRING]; // the main window class name

// Forward declarations of functions included in this code module:
ATOM    MyRegisterClass(HINSTANCE hInstance);
HWND    InitMainWindow(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

extern "C" VOID WINAPI GdiplusShutdown(ULONG_PTR token);

BYTE ctrlStruct;

std::vector<uint8_t> Buf;
POINT pointerPos;
std::vector<uint8_t> pointerBuf;
DXGI_OUTDUPL_POINTER_SHAPE_INFO pointerInfo;

SOCKET workingSocket;
sockaddr_in serverAddr;

int clientWidth, clientHeight;
int serverWidth = 1920, serverHeight = 1080;

void RecvNBytes(void* buf, int n) {
	int remainedBytesCount = n;
	auto ptr = (uint8_t*)buf;
	while (remainedBytesCount) {
		int receivedBytesCount;
		while ((receivedBytesCount = (int)recv(workingSocket, (char*)ptr, (size_t)remainedBytesCount, 0)) == -1) {
			OutputDebugStringA(tsf::fmt("recv error, code: %d\n", WSAGetLastError()).c_str());
			if (connect(workingSocket, (sockaddr*)& serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
				throw std::runtime_error(tsf::fmt("connect error, code: %d\n", WSAGetLastError()));
			}
		}
		OutputDebugStringA(tsf::fmt("recv %d bytes\n", receivedBytesCount).c_str());
		remainedBytesCount -= receivedBytesCount;
		ptr += receivedBytesCount;
	}
}

void SetupDpiAwareness() {
	if (!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2))
		tsf::print("SetProcessDpiAwarenessContext failed\n");
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                      _In_opt_ HINSTANCE hPrevInstance,
                      _In_ LPWSTR lpCmdLine,
                      _In_ int    nCmdShow) {
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	
	// Initialize global strings
	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_WINDUP, szWindowClass, MAX_LOADSTRING);

	ShowCursor(FALSE);

	Gdiplus::GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;
	Gdiplus::Status st;
	st = GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
	if (st != Gdiplus::Status::Ok) {
		OutputDebugStringA(tsf::fmt("GdiplusStartup failed, error code: %d\n", st).c_str());
		return 1;
	}	

	WSADATA wsaData;
	auto iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		OutputDebugStringA(tsf::fmt("WSAStartup error, code: %d\n", iResult).c_str());
		return 1;
	}

	workingSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (workingSocket == INVALID_SOCKET) {
		OutputDebugStringA(tsf::fmt("socket error, code: %d\n", WSAGetLastError()).c_str());
		return 1;
	}

	InetPton(AF_INET, TEXT("192.168.137.1"), &serverAddr.sin_addr.s_addr);
	serverAddr.sin_port = htons(3300);
	serverAddr.sin_family = AF_INET;
	if (connect(workingSocket, (sockaddr*)& serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
		OutputDebugStringA(tsf::fmt("connect error, code: %d\n", WSAGetLastError()).c_str());
		return 1;
	}

	SetupDpiAwareness();

	MyRegisterClass(hInstance);
	HWND wnd = InitMainWindow(hInstance, nCmdShow);

	Buf.resize((long long)serverWidth * serverHeight * 4);

	MSG msg;
	// Main message loop:
	while (true) {
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT)
				break;
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else {
			RecvNBytes(&ctrlStruct, sizeof(ctrlStruct));

			if (ctrlStruct & 0x1) {
				RecvNBytes(Buf.data(), (int)Buf.size());
			}
			if (ctrlStruct & 0x2) {
				RecvNBytes(&pointerPos, sizeof(pointerPos));
			}
			if (ctrlStruct & 0x4) {
				UINT pointerBufSize;
				RecvNBytes(&pointerBufSize, sizeof(pointerBufSize));
				if (pointerBuf.size() < pointerBufSize)
					pointerBuf.resize(pointerBufSize);
				RecvNBytes(pointerBuf.data(), pointerBufSize);
				RecvNBytes(&pointerInfo, sizeof(pointerInfo));
			}
			
			if(ctrlStruct)
				InvalidateRect(wnd, NULL, FALSE);
		}
	}

	GdiplusShutdown(gdiplusToken);

	return (int) msg.wParam;
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance) {
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style         = 0;
	wcex.lpfnWndProc   = WndProc;
	wcex.cbClsExtra    = 0;
	wcex.cbWndExtra    = 0;
	wcex.hInstance     = hInstance;
	wcex.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_WINDUP));
	wcex.hCursor       = LoadCursor(nullptr, IDC_CROSS);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm       = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
HWND InitMainWindow(HINSTANCE hInstance, int nCmdShow) {
	auto screenWidth = GetSystemMetrics(SM_CXSCREEN);
	auto screenHeight = GetSystemMetrics(SM_CYSCREEN);

	auto screenRatio = (float)screenWidth / screenHeight;
	auto serverRatio = (float)serverWidth / serverHeight;
	HWND hWnd;
	if (screenRatio < serverRatio) {
		clientWidth = screenWidth;
		clientHeight = (int)((float)screenWidth / serverWidth * serverHeight);
		hWnd = CreateWindowW(szWindowClass, szTitle, WS_POPUP | WS_VISIBLE,
			0, (screenHeight - clientHeight) / 2, clientWidth, clientHeight, NULL, NULL, hInstance, NULL);
	}
	else {
		clientWidth = (int)((float)screenHeight / serverHeight * serverWidth);
		clientHeight = screenHeight;
		hWnd = CreateWindowW(szWindowClass, szTitle, WS_POPUP | WS_VISIBLE,
			(screenWidth - clientWidth) / 2, 0, clientWidth, clientHeight, NULL, NULL, hInstance, NULL);
	}

	if (!hWnd) {
		throw std::runtime_error(tsf::fmt("CreateWindow error, code: %d\n", GetLastError()));
	}

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	return hWnd;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
	case WM_PAINT: {
		PAINTSTRUCT ps;
		HDC         hdc = BeginPaint(hWnd, &ps);

		if (ctrlStruct & 0x1) {
			BITMAPINFO  inf;
			memset(&inf, 0, sizeof(inf));
			inf.bmiHeader.biSize = sizeof(inf.bmiHeader);
			inf.bmiHeader.biWidth = serverWidth;
			inf.bmiHeader.biHeight = -serverHeight;
			inf.bmiHeader.biPlanes = 1;
			inf.bmiHeader.biBitCount = 32;
			inf.bmiHeader.biCompression = BI_RGB;
			void* bits = nullptr;
			HDC     srcDC = CreateCompatibleDC(hdc);
			if (srcDC == NULL) {
				throw std::runtime_error("CreateCompatibleDC failed");
			}
			HBITMAP dib = CreateDIBSection(hdc, &inf, 0, &bits, nullptr, 0);
			if (dib == NULL) {
				throw std::runtime_error("CreateDIBSection failed");
			}
			memcpy(bits, Buf.data(), Buf.size());
			SelectObject(srcDC, dib);

			if (ctrlStruct & 0x2 || ctrlStruct & 0x3) {
				auto& info = pointerInfo;
				auto& h = info.Height;
				auto& w = info.Width;
				auto& p = info.Pitch;
				auto& px = pointerPos.x;
				auto& py = pointerPos.y;
				auto& b = pointerBuf;

				Gdiplus::Graphics graphics(srcDC);
				Gdiplus::Status st;

				switch (info.Type)
				{
				case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR: {
					Gdiplus::Bitmap bitmap(w, h, p, PixelFormat32bppARGB, b.data());
					st = graphics.DrawImage(&bitmap, px, py);
					if (st != Gdiplus::Status::Ok) {
						OutputDebugStringA(tsf::fmt("GdiplusStartup failed, error code: %d\n", st).c_str());
						throw std::runtime_error("");
					}
				} break;
				case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MONOCHROME:
				case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MASKED_COLOR: {
					RECT rcPointer;
					SetRect(&rcPointer, px, py, px + w, py + h / 2);
					RECT rcDesktop;
					SetRect(&rcDesktop, 0, 0, serverWidth, serverHeight);
					RECT rcIntersect;
					IntersectRect(&rcIntersect, &rcPointer, &rcDesktop);
					CopyRect(&rcPointer, &rcIntersect);
					OffsetRect(&rcPointer, -px, -py);

					if (info.Type == DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MONOCHROME) {
						for (int j = rcPointer.top, jDp = rcIntersect.top; j < rcPointer.bottom && jDp < rcIntersect.bottom; j++, jDp++) {
							for (int i = rcPointer.left, iDp = rcIntersect.left; i < rcPointer.right && iDp < rcIntersect.right; i++, iDp++) {
								BYTE Mask = 0x80 >> (i % 8);
								BYTE AndMask = b[i / 8 + p * j] & Mask;
								BYTE XorMask = b[i / 8 + p * (j + h / 2)] & Mask;

								UINT AndMask32 = (AndMask) ? 0xFFFFFFFF : 0xFF000000;
								UINT XorMask32 = (XorMask) ? 0x00FFFFFF : 0x00000000;

								DWORD* target = (DWORD*)bits + jDp * serverWidth + iDp;
								*target = ((*target) & AndMask32) ^ XorMask32;
							}
						}
					}
					else {
						UINT* pShapeBuffer32 = (UINT*)b.data();
						for (int j = rcPointer.top, jDp = rcIntersect.top; j < rcPointer.bottom && jDp < rcIntersect.bottom; j++, jDp++) {
							for (int i = rcPointer.left, iDp = rcIntersect.left; i < rcPointer.right && iDp < rcIntersect.right; i++, iDp++) {
								// Set up mask
								UINT c = pShapeBuffer32[i + (p / 4) * j];
								UINT MaskVal = 0xFF000000 & c;
								DWORD* target = (DWORD*)bits + jDp * serverWidth + iDp;
								if (MaskVal) {
									// Mask was 0xFF
									*target = ((*target) ^ c) | 0xFF000000;
								}
								else {
									// Mask was 0x00 - replacing pixel
									*target = c;
								}
							}
						}
					}
				} break;
				}
			}

			StretchBlt(hdc, 0, 0, clientWidth, clientHeight, srcDC, 0, 0, serverWidth, serverHeight, SRCCOPY);
			DeleteObject(dib);
			DeleteObject(srcDC);
		}

		EndPaint(hWnd, &ps);
	} break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}
