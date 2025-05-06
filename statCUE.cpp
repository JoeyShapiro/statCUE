#include <windows.h>
#include <psapi.h>
#include "iCUESDK/iCUESDK.h"

#include <d3d11.h>
#include <d3dcompiler.h>

static CorsairDeviceInfo corsair_ram;
static CorsairLedPosition* leds = nullptr;
static CorsairLedColor* colors = nullptr;

static ID3D11Device* device = nullptr;
static ID3D11DeviceContext* d11context = nullptr;
static ID3D11Buffer* outputBuffer = nullptr;
static ID3D11UnorderedAccessView* outputUAV = nullptr;
static ID3D11Buffer* constantBuffer = nullptr;
static ID3D11Buffer* stagingBuffer = nullptr;

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

bool setupShaderBuffers() {
	if (stagingBuffer) stagingBuffer->Release();
	if (constantBuffer) constantBuffer->Release();
	if (outputUAV) outputUAV->Release();
	if (outputBuffer) outputBuffer->Release();

	// Create output buffer
	D3D11_BUFFER_DESC outputDesc = {};
	outputDesc.ByteWidth = corsair_ram.ledCount * sizeof(Color);
	outputDesc.Usage = D3D11_USAGE_DEFAULT;
	outputDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	outputDesc.CPUAccessFlags = 0;
	outputDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	outputDesc.StructureByteStride = sizeof(Color);

	HRESULT hr = device->CreateBuffer(&outputDesc, nullptr, &outputBuffer);
	if (FAILED(hr)) {
		MessageBoxA(NULL, "Device failed to Create Buffer", "Error", MB_ICONERROR | MB_OK);
		return false;
	}

	// Create UAV for output buffer
	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = corsair_ram.ledCount;
	uavDesc.Buffer.Flags = 0;

	hr = device->CreateUnorderedAccessView(outputBuffer, &uavDesc, &outputUAV);
	if (FAILED(hr)) {
		MessageBoxA(NULL, "Device failed to create unordered access view", "Error", MB_ICONERROR | MB_OK);
		return false;
	}

	// Create constant buffer for parameters
	D3D11_BUFFER_DESC constantDesc = {};
	constantDesc.ByteWidth = sizeof(ShaderParameters);  // TTODOOo  mmultiiplle of 16
	constantDesc.Usage = D3D11_USAGE_DYNAMIC;
	constantDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	constantDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	constantDesc.MiscFlags = 0;

	hr = device->CreateBuffer(&constantDesc, nullptr, &constantBuffer);
	if (FAILED(hr)) {
		MessageBoxA(NULL, "Device failed to create constant buffer", "Error", MB_ICONERROR | MB_OK);
		return false;
	}

	// Create staging buffer for reading back results
	D3D11_BUFFER_DESC stagingDesc = {};
	stagingDesc.ByteWidth = corsair_ram.ledCount * sizeof(Color);
	stagingDesc.Usage = D3D11_USAGE_STAGING;
	stagingDesc.BindFlags = 0;
	stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	stagingDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	stagingDesc.StructureByteStride = sizeof(Color);

	hr = device->CreateBuffer(&stagingDesc, nullptr, &stagingBuffer);
	if (FAILED(hr)) {
		MessageBoxA(NULL, "Device failed to create staging buffer", "Error", MB_ICONERROR | MB_OK);
		return false;
	}

	return true;
}

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

	// TODO add structure for multiple devices support
	for (size_t i = 0; i < size; i++)
	{
		if (devices[i].type != CDT_MemoryModule) continue;

		// copy device info to global variable
		corsair_ram.type = devices[i].type;
		corsair_ram.id[0] = 0;
		corsair_ram.ledCount = devices[i].ledCount;
		corsair_ram.channelCount = devices[i].channelCount;
		strncpy_s(corsair_ram.id, devices[i].id, CORSAIR_STRING_SIZE_M);


		leds = (CorsairLedPosition*)malloc(sizeof(CorsairLedPosition) * corsair_ram.ledCount);
		if (!leds) {
			MessageBoxA(NULL, "This error is here because, somehow, malloc failed. Download more RAM lol", "Error", MB_ICONERROR | MB_OK);

			if (stagingBuffer) stagingBuffer->Release();
			if (constantBuffer) constantBuffer->Release();
			if (outputUAV) outputUAV->Release();
			if (outputBuffer) outputBuffer->Release();
			if (d11context) d11context->Release();
			if (device) device->Release();

			ExitProcess(1);
		}

		int size = 0;
		err = CorsairGetLedPositions(corsair_ram.id, CORSAIR_DEVICE_LEDCOUNT_MAX, leds, &size);
		if (err != CE_Success) {
			MessageBoxA(NULL, "Failed to get LED positions", "Error", MB_ICONERROR | MB_OK);
		}
		if (size != corsair_ram.ledCount) {
			MessageBoxA(NULL, "Failed to get LED positions", "Error", MB_ICONERROR | MB_OK);
		}

		colors = (CorsairLedColor*)malloc(sizeof(CorsairLedColor) * corsair_ram.ledCount);
		if (!colors) {
			MessageBoxA(NULL, "This error is here because, somehow, malloc failed. Download more RAM lol", "Error", MB_ICONERROR | MB_OK);

			if (stagingBuffer) stagingBuffer->Release();
			if (constantBuffer) constantBuffer->Release();
			if (outputUAV) outputUAV->Release();
			if (outputBuffer) outputBuffer->Release();
			if (d11context) d11context->Release();
			if (device) device->Release();

			ExitProcess(1);
		}

		if (!setupShaderBuffers()) {
			if (stagingBuffer) stagingBuffer->Release();
			if (constantBuffer) constantBuffer->Release();
			if (outputUAV) outputUAV->Release();
			if (outputBuffer) outputBuffer->Release();
			if (d11context) d11context->Release();
			if (device) device->Release();

			ExitProcess(1);
		}
	}

	if (devices) free(devices);
}

int main() {
	HWND hWnd = GetConsoleWindow();
    ShowWindow(hWnd, SW_HIDE);

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
		&d11context
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
        nullptr, // macros
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

        if (errorBlob) errorBlob->Release();
        if (device) device->Release();
        if (d11context) d11context->Release();
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
        if (d11context) d11context->Release();
        return 1;
    }
    
	D3D11_MAPPED_SUBRESOURCE mappedResource;

	CorsairError err = CorsairConnect(stateChange, NULL);
	if (err != CE_Success) {
		MessageBoxA(NULL, "Failed to connect to iCUE SDK", "Error", MB_ICONERROR | MB_OK);
		return 1;
	}

    MEMORYSTATUSEX memInfo;
	memInfo.dwLength = sizeof(MEMORYSTATUSEX);
	while (true) {
		Sleep(16); // TODO allow option to change this value
		if (!corsair_ram.id[0]) continue;
		
		GlobalMemoryStatusEx(&memInfo);

		// Update parameters
		ShaderParameters params = {};
		params.ledCount = corsair_ram.ledCount;
		params.usage = memInfo.dwMemoryLoad / static_cast<float>(100);
		params.ticks = GetTickCount();
		params.corsairDeviceType = corsair_ram.type;

		hr = d11context->Map(constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
		if (FAILED(hr)) {
			MessageBoxA(NULL, "Context failed to map constant resources", "Error", MB_ICONERROR | MB_OK);
			break;
		}

		memcpy(mappedResource.pData, &params, sizeof(ShaderParameters));
		d11context->Unmap(constantBuffer, 0);

		// Set up compute shader
		d11context->CSSetShader(computeShader, nullptr, 0);
		d11context->CSSetConstantBuffers(0, 1, &constantBuffer);
		d11context->CSSetUnorderedAccessViews(0, 1, &outputUAV, nullptr);

		// Dispatch compute shader
		// Calculate how many thread groups to dispatch (each group has 256 threads)
		// UINT threadGroupCount = (20 + 255) / 256;
		d11context->Dispatch(1, 1, 1);

		// Unbind resources
		ID3D11UnorderedAccessView* nullUAV = nullptr;
		d11context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);

		// Copy results to staging buffer for reading
		d11context->CopyResource(stagingBuffer, outputBuffer);

		// Read back results
		hr = d11context->Map(stagingBuffer, 0, D3D11_MAP_READ, 0, &mappedResource);
		if (FAILED(hr)) {
			MessageBoxA(NULL, "Failed to read staging buffer", "Error", MB_ICONERROR | MB_OK);
			break;
		}

		// Get data pointer
		Color* data = static_cast<Color*>(mappedResource.pData);
		for (size_t i = 0; i < corsair_ram.ledCount; i++)
		{
			// TODO uses float values, need to convert to 0-255 range
			colors[i].id = leds[i].id;
			colors[i].r = data[i].r;
			colors[i].g = data[i].g;
			colors[i].b = data[i].b;
			colors[i].a = data[i].a;
		}

		d11context->Unmap(stagingBuffer, 0);

		err = CorsairSetLedColors(corsair_ram.id, corsair_ram.ledCount, colors);
	}

    // Clean up resources
	if (stagingBuffer) stagingBuffer->Release();
    if (constantBuffer) constantBuffer->Release();
    if (outputUAV) outputUAV->Release();
    if (outputBuffer) outputBuffer->Release();
	if (computeShader) computeShader->Release();
    if (d11context) d11context->Release();
    if (device) device->Release();
}
