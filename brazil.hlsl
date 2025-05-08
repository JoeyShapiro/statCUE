// TODO add config to this
RWStructuredBuffer<float4> OutputBuffer : register(u0);

// Parameters for the shader
cbuffer Parameters : register(b0)
{
    uint ledCount;  // Number of elements to process
	float usage;
    dword ticks;         // Time value for animation or processing
	int  corsairDeviceType;
};

[numthreads(20, 1, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
	// Get our element index from the thread ID
    uint i = DTid.x;
    
    // Check if we're within the data bounds
	if (i >= ledCount)
		return;

	float4 color;
    color.r = 0;
    color.g = 0;
    color.b = 0;
    color.a = 255;
    
    if (usage <= 0.25) {
        color.g = 255;
    } else if (usage <= 0.5) {
        color.b = 255;
    } else {
        color.r = 255;
        color.g = 255;
    }

	OutputBuffer[i] = color;
}
