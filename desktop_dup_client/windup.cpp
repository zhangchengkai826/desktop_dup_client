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

SOCKET workingSocket;
sockaddr_in serverAddr;

int clientWidth, clientHeight;
int serverWidth = 2736, serverHeight = 1824;
float clientServerRatio;
//int serverWidth = 1920, serverHeight = 1080;

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

class FrameUpdateData {
public:
	std::vector<uint8_t> header;
	std::vector<uint8_t> metaData;
	std::vector<uint8_t> buf;
} fud;

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                      _In_opt_ HINSTANCE hPrevInstance,
                      _In_ LPWSTR lpCmdLine,
                      _In_ int    nCmdShow) {
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	
	// Initialize global strings
	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_WINDUP, szWindowClass, MAX_LOADSTRING);

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

	InetPton(AF_INET, TEXT("10.129.2.217"), &serverAddr.sin_addr.s_addr);
	serverAddr.sin_port = htons(3300);
	serverAddr.sin_family = AF_INET;
	if (connect(workingSocket, (sockaddr*)& serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
		OutputDebugStringA(tsf::fmt("connect error, code: %d\n", WSAGetLastError()).c_str());
		return 1;
	}

	SetupDpiAwareness();

	MyRegisterClass(hInstance);
	HWND wnd = InitMainWindow(hInstance, nCmdShow);

	fud.header.resize(sizeof(UINT));
	fud.buf.resize((long long)serverWidth * serverHeight * 4);

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
			RecvNBytes(fud.header.data(), (int)fud.header.size());
			InvalidateRect(wnd, NULL, FALSE);
		}
	}

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
		clientServerRatio = (float)clientWidth / serverWidth;
		hWnd = CreateWindowW(szWindowClass, szTitle, WS_POPUP | WS_VISIBLE,
			0, (screenHeight - clientHeight) / 2, clientWidth, clientHeight, NULL, NULL, hInstance, NULL);
	}
	else {
		clientWidth = (int)((float)screenHeight / serverHeight * serverWidth);
		clientHeight = screenHeight;
		hWnd = CreateWindowW(szWindowClass, szTitle, WS_POPUP | WS_VISIBLE,
			(screenWidth - clientWidth) / 2, 0, clientWidth, clientHeight, NULL, NULL, hInstance, NULL);
		clientServerRatio = (float)clientHeight / serverHeight;
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
		HDC     srcDC = CreateCompatibleDC(hdc);
		if (srcDC == NULL) {
			throw std::runtime_error("CreateCompatibleDC failed");
		}

		if (fud.header.size() != 0) {
			auto metaSize = *(UINT*)fud.header.data();
			if (metaSize > 0) {
				fud.metaData.resize(metaSize);
				RecvNBytes(fud.metaData.data(), (int)fud.metaData.size());

				auto numMoveRects = (UINT)fud.metaData[0];
				if (numMoveRects > 0) {
					const auto* r = (DXGI_OUTDUPL_MOVE_RECT*)(fud.metaData.data() + 1);
					for (int i = 0; i < (int)numMoveRects; i++) {
						const auto& dr = r[i].DestinationRect;
						const auto& sp = r[i].SourcePoint;
						auto w = dr.right - dr.left;
						auto h = dr.bottom - dr.top;
						BitBlt(hdc, (int)((float)dr.left * clientServerRatio), (int)((float)dr.top * clientServerRatio), (int)((float)w * clientServerRatio), (int)((float)h * clientServerRatio), hdc, (int)((float)sp.x * clientServerRatio), (int)((float)sp.y * clientServerRatio), SRCCOPY);
					}
				}

				auto szMoveRects = numMoveRects * sizeof(DXGI_OUTDUPL_MOVE_RECT);
				auto numDirtyRects = (UINT)fud.metaData[1 + szMoveRects];
				if (numDirtyRects > 0) {
					const auto* r = (RECT*)(fud.metaData.data() + 1 + szMoveRects + 1);
					for (int i = 0; i < (int)numDirtyRects; i++) {
						auto w = r[i].right - r[i].left;
						auto h = r[i].bottom - r[i].top;
						RecvNBytes(fud.buf.data(), w * h * 4);

						BITMAPINFO  inf;
						memset(&inf, 0, sizeof(inf));
						inf.bmiHeader.biSize = sizeof(inf.bmiHeader);
						inf.bmiHeader.biWidth = w;
						inf.bmiHeader.biHeight = -h;
						inf.bmiHeader.biPlanes = 1;
						inf.bmiHeader.biBitCount = 32;
						inf.bmiHeader.biCompression = BI_RGB;
						void* bits = nullptr;
						HBITMAP dib = CreateDIBSection(hdc, &inf, 0, &bits, nullptr, 0);
						if (dib == NULL) {
							throw std::runtime_error("CreateDIBSection failed");
						}
						memcpy(bits, fud.buf.data(), w * h * 4);
						SelectObject(srcDC, dib);
						StretchBlt(hdc, (int)((float)r[i].left * clientServerRatio), (int)((float)r[i].top * clientServerRatio), (int)((float)w * clientServerRatio), (int)((float)h * clientServerRatio), srcDC, 0, 0, w, h, SRCCOPY);
						DeleteObject(dib);
					}
				}
			}
		}
		
		DeleteObject(srcDC);
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
