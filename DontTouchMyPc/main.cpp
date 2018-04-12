#include <windows.h>
#include <windowsx.h>
#include <strsafe.h>
#include <debugapi.h>

#include <iostream>
#include <string>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <vector>
#include <map>

HINSTANCE g_hinst;
int cmdShow;
static HFONT lucidaFont;

typedef struct MONITOR_DATA : MONITORINFOEX {
	DEVMODE oldDeviceMode;
	bool adapter = false;
} MONITORDATA;

DISPLAY_DEVICE mainDisplayDevice;
std::vector<MONITORDATA> monitorData;
std::map<std::string, MONITORDATA> monitors;
std::vector<HWND> bsodWindows;

std::string pwd = "pwd";
std::string storePwd = "";

// function declarations
LRESULT CALLBACK BSODFn(HWND, UINT, WPARAM, LPARAM);
BOOL CALLBACK MonitorEnumProc(__in HMONITOR, __in HDC, __in LPRECT, __in LPARAM);
void CreateBSODClass();
HWND CreateBSODWindow(MONITORINFOEX);
void EnumerateMonitors();
void ListenForMonitorChanges();
void PaintBSODText(HWND hWnd);
void BufferPwdChars();
void CheckPwd();

LRESULT CALLBACK BSODFn(HWND hWnd, UINT uiMsg, WPARAM wParam, LPARAM lParam) {
	switch (uiMsg) {
	case WM_PAINT:
		PaintBSODText(hWnd);
		break;
	case WM_CHAR:
		storePwd += (char) wParam;
		if (storePwd == pwd)
			exit(0);
		break;
	}
	return DefWindowProc(hWnd, uiMsg, wParam, lParam);
}

void PaintBSODText(HWND hWnd) {

	HDC hContext = GetDC(hWnd);
	srand((int)clock());
	RECT windowRect;
	GetClientRect(hWnd, &windowRect);
	int maxPixels = (windowRect.right - windowRect.left) * (windowRect.bottom - windowRect.top);

	// draw random pixels!!
	for (int i = 0; i < maxPixels; i++) {
		// draw to random pixel
		int pixelShade = rand() % 255;
		int x = rand() % (windowRect.right - windowRect.left);
		int y = rand() % (windowRect.bottom - windowRect.top);
		SetPixel(hContext, x, y, RGB(pixelShade, pixelShade, pixelShade));
	}
	ReleaseDC(hWnd, hContext);
}

void EnumerateMonitors() {
	EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, 0);
}

void ListenForMonitorChanges() {
	for (;;) {
		for (int i = 0; i < monitorData.size(); i++) {
			DEVMODE newDeviceMode;
			ZeroMemory(&newDeviceMode, sizeof(DEVMODE));
			newDeviceMode.dmSize = sizeof(DEVMODE);

			if (!EnumDisplaySettingsEx(monitorData.at(i).szDevice, ENUM_CURRENT_SETTINGS, &newDeviceMode,EDS_RAWMODE)) {
				OutputDebugString("Failed to enumerate display settings while listening for monitor changes");
				exit(-1);
			}

			if (monitorData.at(i).oldDeviceMode.dmDisplayOrientation != newDeviceMode.dmDisplayOrientation) {
				// load the old monitor settings
				if (ChangeDisplaySettingsEx(monitorData.at(i).szDevice,&monitorData.at(i).oldDeviceMode,NULL,0,0) != DISP_CHANGE_SUCCESSFUL) {
					OutputDebugString("Could not change the graphics settings back!");
					exit(-1);
				}

				return;
			}
		}
	}
}

BOOL CALLBACK MonitorEnumProc(__in HMONITOR hMonitor, __in HDC hdcMonitor, __in LPRECT lprcMonitor, __in LPARAM dwData) {
	// store the monitor information passed in
	MONITORDATA mInfo;
	ZeroMemory(&mInfo, sizeof(MONITORDATA));
	mInfo.cbSize = sizeof(MONITORINFOEX);
	GetMonitorInfo(hMonitor, &mInfo);
	mInfo.oldDeviceMode.dmSize = sizeof(DEVMODE);

	// try to enumerate the monitor settings
	if (!EnumDisplaySettingsEx(mInfo.szDevice, ENUM_CURRENT_SETTINGS, &mInfo.oldDeviceMode,EDS_ROTATEDMODE)) {
		OutputDebugString("Failed to enumerate display settings");
		exit(-1);
	}

	monitorData.push_back(mInfo);
	return true;
}

void CreateBSODClass() {

	HBRUSH bsodBrush = CreateSolidBrush(RGB(150,150,150));
	WNDCLASS bsodClass = { sizeof(WNDCLASS) };

	bsodClass.lpszClassName = "BSOD";
	bsodClass.lpszMenuName = "NULL";
	bsodClass.style = 0;
	bsodClass.cbClsExtra = 0;
	bsodClass.cbWndExtra = 0;
	bsodClass.hInstance = g_hinst;
	bsodClass.hIcon = NULL;
	bsodClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	bsodClass.hbrBackground = bsodBrush;
	bsodClass.lpfnWndProc = BSODFn;

	if (!RegisterClass(&bsodClass)) {
		exit(-10);
	}

	LOGFONT newFont;
	ZeroMemory(&newFont, sizeof(LOGFONT));
	newFont.lfHeight = 12;
	newFont.lfWeight = FW_REGULAR;
	StringCchCopy((LPSTR)&newFont.lfFaceName, 32, "Consolas");

	lucidaFont = CreateFontIndirect(&newFont);
}

HWND CreateBSODWindow(MONITORINFOEX monitor) {
	
	// create the new window
	HWND newWindow = CreateWindow(TEXT("BSOD"),
		TEXT("BSOD"),
		WS_POPUP | WS_VISIBLE,
		monitor.rcMonitor.left,
		monitor.rcMonitor.top,
		monitor.rcMonitor.right - monitor.rcMonitor.left,
		monitor.rcMonitor.bottom - monitor.rcMonitor.top,
		NULL,
		NULL,
		g_hinst,
		0);

	if (!newWindow) {
		return NULL;
	}

	return newWindow;
}

int MessagePump() {

	MSG programMessage;
	int messageResult;

	while ((messageResult = GetMessage(&programMessage, NULL, 0, 0)) != 0 && messageResult != -1) {

		// disable Alt-F4
		if (programMessage.message == WM_SYSKEYDOWN && programMessage.wParam == VK_F4) {
			OutputDebugString("BAHAHAHA!!!!");
		} else if (programMessage.message == WM_SYSKEYDOWN && programMessage.wParam == VK_TAB) {
			OutputDebugString("That doesn't work either!!!");
		} else {
			TranslateMessage(&programMessage);
			DispatchMessage(&programMessage);
		}
	}

	return programMessage.lParam;
}

// main program execution
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

	g_hinst = hInstance;
	cmdShow = nCmdShow;

	EnumerateMonitors();
	ListenForMonitorChanges();

	// once a monitor changes, the above function will return
	// we can now show the BSOD until the correct password is given
	CreateBSODClass();

	for (int i = 0; i < monitorData.size(); i++) {
		if (!monitorData.at(i).adapter) {
			HWND bsodWindow = CreateBSODWindow(monitorData.at(i));

			if (!bsodWindow) {
				exit(-2);
			}
			bsodWindows.push_back(bsodWindow);
		}
	}


	ShowCursor(false);
	MessagePump();
}