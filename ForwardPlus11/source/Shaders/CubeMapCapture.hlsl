

cbuffer cbPerObject:register(b0)
{
	matrix g_mWolrdViewProjection:packoffset(c0);
	matrix g_mWolrd:packoffset(c4);

};

cbuffer cbPerFrame : register(b1)
{
	float3 g_vCameraPos:packoffset(c0);
	float g_fAlphaTest : packoffset(c0.w);
};


struct VS_INPUT
{
	float3 PositionL:POSITION;//Vertex position
};

struct VS_OUTPUT
{
	float4 PositionH:SV_POSITION;//Vertex position
	float4 PositionW:TEXCOORD0;//World position
};

VS_OUTPUT CubeMapCaptureVS(VS_INPUT vin)
{
	VS_OUTPUT vout;
	vout.PositionH = mul(float4(vin.PositionL, 1.0f), g_mWolrdViewProjection);
	vout.PositionW = mul(float4(vin.PositionL, 1.0f), g_mWolrd);

	return vout;
}


Texture2D HdrMap:register(t1);
SamplerState g_Sampler:register(s0);


const float2 invAtan = float2(0.1591, 0.3183);
float2 SampleSphericalMap(float3 v)
{
	float2 uv = float2(atan(v.z, v.x), asin(v.y));
	uv *= invAtan;
	uv += 0.5;
	return uv;
}


float4 CubeMapCapturePS(VS_OUTPUT pin):SV_TARGET
{
	float2 uv = SampleSphericalMap(normalize(pin.PositionW));

	float3 color = HdrMap.Sample(g_Sampler, uv).xyz;
	return float4(color,1.0);
}