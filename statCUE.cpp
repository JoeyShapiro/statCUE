#include <windows.h>
#include <psapi.h>
#include "iCUESDK/iCUESDK.h"

#include <d3d11.h>
#include <d3dcompiler.h>

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
	if (FAILED(hr)) return 1;

	// Load and compile the compute shader
    ID3DBlob* shaderBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;
    
    hr = D3DCompileFromFile(
        L"ComputeShader.hlsl",      // Shader file
        nullptr,                    // Defines
        D3D_COMPILE_STANDARD_FILE_INCLUDE, // Include handler
        "main",                   // Entry point
        "cs_5_0",                   // Target
        D3DCOMPILE_ENABLE_STRICTNESS, // Flags1
        0,                          // Flags2
        &shaderBlob,                // Output shader blob
        &errorBlob                  // Output error blob
    );
    
    if (FAILED(hr)) {
        if (errorBlob) {
            // OutputError(static_cast<char*>(errorBlob->GetBufferPointer()));
            errorBlob->Release();
        } else {
            // OutputError("Failed to compile shader (unknown error)");
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
        // OutputError("Failed to create compute shader");
        if (device) device->Release();
        if (context) context->Release();
        return 1;
    }
    
    // Create output buffer
    D3D11_BUFFER_DESC outputDesc = {};
    outputDesc.ByteWidth = pixelCount * sizeof(DirectX::XMFLOAT4);
    outputDesc.Usage = D3D11_USAGE_DEFAULT;
    outputDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
    outputDesc.CPUAccessFlags = 0;
    outputDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    outputDesc.StructureByteStride = sizeof(DirectX::XMFLOAT4);
    
    ID3D11Buffer* outputBuffer = nullptr;
    hr = device->CreateBuffer(&outputDesc, nullptr, &outputBuffer);
    
    if (FAILED(hr)) {
        // OutputError("Failed to create output buffer");
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
    uavDesc.Buffer.NumElements = pixelCount;
    uavDesc.Buffer.Flags = 0;
    
    ID3D11UnorderedAccessView* outputUAV = nullptr;
    hr = device->CreateUnorderedAccessView(outputBuffer, &uavDesc, &outputUAV);
    
    if (FAILED(hr)) {
        // OutputError("Failed to create UAV");
        outputBuffer->Release();
        computeShader->Release();
        if (device) device->Release();
        if (context) context->Release();
        return 1;
    }
    
    // Create constant buffer for parameters
    D3D11_BUFFER_DESC constantDesc = {};
    constantDesc.ByteWidth = sizeof(ShaderParameters);
    constantDesc.Usage = D3D11_USAGE_DYNAMIC;
    constantDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    constantDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    constantDesc.MiscFlags = 0;
    
    ID3D11Buffer* constantBuffer = nullptr;
    hr = device->CreateBuffer(&constantDesc, nullptr, &constantBuffer);
    
    if (FAILED(hr)) {
        // OutputError("Failed to create constant buffer");
        outputUAV->Release();
        outputBuffer->Release();
        computeShader->Release();
        if (device) device->Release();
        if (context) context->Release();
        return 1;
    }
    
    // Create staging buffer for reading back results
    D3D11_BUFFER_DESC stagingDesc = {};
    stagingDesc.ByteWidth = pixelCount * sizeof(DirectX::XMFLOAT4);
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.BindFlags = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    stagingDesc.StructureByteStride = sizeof(DirectX::XMFLOAT4);
    
    ID3D11Buffer* stagingBuffer = nullptr;
    hr = device->CreateBuffer(&stagingDesc, nullptr, &stagingBuffer);
    
    if (FAILED(hr)) {
        // OutputError("Failed to create staging buffer");
        constantBuffer->Release();
        outputUAV->Release();
        outputBuffer->Release();
        computeShader->Release();
        if (device) device->Release();
        if (context) context->Release();
        return 1;
    }
    
    // Set up parameters
    ShaderParameters params = {};
    params.time = 1.0f;  // Set time to 1.0 for this example
    params.resolution = DirectX::XMFLOAT2(static_cast<float>(width), static_cast<float>(height));
    
    // Update constant buffer
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    hr = context->Map(constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    
    if (SUCCEEDED(hr)) {
        memcpy(mappedResource.pData, &params, sizeof(ShaderParameters));
        context->Unmap(constantBuffer, 0);
    } else {
        // OutputError("Failed to map constant buffer");
        stagingBuffer->Release();
        constantBuffer->Release();
        outputUAV->Release();
        outputBuffer->Release();
        computeShader->Release();
        if (device) device->Release();
        if (context) context->Release();
        return 1;
    }
    
    // Set up compute shader
    context->CSSetShader(computeShader, nullptr, 0);
    context->CSSetConstantBuffers(0, 1, &constantBuffer);
    context->CSSetUnorderedAccessViews(0, 1, &outputUAV, nullptr);
    
    // Dispatch compute shader
    // Each thread group is 16x16, so we need to calculate how many groups to dispatch
    UINT groupsX = (width + 15) / 16;
    UINT groupsY = (height + 15) / 16;
    context->Dispatch(groupsX, groupsY, 1);
    
    // Unbind resources
    ID3D11UnorderedAccessView* nullUAV = nullptr;
    context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
    
    // Copy results to staging buffer for reading
    context->CopyResource(stagingBuffer, outputBuffer);
    
    // Read back results
    hr = context->Map(stagingBuffer, 0, D3D11_MAP_READ, 0, &mappedResource);
    
    if (SUCCEEDED(hr)) {
        // Get data pointer
        DirectX::XMFLOAT4* data = static_cast<DirectX::XMFLOAT4*>(mappedResource.pData);
        
        // For this example, we'll just output the first few pixels' colors
        std::cout << "First 5 pixel colors:" << std::endl;
        for (int i = 0; i < 5; i++) {
            std::cout << "Pixel " << i << ": R=" << data[i].x << ", G=" << data[i].y 
                     << ", B=" << data[i].z << ", A=" << data[i].w << std::endl;
        }
        
        // In a real application, you would pass this data to your C library
        // c_library_process_image(data, width, height);
        
        context->Unmap(stagingBuffer, 0);
    } else {
        // OutputError("Failed to map staging buffer");
    }
    
    // Clean up resources
    stagingBuffer->Release();
    constantBuffer->Release();
    outputUAV->Release();
    outputBuffer->Release();
    computeShader->Release();
    if (device) device->Release();
    if (context) context->Release();

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
