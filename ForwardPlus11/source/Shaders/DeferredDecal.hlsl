

//Textures,Samplers,and Buffers
Texture2D<float> g_TextureDepth:register(t0);
Texture2D<float4> g_TextureDecal:register(t1);
SamplerState g_Sampler:register(s0);

cbuffer cbPerObject:register(b0)
{
	matrix g_mWolrdViewProjection:packoffset(c0);
	matrix g_mWolrd:packoffset(c4);
	matrix g_mWorldViewInv:packoffset(c8);
	float4 boxExtend:packoffset(c12);
};

cbuffer cbPerFrame : register(b1)
{
	float3 g_vCameraPos:packoffset(c0);
	float g_fAlphaTest : packoffset(c0.w);
	matrix g_mProjection:packoffset(c1);
	matrix g_mProjectionInv:packoffset(c5);
	float g_tanHalfFovY : packoffset(c9.x);
	float g_radioWtoH : packoffset(c9.y);
	float g_NearZ : packoffset(c9.z);
	float g_RangZ : packoffset(c9.w);
	float2 g_RenderTargetHalfSize : packoffset(c10);
	float g_FarZ:packoffset(c10.z);

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

VS_OUTPUT_SCENE DeferredDecalVS(VS_INPUT_SCENE vin)
{
	VS_OUTPUT_SCENE vout;
	vout.PositionH = mul(float4(vin.PositionL, 1.0f), g_mWolrdViewProjection);

	//rotation maxtix equal to its transposed inverse matrix
	// assume that there is not a scale factor
	vout.NormalW = float4(mul(vin.NormalL, (float3x3)g_mWolrd), 1.0f);
	vout.TextureUV = vin.TextureUV;

	return vout;
}

float4 ConvertViewToLocal(float4 p)
{
	float4 posL = mul(p, g_mWorldViewInv);
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

//float3 ConverProjToView(float3 projPos3)
//{
//	float3 viewPos;
//
//}

float3 ConvertNdcToProj(float3 ndcPos3)
{
	float3 projPos3;
	projPos3.x = ndcPos3.x*g_radioWtoH;
	projPos3.y = ndcPos3.y;
	projPos3.z = ndcPos3.z;
	return projPos3;
}

float3 ConvertScreenToNdc(float2 screenXY)
{
	//return float4(1.0f,0.0f,0.0f,1.0f);
	float tempX = screenXY.x - 0.5f;
	float tempY = screenXY.y - 0.5f;
	float tempDepth = g_TextureDepth.Load(uint3(tempX, tempY, 0)).x;

	float ndcX = tempX / g_RenderTargetHalfSize.x - 1.0f;
	float ndcY = -tempY / g_RenderTargetHalfSize.y + 1.0f;

	return float3(ndcX, ndcY, tempDepth);
}


float2 ConvertLocalToTexture(float2 PosL)
{
	float2 uv = (PosL.xy + boxExtend.xy / 2) / boxExtend.xy;
	uv.y = -uv.y;
	return uv;
}

float4 DeferredDecalPS(VS_OUTPUT_SCENE pin) :SV_TARGET
{
	float3 ndcPos3 = ConvertScreenToNdc(pin.PositionH.xy);
	float3 projPos3 = ConvertNdcToProj(ndcPos3);
	float viewdDepth = g_NearZ*g_RangZ / (g_RangZ - projPos3.z);
	float3 viewPos3 = float3(projPos3.x*viewdDepth*g_tanHalfFovY, projPos3.y*viewdDepth*g_tanHalfFovY, viewdDepth);

	float4 PosL = mul(float4(viewPos3,1.0f), g_mWorldViewInv);

	bool flag = IsPointInsideBox(PosL);
	if (!flag)
	{
		discard;
	}

//	return float4(1.0f, 0.0f, 0.0f, 1.0f);

	//float3 uvw = (PosL.xyz + boxExtend.xyz / 2) / boxExtend.xyz;
	float2 uv = ConvertLocalToTexture(PosL.xy);
	float4 color = g_TextureDecal.Sample(g_Sampler, uv);

	if (color.a < g_fAlphaTest)
	{
		discard;
	}

	return color;
}