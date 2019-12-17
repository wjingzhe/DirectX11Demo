

cbuffer cbPerObject:register(b0)
{
	matrix g_mWolrdViewProjection:packoffset(c0);
	matrix g_mWolrd:packoffset(c4);

};

cbuffer cbPerFrame : register(b1)
{
	float3 g_vCameraPos:packoffset(c0);
	float g_fAlphaTest : packoffset(c0.w);
	float g_ScreenWidth: packoffset(c1.x);
	float g_ScreenHeight : packoffset(c1.y);
};


struct VS_INPUT
{
	float3 PositionL:POSITION;//Vertex position
	float3 NormalL:NORMAL;
	float2 TextureUV:TEXCOORD0;
	float3 TangentUVW:TANGENT;
};

struct VS_OUTPUT
{
	float4 PositionH:SV_POSITION;//Vertex position
	float4 PositionW:TEXCOORD0;//World position
};

VS_OUTPUT CubeMapCaptureVS(VS_INPUT vin)
{
	VS_OUTPUT vout;
	//vout.PositionH = float4(vin.PositionL, 1.0f);
	vout.PositionH = mul(float4(vin.PositionL, 1.0f), g_mWolrdViewProjection);
	vout.PositionW = float4(vin.PositionL, 1.0f);

	return vout;
}


Texture2D HdrMap:register(t1);
SamplerState g_Sampler:register(s0);


const float2 invAtan = float2(0.1591, 0.3183);
float2 SampleSphericalMap(float3 v)
{
	float2 uv = float2(atan(v.z/v.x), asin(v.y));
	uv *= invAtan;
	uv += 0.5;
	return uv;
}


float4 CubeMapCapturePS(VS_OUTPUT pin):SV_TARGET
{
	float2 uv = SampleSphericalMap(normalize(pin.PositionW));
	float2 uv2 = float2(pin.PositionH.x / g_ScreenWidth, pin.PositionH.y / g_ScreenHeight);
	float3 color = HdrMap.Sample(g_Sampler, uv2).xyz;
	return float4(1.0f,0.0f,0.0f,1.0f);
}