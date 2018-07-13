

//Textures,Samplers,and Buffers
Texture2D<float> g_TextureDepth:register(t0);
Texture2D<float4> g_TextureDecal:register(t1);
SamplerState g_Sampler:register(s0);

cbuffer cbPerObject:register(b0)
{
	matrix g_mWolrdViewProjection:packoffset(c0);
	matrix g_mWolrd:packoffset(c4);
	matrix g_mWorldViewProjectionInv:packoffset(c8);
	float4 boxExtend:packoffset(c12);
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

float4 ConvertPrjoToLocal(float4 p)
{
	float4 posL = mul(p, g_mWorldViewProjectionInv);
	posL /= posL.w;

	return posL;
}

bool IsPointInsideBox(float4 PosL)
{
	if (PosL.x >= - boxExtend.x / 2
		&& PosL.y >= - boxExtend.y / 2
		&& PosL.z >= - boxExtend.z / 2
		&& PosL.x <= boxExtend.x / 2
		&& PosL.y <= boxExtend.y / 2
		&& PosL.z <= boxExtend.z / 2)
	{
		return true;
	}
	return false;
}


float4 BaseGeometryPS(VS_OUTPUT_SCENE pin) :SV_TARGET
{
	float4 posH = normalize(pin.PositionH);
	
	float depth = g_TextureDepth.Load(float3(posH.x, posH.y, 0)).x;

	float4 ProjPos4 = float4(posH.x, posH.y, depth,1.0f);

	float4 PosL = ConvertPrjoToLocal(ProjPos4);

	//if (!IsPointInsideBox(PosL))
	//{
	//	discard;
	//}

	float3 uvw = (PosL.xyz + boxExtend.xyz / 2) / boxExtend.xyz;

	float4 color = g_TextureDecal.Load(uvw);

	//if (color.a < g_fAlphaTest)
	//{
	//	discard;
	//}

	return color;
}