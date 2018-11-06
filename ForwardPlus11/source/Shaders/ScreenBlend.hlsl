
Texture2D<float4> g_TextureBlendSrc:register(t1);
SamplerState g_Sampler:register(s0);

struct VS_INPUT
{
	float3 PositionL:POSITION;//Vertex position
	float3 NormalL:NORMAL;
	float2 TextureUV:TEXCOORD0;
	float3 TangentUVW:TANGENT;
};

struct VS_OUTPUT
{
	float4 PositionH:SV_POSITION;
	float2 TextureUV:TEXCOORD0;
};


VS_OUTPUT ScreenBlendVS(VS_INPUT vin)
{
	VS_OUTPUT vout;
	vout.PositionH = float4(vin.PositionL, 0.0f);
	vout.TextureUV = vin.TextureUV;

	return vout;
}

float4 ScreenBlendPS(VS_OUTPUT pin):SV_TARGET
{
	return g_TextureBlendSrc.Sample(g_Sampler, pin.TextureUV);
}