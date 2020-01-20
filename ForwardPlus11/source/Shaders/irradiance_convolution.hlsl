cbuffer cbPerObject:register(b0)
{
	matrix g_mWolrdViewProjection:packoffset(c0);
	matrix g_mWolrd:packoffset(c4);
	float3 g_Up: packoffset(c8);

};
cbuffer cbPerFrame : register(b1)
{
	float3 g_vCameraPos:packoffset(c0);
	float g_fAlphaTest : packoffset(c0.w);
	float g_ScreenWidth : packoffset(c1.x);
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
	float3 PositionW:TEXCOORD0;//World position
	float3 NormalL:TEXCOORD1;
};

VS_OUTPUT IrradianceConvolutionVS(VS_INPUT vin)
{

	VS_OUTPUT vout;
	//vout.PositionH = float4(vin.PositionL, 1.0f);
	vout.PositionH = mul(float4(vin.PositionL, 1.0f), g_mWolrdViewProjection);
	vout.PositionW = mul(vin.PositionL, (float3x3)g_mWolrd);
	vout.NormalL = mul(vin.NormalL, (float3x3)g_mWolrd);
	return vout;
}


TextureCube gCubeMap:register(t0);
SamplerState g_Sampler:register(s0);


float4 IrradianceConvolutionPS(VS_OUTPUT pin) :SV_TARGET
{
	float PI = 3.141591653f;

	//gCubeMap.Sample(g_Sampler, pin.PositionW);

	float3 N = normalize(pin.PositionW.xyz);

	float3 irradiance = float3(0.0f, 0.0f, 0.0f);

	// tangent space calculation from origin point
	float3 up = float3(0.0f, 1.0f, 0.0f);//float3(0.0f,1.0f,0.0f);// g_Up;
	float3 right = cross(up, N);
	up = cross(N, right);

	float sampleDelta = 0.025f;
	float nrSamples = 0.0f;
	for (float phi = 0.0f; phi < 2.0f * PI; phi += sampleDelta)
	{
		for (float theta = 0.0f; theta < 0.5f * PI; theta += sampleDelta)
		{
			// spherical to cartesian (in tangent space)
			float3 tangentSample = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
			// tangent space to world
			float3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * N;
			//return float4(sampleVec, 1.0f);
			//float3 temp = gCubeMap.Sample(g_Sampler, sampleVec).rgb;
			//irradiance += pow(gCubeMap.Sample(g_Sampler, sampleVec).rgb, float3(2.2f, 2.2f, 2.2f)).rgb * cos(theta) * sin(theta);
			irradiance += gCubeMap.Sample(g_Sampler, sampleVec).rgb * cos(theta) * sin(theta);
			nrSamples++;
		}
	}
	irradiance = PI * irradiance * (1.0f / float(nrSamples));

	return float4(irradiance,1.0f);
}