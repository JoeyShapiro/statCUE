// TODO add config to this
RWStructuredBuffer<float4> OutputBuffer : register(u0);

// Parameters for the shader
cbuffer Parameters : register(b0)
{
    uint ledCount;  // Number of elements to process
	float usage;
    dword ticks;         // Time value for animation or processing
	int  corsairDeviceType;
	int channels;
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
	int dim = ((int)(ledCount / channels));
	float current = (float)(i % dim) / (ledCount / (channels));

	color.r = usage > current ? 255 : 0;
	color.g = 0;
	color.b = 0;
	color.a = 255;

	OutputBuffer[i] = color;
}
