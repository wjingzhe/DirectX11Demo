cbuffer cbPerObject:register(b0)
{
	matrix g_mWolrdViewProjection:packoffset(c0);
	matrix g_mWolrd:packoffset(c4);

};
cbuffer cbPerFrame : register(b1)
{
	float3 g_vCameraPos:packoffset(c0);
	float g_fAlphaTest : packoffset(c0.w);
	float g_ScreenWidth : packoffset(c1.x);
	float g_ScreenHeight : packoffset(c1.y);
	float g_roughness : packoffset(c1.z);
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
};

VS_OUTPUT CubeMapPrefilterVS(VS_INPUT vin)
{

	VS_OUTPUT vout;
	//vout.PositionH = float4(vin.PositionL, 1.0f);
	vout.PositionH = mul(float4(vin.PositionL, 1.0f), g_mWolrdViewProjection);
	vout.PositionW = vin.PositionL;

	return vout;
}


TextureCube gCubeMap:register(t0);
SamplerState g_Sampler:register(s0);


static const float PI = 3.141591653f;

float DistributionGGX(float3 N, float3 H, float roughness)
{
	float a = roughness* roughness;
	float a2 = a*a;
	float NdotH = max(dot(N, H), 0.0);
	float NdotH2 = NdotH*NdotH;

	float nom = a2;
	float denom = (NdotH2 * (a2 - 1.0) + 1.0);
	denom = PI*denom*denom;

	return nom / denom;

}

// ----------------------------------------------------------------------------
// http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
// efficient VanDerCorpus calculation.
float RadicalInverse_VdC(uint bits)
{
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

float2 Hammersley(uint i, uint N)
{
	return float2(float(i) / float(N), RadicalInverse_VdC(i));
}

float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness)
{
	float a = roughness * roughness;

	float phi = 2.0f* PI*Xi.x;
	float cosTheta = sqrt((1.0f - Xi.y) / (1.0f + (a*a - 1.0f)*Xi.y));
	float sinTheta = sqrt(1.0f - cosTheta*cosTheta);

	//from spherical coordinates to cartesian coordiantes - halfway vector
	float3 H;
	H.x = cos(phi)*sinTheta;
	H.y = sin(phi)*sinTheta;
	H.z = cosTheta;

	//from tangent-space H vector to world-space sample vector
	float3 up = abs(N.z) < 0.999f ? float3(0.0f, 0.0f, 1.0f) : float3(1.0f, 0.0f, 0.0f);
	float3 tangent = normalize(cross(up, N));
	float3 bitangent = cross(N, tangent);

	float3 sampleVec = tangent * H.x + bitangent*H.y + N*H.z;

	return normalize(sampleVec);
}


float4 CubeMapPrefilterPS(VS_OUTPUT pin) :SV_TARGET
{


	float3 N = normalize(pin.PositionW);

	float3 R = N;
	float3 V = R;

	const uint SAMPLE_COUNT = 1024u;
	float3 prefilteredColor = float3(0.0f, 0.0f, 0.0f);

	float totalWeight = 0.0f;

	for (uint i = 0u; i < SAMPLE_COUNT; ++i)
	{
		float2 Xi = Hammersley(i, SAMPLE_COUNT);
		float3 H = ImportanceSampleGGX(Xi, N, g_roughness);
		float3 L = normalize(2.0f*dot(V, H)*H - V);


		float NdotL = max(dot(N, L), 0.0);

		if (NdotL > 0.0)
		{
			// sample from the environment's mip level based on roughness/pdf
			float D = DistributionGGX(N, H, g_roughness);
			float NdotH = max(dot(N, H), 0.0);
			float HdotV = max(dot(H, V), 0.0);
			float pdf = D*NdotH / (4.0*HdotV) + 0.0001f;


			float resolution = 512.0f;
			float saTexel = 4.0f* PI/(6.0f*resolution*resolution);
			float saSample = 1.0f / (float(SAMPLE_COUNT)*pdf + 0.0001f);

			float mipLevel = g_roughness <= 1E-5 ? 0.0f : 0.5*log2(saSample / saTexel);

			//prefilteredColor += pow(gCubeMap.SampleLevel(g_Sampler, L, mipLevel).rgb, float3(2.2f,2.2f,2.2f)).rgb*NdotL;
			prefilteredColor += gCubeMap.SampleLevel(g_Sampler, L, mipLevel).rgb.rgb*NdotL;
			totalWeight += NdotL;
		}

	}



	prefilteredColor = prefilteredColor / totalWeight;

	//prefilteredColor = pow(prefilteredColor, float3(2.2f,2.2f,2.2f));

	return float4(prefilteredColor,1.0f);
}