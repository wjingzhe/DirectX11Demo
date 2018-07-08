#include "ForwardPlus11Common.hlsl"


//Textures,Samplers,and Buffers
Texture2D g_TextureDiffuse:register(t0);
Texture2D g_TextureNormal:register(t1);
SamplerState g_Sampler:register(s0);

//Save two slots for CDXUTSDKMesh diffuse and normal,
// so start with the third slot,t2
Buffer<float4> g_PointLightBufferCenterAndRadius:register(t2);
Buffer<float4> g_PointLightBufferColor:register(t3);
Buffer<float4> g_SpotLightBufferCenterAndRadius:register(t4);
Buffer<float4> g_SpotLightBufferColor:register(t5);
Buffer<float4> g_SpotLightBufferSpotParams:register(t6);
Buffer<uint> g_PerTileLightIndexBuffer:register(t7);


//
// shader input/output structure
//

struct VS_INPUT_SCENE
{
	float3 Position:POSITION; //Vertex Position
	float3 Normal:NORMAL;// Vertex normal
	float2 TextureUV:TEXCOORD0;//vertex texture coords
	float3 Tangent:TANGENT;// vertex tangent
};

struct VS_OUTPUT_SCENE
{
	float4 Position:SV_POSITION;// vertex position
	float3 Normal:NORMAL;// vertex normal
	float2 TextureUV:TEXCOORD0;
	float3 Tangent:TEXCOORD1;
	float4 PositionWS:TEXCOORD2;//
};

struct VS_OUTPUT_POSITION_ONLY
{
	float4 Position:SV_POSITION;
};


struct VS_OUTPUT_POSITION_AND_TEX
{
	float4 Position:SV_POSITION;
	float2 TextureUV:TEXCOORD0;
};

// this shader just transforms position(e.g. for depth pre-pass)
VS_OUTPUT_POSITION_ONLY RenderScenePositionOnlyVS(VS_INPUT_SCENE Input)
{
	VS_OUTPUT_POSITION_ONLY Output;

	// Transform the position from object space to homogenous projection space
	Output.Position = mul(float4(Input.Position, 1), g_mWorldViewProjection);

	return Output;
}

//---------------------------------------------------------------------
// This shader just tranforms position and passes through tex coord
// (e.g. for depth pre-pass with alpha test)
//---------------------------------------------------------------------
VS_OUTPUT_POSITION_AND_TEX RenderScenePositionAndTexVS(VS_INPUT_SCENE Input)
{
	VS_OUTPUT_POSITION_AND_TEX Output;

	//Transform the position from object space to homogenous projection space
	Output.Position = mul(float4(Input.Position, 1), g_mWorldViewProjection);

	// just copy the texture coordinate through
	Output.TextureUV = Input.TextureUV;

	return Output;
}


// this shader transforms position.calculates world-space position,normal,
// and tangent,and passes tex coords through to the pixel shader.
VS_OUTPUT_SCENE RenderSceneVS(VS_INPUT_SCENE Input)
{
	VS_OUTPUT_SCENE Output;

	//Transform the position from object space to homogenous projection space
	Output.Position = mul(float4(Input.Position, 1), g_mWorldViewProjection);

	// Position, normal, and tangent in world space
	Output.PositionWS = mul(float4(Input.Position,1.0f), g_mWorld);
	Output.Normal = mul(Input.Normal, (float3x3)g_mWorld);//without scaling
	Output.Tangent = mul(Input.Tangent, (float3x3)g_mWorld);

	//Just copy the texture coordinate through
	Output.TextureUV = Input.TextureUV;

	return Output;

}

////////////////////////////////////
// Pixel Shader
/////////////////////////////////////
float4 RenderSceneAlphaTestOnlyPS(VS_OUTPUT_POSITION_AND_TEX Input):SV_TARGET
{
	float4 DiffuseTex = g_TextureDiffuse.Sample(g_Sampler,Input.TextureUV);
	float fAlpha = DiffuseTex.a;

	if (fAlpha < g_fAlphaTest)
	{
		discard;
	}

	return DiffuseTex;
}

// This shader doed alpha testing
float4 RenderScenePS(VS_OUTPUT_SCENE pin):SV_TARGET
{
	float3 vPositionWS = pin.PositionWS.xyz;
	float3 AccumDiffuse = float3(0, 0, 0);
	float3 AccumSpecular = float3(0, 0, 0);

	float4 DiffuseTex = g_TextureDiffuse.Sample(g_Sampler, pin.TextureUV);

#if(USE_ALPHA_TEST == 1)
	float fSpecMask = 0.0f;
	float fAlpha = DiffuseTex.a;
	if (fAlpha < g_fAlphaTest)
	{
		discard;
	}

#else

	float fSpecMask = DiffuseTex.a;

#endif

	// Get normal from normal map
	float3 vNorm = g_TextureNormal.Sample(g_Sampler, pin.TextureUV).xyz;
	vNorm *= 2;
	vNorm -= float3(1.0f, 1.0f, 1.0f);

	// transform normal into world space
	float3 vBiNorm = normalize(cross(pin.Normal, pin.Tangent));
	float3x3 BTNMatrix = float3x3(vBiNorm, pin.Tangent, pin.Normal);
	vNorm = normalize(mul(vNorm, BTNMatrix));

	float3 vViewDir = normalize(g_vCameraPos - vPositionWS);

#if(USE_LIGHT_CULLING ==1)
	uint nTileIndex = GetTileIndex(pin.Position.xy);
	uint nIndex = g_uMaxNumLightsPerTile* nTileIndex;
	uint nNextLightIndex = g_PerTileLightIndexBuffer[nIndex];
#else
	uint nIndex;
	uint nNumPointLights = g_uNumLights & 0xFFFFu;
#endif

	// loop over the point lights
	[loop]
#if(USE_LIGHT_CULLING ==1)
	while(nNextLightIndex != LIGHT_INDEX_BUFFER_SENTINEL)
#else
	for (nIndex = 0; nIndex < nNumPointLights; nIndex++)
#endif
	{
#if(USE_LIGHT_CULLING == 1)
		uint nLightIndex = nNextLightIndex;
		nIndex++;
		nNextLightIndex = g_PerTileLightIndexBuffer[nIndex];
#else
		uint nLightIndex = nIndex;
#endif

		float4 LightCenterAndRadius = g_PointLightBufferCenterAndRadius[nLightIndex];

		float3 vToLight = LightCenterAndRadius.xyz - vPositionWS.xyz;
		float3 vToLightDir = normalize(vToLight);
		float fToLightDistance = length(vToLight);


		float3 LightColorDiffuse = float3(0, 0, 0);
		float3 LightColorSpecular = float3(0, 0, 0);

		float fRad = LightCenterAndRadius.w;

		if (fToLightDistance < fRad)
		{
			float x = fToLightDistance / fRad;

			//jingz 表示没看懂
			// fake inverse squared falloff
			// -(1/k)*(1 - (k+1)/(1+k*x^2))
			// k = 20; -(1/20)*(1-21/(1+20*x^2))再转化一下就变成下面的形式
			float fFallOff = -0.05f + 1.05f / (1 + 20 * x*x);
			LightColorDiffuse = g_PointLightBufferColor[nLightIndex].rgb* saturate(dot(vToLightDir, vNorm))*fFallOff;

			float3 vHalfAngle = normalize(vViewDir + vToLightDir);
			LightColorSpecular = g_PointLightBufferColor[nLightIndex].rgb * pow(saturate(dot(vHalfAngle, vNorm)), 8)*fFallOff;

		}

		AccumDiffuse += LightColorDiffuse;
		AccumSpecular += LightColorSpecular;

	}


#if(USE_LIGHT_CULLING == 1)
	// More past the first sentinel to get the spot lights
	nIndex++;
	nNextLightIndex = g_PerTileLightIndexBuffer[nIndex];
#else
	uint nNumSpotLights = (g_uNumLights & 0xFFFF0000u) >> 16;
#endif

	// loop over the spot lights
	[loop]
#if(USE_LIGHT_CULLING == 1)
	while(nNextLightIndex!= LIGHT_INDEX_BUFFER_SENTINEL)
#else
	for (nIndex = 0; nIndex < nNumSpotLights; nIndex++)
#endif
	{

#if(USE_LIGHT_CULLING == 1)
		uint nLightIndex = nNextLightIndex;
		nIndex++;
		nNextLightIndex = g_PerTileLightIndexBuffer[nIndex];

#else
		uint nLightIndex = nIndex;
#endif


		float4 BoundingSphereCenterAndRadius = g_SpotLightBufferCenterAndRadius[nLightIndex];
		float4 SpotParams = g_SpotLightBufferSpotParams[nLightIndex];


		// reconstruct z component of the light dir from x and y;
		float3 SpotLightDir;
		SpotLightDir.xy = SpotParams.xy;
		SpotLightDir.z = sqrt(1 - SpotLightDir.x*SpotLightDir.x - SpotLightDir.y*SpotLightDir.y);

		//the sign bit for cone angle is used to store the sign for the z component of the light dir
		SpotLightDir.z = (SpotParams.z > 0) ? SpotLightDir.z : -SpotLightDir.z;

		// calculate the light position from the bounding sphere
		// (we know the top of the cone is r_bounding_sphere units away from the bounding sphere center along the negated light direction)
		float3 LightPosition = BoundingSphereCenterAndRadius.xyz - BoundingSphereCenterAndRadius.w * SpotLightDir;

		float3 vToLight = LightPosition.xyz - vPositionWS.xyz;
		float3 vToLightNormalized = normalize(vToLight);
		float fLightDistance = length(vToLight);
		float fCosineOfCurrentConeAngle = dot(-vToLightNormalized, SpotLightDir);

		float3 LightColorDiffuse = float3(0, 0, 0);
		float3 LightColorSpecular = float3(0, 0, 0);

		float fRad = SpotParams.w;
		float fCosineOfConeAngle = (SpotParams.z > 0) ? SpotParams.z : -SpotParams.z;
		if (fLightDistance < fRad && fCosineOfCurrentConeAngle > fCosineOfConeAngle)
		{
			float fRadialAttenuation = (fCosineOfCurrentConeAngle - fCosineOfConeAngle) / (1.0 - fCosineOfConeAngle);
			fRadialAttenuation = fRadialAttenuation  * fRadialAttenuation;

			float x = fLightDistance / fRad;
			// fake inverse squared falloff;
			// -(1/k)*(1-(k+1)/(1+k*x^2))
			// k=20； -(1/20)*(1 - 21/(1+20*x^2))
			float fFalloff = -0.05 + 1.05 / (1 + 20 * x*x);
			LightColorDiffuse = g_SpotLightBufferColor[nLightIndex].rgb * saturate(dot(vToLightNormalized, vNorm))*fFalloff * fRadialAttenuation;


			float3 vHalfAngle = normalize(vViewDir + vToLightNormalized);
			LightColorSpecular = g_SpotLightBufferColor[nLightIndex].rgb * pow(saturate(dot(vHalfAngle, vNorm)), 8)*fFalloff*fRadialAttenuation;

		}

		AccumDiffuse += LightColorDiffuse;
		AccumSpecular += LightColorSpecular;

	}



	// pump up the lights
	AccumDiffuse *= 2;
	AccumSpecular *= 8;

	//This is poor man's ambient cubemap(blend between an up color and a down color)
	float fAmbientBlend = 0.5f* vNorm.y + 0.5;
	float3 Ambient = g_MaterialAmbientColorUp.rgb*fAmbientBlend + g_MaterialAmbientColorDown.rgb*(1 - fAmbientBlend);

	//Modulate mesh texture with lighting
	float3 DiffuseAndAmbient = AccumDiffuse + Ambient;
	return float4(DiffuseTex.xyz* (DiffuseAndAmbient + AccumSpecular*fSpecMask), 1);
}
