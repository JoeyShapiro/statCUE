#include <windows.h>
#include <psapi.h>
#include "iCUESDK/iCUESDK.h"

#include <d3d11.h>
#include <d3dcompiler.h>

static CorsairDeviceInfo corsair_ram;
static CorsairLedPosition* leds = nullptr;
static CorsairLedColor* colors = nullptr;

struct ShaderParameters {
    UINT ledCount;
    float usage;
    DWORD ticks;
    int corsairDeviceType;
};

struct Color {
    float r;
    float g;
    float b;
    float a;
};

void stateChange(void* context, const CorsairSessionStateChanged* event) {
	if (event->state != CSS_Connected) return;

	if (leds) free(leds);
	if (colors) free(colors);

	CorsairDeviceFilter filter = { 0 };
	filter.deviceTypeMask = CDT_All;// CDT_MemoryModule;
	int size = 0;
	CorsairDeviceInfo* devices = (CorsairDeviceInfo*)malloc(sizeof(CorsairDeviceInfo) * 3);
	CorsairError err = CorsairGetDevices(&filter, CORSAIR_DEVICE_COUNT_MAX, devices, &size);
    if (!devices) {
        MessageBoxA(NULL, "Couldn't to find any Corsair Devices", "Warning", MB_ICONWARNING | MB_OK);
        return;
    }

	for (size_t i = 0; i < size; i++)
	{
		if (devices[i].type != CDT_MemoryModule) continue;

		corsair_ram = devices[i];

		leds = (CorsairLedPosition*)malloc(sizeof(CorsairLedPosition) * corsair_ram.ledCount);
		int size = 0;
		err = CorsairGetLedPositions(corsair_ram.id, CORSAIR_DEVICE_LEDCOUNT_MAX, leds, &size);
		if (err != CE_Success) {
			MessageBoxA(NULL, "Failed to get LED positions", "Error", MB_ICONERROR | MB_OK);
			return;
		}
		if (size != corsair_ram.ledCount) {
			MessageBoxA(NULL, "Failed to get LED positions", "Error", MB_ICONERROR | MB_OK);
			return;
		}

		colors = (CorsairLedColor*)malloc(sizeof(CorsairLedColor) * corsair_ram.ledCount);
		if (!colors) {
			MessageBoxA(NULL, "This error is here because, somehow, malloc failed. Download more RAM lol", "Error", MB_ICONERROR | MB_OK);
			return;
		}
	}
}

int main() {
	HWND hWnd = GetConsoleWindow();
    ShowWindow(hWnd, SW_HIDE);

	ID3D11Device* device = nullptr;
	ID3D11DeviceContext* context = nullptr;
	D3D_FEATURE_LEVEL featureLevel;
	UINT flags = 0;

	HRESULT hr = D3D11CreateDevice(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		flags,
		nullptr,
		0,
		D3D11_SDK_VERSION,
		&device,
		&featureLevel,
		&context
	);
    
    if (FAILED(hr)) {
        MessageBoxA(NULL, "Failed to create DirectX Device", "Error", MB_ICONERROR | MB_OK);
        return 1;
    }
    
    // Load and compile the compute shader
    ID3DBlob* shaderBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;
    
    hr = D3DCompileFromFile(
        L"ComputeShader.hlsl",
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "main",
        "cs_5_0",
        0,
        0,
        &shaderBlob,
        &errorBlob
    );
    
    if (FAILED(hr)) {
        MessageBoxA(NULL, "Failed to compile ComputeShader.hlsl", "Error", MB_ICONERROR | MB_OK);

        if (errorBlob) {
            errorBlob->Release();
        }
        if (device) device->Release();
        if (context) context->Release();
        return 1;
    }
    
    // Create the compute shader
    ID3D11ComputeShader* computeShader = nullptr;
    hr = device->CreateComputeShader(
        shaderBlob->GetBufferPointer(),
        shaderBlob->GetBufferSize(),
        nullptr,
        &computeShader
    );
    
    shaderBlob->Release();
    
    if (FAILED(hr)) {
        MessageBoxA(NULL, "Device failed to create Compute Shader", "Error", MB_ICONERROR | MB_OK);

        if (device) device->Release();
        if (context) context->Release();
        return 1;
    }
    
    // Create output buffer
    D3D11_BUFFER_DESC outputDesc = {};
    outputDesc.ByteWidth = 20 * sizeof(Color);
    outputDesc.Usage = D3D11_USAGE_DEFAULT;
    outputDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    outputDesc.CPUAccessFlags = 0;
    outputDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    outputDesc.StructureByteStride = sizeof(Color);
    
    ID3D11Buffer* outputBuffer = nullptr;
    hr = device->CreateBuffer(&outputDesc, nullptr, &outputBuffer);
    
    if (FAILED(hr)) {
        MessageBoxA(NULL, "Device failed to Create Buffer", "Error", MB_ICONERROR | MB_OK);

        computeShader->Release();
        if (device) device->Release();
        if (context) context->Release();
        return 1;
    }
    
    // Create UAV for output buffer
    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = 20;
    uavDesc.Buffer.Flags = 0;
    
    ID3D11UnorderedAccessView* outputUAV = nullptr;
    hr = device->CreateUnorderedAccessView(outputBuffer, &uavDesc, &outputUAV);
    
    if (FAILED(hr)) {
        MessageBoxA(NULL, "Device failed to create unordered access view", "Error", MB_ICONERROR | MB_OK);

        outputBuffer->Release();
        computeShader->Release();
        if (device) device->Release();
        if (context) context->Release();
        return 1;
    }
    
    // Create constant buffer for parameters
    D3D11_BUFFER_DESC constantDesc = {};
    constantDesc.ByteWidth = sizeof(ShaderParameters);  // TTODOOo  mmultiiplle of 16
    constantDesc.Usage = D3D11_USAGE_DYNAMIC;
    constantDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    constantDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    constantDesc.MiscFlags = 0;
    
    ID3D11Buffer* constantBuffer = nullptr;
    hr = device->CreateBuffer(&constantDesc, nullptr, &constantBuffer);
    
    if (FAILED(hr)) {
        MessageBoxA(NULL, "Device failed to create constant buffer", "Error", MB_ICONERROR | MB_OK);

        outputUAV->Release();
        outputBuffer->Release();
        computeShader->Release();
        if (device) device->Release();
        if (context) context->Release();
        return 1;
    }
    
    // Create staging buffer for reading back results
    D3D11_BUFFER_DESC stagingDesc = {};
    stagingDesc.ByteWidth = 20 * sizeof(Color);
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.BindFlags = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    stagingDesc.StructureByteStride = sizeof(Color);
    
    ID3D11Buffer* stagingBuffer = nullptr;
    hr = device->CreateBuffer(&stagingDesc, nullptr, &stagingBuffer);
    
    if (FAILED(hr)) {
        MessageBoxA(NULL, "Device failed to create staging buffer", "Error", MB_ICONERROR | MB_OK);

        constantBuffer->Release();
        outputUAV->Release();
        outputBuffer->Release();
        computeShader->Release();
        if (device) device->Release();
        if (context) context->Release();
        return 1;
    }

	CorsairError err = CorsairConnect(stateChange, NULL);

    MEMORYSTATUSEX memInfo;
	memInfo.dwLength = sizeof(MEMORYSTATUSEX);
	while (true) {
		Sleep(16);
		if (!corsair_ram.id[0]) continue;
		
		GlobalMemoryStatusEx(&memInfo);

		// Update parameters
		ShaderParameters params = {
			.ledCount = corsair_ram.ledCount,
			.usage = memInfo.dwMemoryLoad / static_cast<float>(100),
			.ticks = GetTickCount(),
			.corsairDeviceType = corsair_ram.type
		};

		D3D11_MAPPED_SUBRESOURCE mappedResource;
		hr = context->Map(constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
		if (FAILED(hr)) {
			MessageBoxA(NULL, "Context failed to map constant resources", "Error", MB_ICONERROR | MB_OK);

			stagingBuffer->Release();
			constantBuffer->Release();
			outputUAV->Release();
			outputBuffer->Release();
			computeShader->Release();
			if (device) device->Release();
			if (context) context->Release();
			return 1;
		}

		memcpy(mappedResource.pData, &params, sizeof(ShaderParameters));
		context->Unmap(constantBuffer, 0);

		// Set up compute shader
		context->CSSetShader(computeShader, nullptr, 0);
		context->CSSetConstantBuffers(0, 1, &constantBuffer);
		context->CSSetUnorderedAccessViews(0, 1, &outputUAV, nullptr);

		// Dispatch compute shader
		// Calculate how many thread groups to dispatch (each group has 256 threads)
		UINT threadGroupCount = (20 + 255) / 256;
		context->Dispatch(threadGroupCount, 1, 1);

		// Unbind resources
		ID3D11UnorderedAccessView* nullUAV = nullptr;
		context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);

		// Copy results to staging buffer for reading
		context->CopyResource(stagingBuffer, outputBuffer);

		// Read back results
		hr = context->Map(stagingBuffer, 0, D3D11_MAP_READ, 0, &mappedResource);
		if (FAILED(hr)) {
			MessageBoxA(NULL, "Failed to read staging buffer", "Error", MB_ICONERROR | MB_OK);
			return 1;
		}

		if (!leds) {
			MessageBoxA(NULL, "Corsair failed to get device LEDs", "Error", MB_ICONERROR | MB_OK);
			return 1;
		}

		// Get data pointer
		Color* data = static_cast<Color*>(mappedResource.pData);
		for (size_t i = 0; i < corsair_ram.ledCount; i++)
		{
			colors[i].id = leds[i].id;
			colors[i].r = data[i].r;
			colors[i].g = data[i].g;
			colors[i].a = data[i].a;
		}

		context->Unmap(stagingBuffer, 0);

		err = CorsairSetLedColors(corsair_ram.id, corsair_ram.ledCount, colors);
	}

    // Clean up resources
    stagingBuffer->Release();
    constantBuffer->Release();
    outputUAV->Release();
    outputBuffer->Release();
    computeShader->Release();
    context->Release();
    device->Release();
}
