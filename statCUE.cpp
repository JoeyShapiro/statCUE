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

		double used = memInfo.dwMemoryLoad / static_cast<double>(100);

		if (corsair_ram.id[0]) {
			CorsairLedPosition* leds = (CorsairLedPosition*)malloc(sizeof(CorsairLedPosition) * corsair_ram.ledCount);
			int size = 0;
			err = CorsairGetLedPositions(corsair_ram.id, CORSAIR_DEVICE_LEDCOUNT_MAX, leds, &size);

			// TODO handle all frees and stuff
			CorsairLedColor* colors = (CorsairLedColor*)malloc(sizeof(CorsairLedColor)*corsair_ram.ledCount);

			ZeroMemory(colors, sizeof(CorsairLedColor) * corsair_ram.ledCount);
			for (size_t i = 0; i < corsair_ram.ledCount; i++)
			{
				int sticks = 2;
				int dim = ((int)(corsair_ram.ledCount / sticks));
				double current = (float)(i % dim) / (corsair_ram.ledCount / static_cast<double>(sticks));

				colors[i].id = leds[i].id;
				colors[i].r = used > current ? 255 : 0;
				colors[i].a = 255;
			}

			err = CorsairSetLedColors(corsair_ram.id, corsair_ram.ledCount, colors);
		}
		Sleep(1000);
	}
}
