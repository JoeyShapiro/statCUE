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

	int dim = ((int)(ledCount / channels));
	float current = (float)(i % dim) / (ledCount / (channels));

    // Normalized pixel coordinates (from 0 to 1)
    float uv = i/ledCount;
    // Time varying pixel color, ticks has to be 1000f
	// still feels too slow
	// whole loop is in usage range
    float3 col = 0.5 + 0.5*cos((ticks/1000.0) + current/usage + float3(0,2,4));

	// Returns 1 if usage >= current, 0 otherwise (close enough)
	float mask = step(current, usage) * 255;

	float4 color;
	color.rgb = col * mask;
	color.a = 255;

	OutputBuffer[i] = color;
}
