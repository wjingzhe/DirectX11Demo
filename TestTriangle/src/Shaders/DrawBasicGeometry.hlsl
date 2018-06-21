
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

struct VS_INPUT_SCENE
{
	float3 PositionL:POSITION;//Vertex position
	float3 NormalL:NORMAL; 
	float2 TextureUV:TEXCOORD0;
	float3 TangentUVW:TANGENT;
};


struct VS_OUTPUT_SCENE
{
	float4 PositionH:SV_POSITION;//Vertex position
	float4 NormalW:NORMAL;
	float2 TextureUV:TEXCOORD0;
};

VS_OUTPUT_SCENE BaseGeometryVS(VS_INPUT_SCENE vin)
{
	VS_OUTPUT_SCENE vout;
	vout.PositionH = mul(float4(vin.PositionL, 1.0f), g_mWolrdViewProjection);
	
	//rotation maxtix equal to its transposed inverse matrix
	// assume that there is not a scale factor
	vout.NormalW = float4(mul(vin.NormalL, (float3x3)g_mWolrd), 1.0f);
	vout.TextureUV = vin.TextureUV;

	return vout;
}

float4 BaseGeometryPS(VS_OUTPUT_SCENE pin) :SV_TARGET
{
	return float4(1, 0, 0, 1.0);
}