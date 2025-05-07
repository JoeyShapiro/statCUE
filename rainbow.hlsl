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
	int sticks = 2;
	int dim = ((int)(ledCount / sticks));
	float current = (float)(i % dim) / (ledCount / (sticks));

    // Normalized pixel coordinates (from 0 to 1)
    float uv = i/ledCount;
    // Time varying pixel color, ticks has to be 1000f
	// still feels too slow
	// TODO start of rainbow should be at usage
    float3 col = 0.5 + 0.5*cos((ticks/1000.0) + current + float3(0,2,4));

	color.r = usage > current ? col.r * 255 : 0;
	color.g = usage > current ? col.g * 255 : 0;
	color.b = usage > current ? col.b * 255 : 0;
	color.a = 255;

	OutputBuffer[i] = color;
}
