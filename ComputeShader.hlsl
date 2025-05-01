RWStructuredBuffer<float4> OutputBuffer : register(u0);

[numthreads(1, 1, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
	float4 color;
	color.r = 255;
	color.g = 0;
	color.b = 0;
	color.a = 255;

	uint index = DTid.x;
	OutputBuffer[index] = color;
}