
Texture2D<float4> g_SrcTexture:register(t0);
SamplerState g_Sampler:register(s0);

cbuffer cbPerObject:register(b0)
{
	float2 RadialBlurCenterUV:packoffset(c0);
	float fRadialBlurLength : packoffset(c0.z);
	int iSampleCount : packoffset(c0.w);
}

cbuffer cbPerFrame : register(b1)
{

}

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

VS_OUTPUT RadialBlurVS(VS_INPUT vin)
{
	VS_OUTPUT vout;
	vout.PositionH = float4(vin.PositionL, 1.0f);
	vout.TextureUV = vin.TextureUV;

	return vout;
}

float4 RadialBlurPS(VS_OUTPUT pin):SV_TARGET
{
	float2 LeaveCenter = pin.TextureUV - RadialBlurCenterUV;
	float2 Step = LeaveCenter / iSampleCount;

	float4 Color = 0.0f;

	for (uint i = 0; i < iSampleCount; ++i)
	{
		float2 uv = pin.TextureUV - Step*i;
		Color += g_SrcTexture.Sample(g_Sampler, uv);
		//float4 temp = g_SrcTexture.Sample(g_Sampler, uv);
		//Color.x = max(Color.x, temp.x);
		//Color.y = max(Color.y, temp.y);
		//Color.z = max(Color.z, temp.z);
		//Color.w = max(Color.w, temp.w);
	}
	
	//float4 temp = Color / iSampleCount;
	Color = Color/ iSampleCount*1.5f;
	//Color.w = g_SrcTexture.Sample(g_Sampler, pin.TextureUV).w;
	Color *= (1 - length(LeaveCenter));
	//Color *= (1 - length(LeaveCenter));
	//Color *= (1 - length(LeaveCenter));
	//Color *= (1 - length(LeaveCenter));
	//Color *= (1 - length(LeaveCenter));
	if (Color.w < 0.15f&&length(Color.xyz)<0.15f)
	{
		discard;
	}

	return Color;

	//return g_SrcTexture.Sample(g_Sampler, pin.TextureUV);

	//float2 NegativeStep = ToCenter / iSampleCount*fRadialBlurLength;
	//for (uint i = 0; i < iSampleCount; ++i)
	//{
	//	float2 uv = pin.TextureUV - NegativeStep*i;
	//	Color += g_SrcTexture.Sample(g_Sampler, uv);
	//}

	//return Color/ (iSampleCount*2);
}