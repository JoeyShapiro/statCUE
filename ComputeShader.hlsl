RWStructuredBuffer<float4> OutputBuffer : register(u0);

// Parameters for the shader
cbuffer Parameters : register(b0)
{
    uint elementCount;  // Number of elements to process
	float usage;
    float time;         // Time value for animation or processing
};

[numthreads(256, 1, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
	// Get our element index from the thread ID
    uint index = DTid.x;
    
    // Check if we're within the data bounds
    if (index >= elementCount)
        return;

	float4 color;
	color.r = 255;
	color.g = 0;
	color.b = 0;
	color.a = 255;

	OutputBuffer[index] = color;
}