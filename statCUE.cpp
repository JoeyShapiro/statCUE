#include <windows.h>
#include <psapi.h>
#include "iCUESDK/iCUESDK.h"

static CorsairDeviceInfo corsair_ram;

void stateChange(void* context, const CorsairSessionStateChanged* event) {
	if (event->state != CSS_Connected) return;

	CorsairDeviceFilter filter = { 0 };
	filter.deviceTypeMask = CDT_All;// CDT_MemoryModule;
	int size = 0;
	CorsairDeviceInfo* devices = (CorsairDeviceInfo*)malloc(sizeof(CorsairDeviceInfo) * 3);
	CorsairError err = CorsairGetDevices(&filter, CORSAIR_DEVICE_COUNT_MAX, devices, &size);
	if (!devices) return;

	for (size_t i = 0; i < size; i++)
	{
		if (devices[i].type != CDT_MemoryModule) continue;

		corsair_ram = devices[i];
	}
}

int main() {
	HWND hWnd = GetConsoleWindow();
	ShowWindow(hWnd, SW_HIDE);

	CorsairError err = CorsairConnect(stateChange, NULL);

	MEMORYSTATUSEX memInfo;
	memInfo.dwLength = sizeof(MEMORYSTATUSEX);
	while (true) {
		GlobalMemoryStatusEx(&memInfo);

		double used = memInfo.dwMemoryLoad;

		if (corsair_ram.id[0]) {
			// TODO handle all frees and stuff
			CorsairLedColor* colors = (CorsairLedColor*)malloc(sizeof(CorsairLedColor)*corsair_ram.ledCount);
			ZeroMemory(colors, sizeof(CorsairLedColor) * corsair_ram.ledCount);
			for (size_t i = 0; i < corsair_ram.ledCount; i++)
			{
				colors[i].id = i;
			}

			err = CorsairSetLedColors(corsair_ram.id, corsair_ram.ledCount, colors);
			int x = 0;
		}
		Sleep(1000);
	}
}
