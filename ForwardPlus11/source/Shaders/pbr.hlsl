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
	float g_Roughness : packoffset(c1.z);

	float3 g_LightPositions[4]: packoffset(c2);
	float3 g_LightColors[4]: packoffset(c6);

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
	float2 TextureUV:TEXCOORD0;
	float3 Normal: TEXCOORD1;
	float3 PositionW:TEXCOORD2;//World position
};

VS_OUTPUT PbrVS(VS_INPUT vin)
{

	VS_OUTPUT vout;
	//vout.PositionH = float4(vin.PositionL, 1.0f);
	vout.PositionH = mul(float4(vin.PositionL, 1.0f), g_mWolrdViewProjection);
	vout.PositionW = mul(float4(vin.PositionL, 1.0f), g_mWolrd).xyz;
	vout.TextureUV = float2(vin.TextureUV.x, 1.0f - vin.TextureUV.y);
	vout.Normal = mul(vin.NormalL, (float3x3)g_mWolrd);

	return vout;
}





TextureCube g_IrradianceMap:register(t0);
TextureCube g_PrefilterMap:register(t1);
Texture2D g_BrdfLUT:register(t2);

Texture2D g_AlbedoMap:register(t3);
Texture2D g_NormalMap:register(t4);
Texture2D g_MetallicMap:register(t5);
Texture2D g_RoughnessMap:register(t6);
Texture2D g_AmbientOverlapMap:register(t7);


SamplerState g_SamplerLinear:register(s0);
SamplerState g_SamplerPoint:register(s1);

static const float PI = 3.141591653f;

float3 GetNormalFromMap(VS_OUTPUT pin)
{
	float3 tangentNormal = g_NormalMap.Sample(g_SamplerLinear, pin.TextureUV).xyz*2.0f - 1.0f;

	float3 Q1 = ddx(pin.PositionW);
	float3 Q2 = ddy(pin.PositionW);
	float2 st1 = ddx(pin.TextureUV);
	float2 st2 = ddy(pin.TextureUV);

	float3 N = normalize(pin.Normal);
	float3 T = normalize(Q1*st2.y-Q2*st1.y);
	float3 B = -normalize(cross(N, T));

	float3x3 TBN = float3x3(T, B, N);

	return normalize(mul(tangentNormal, TBN));

}

float DistributionGGX(float3 N, float3 H, float roughness)
{
	float a = roughness*roughness;
	float a2 = a*a;
	float NdotH = max(dot(N, H), 0.0f);
	float NdotH2 = NdotH*NdotH;

	float nom = a2;
	float denom = (NdotH2 * (a2 - 1.0f) + 1.0f);
	denom = PI*denom*denom;

	return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
	float r = (roughness + 1.0f);
	float k = (r*r) / 8.0f;

	float nom = NdotV;
	float denom = NdotV* (1.0f - k) + k;

	return nom / denom;
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
	float NdotV = max(dot(N, V), 0.0f);
	float NdotL = max(dot(N, L), 0.0f);
	float ggx2 = GeometrySchlickGGX(NdotV, roughness);
	float ggx1 = GeometrySchlickGGX(NdotL, roughness);

	return ggx1*ggx2;
}

float3 FresnelSchlick(float cosTheta, float3 F0)
{
	return F0 + (1.0f - F0)*pow(1.0f - cosTheta, 5.0f);
}

float3 FresnelSchlick(float3 N, float3 H, float3 F0)
{
	float VdotH = dot(H, N);

	float ExponentFactor = (-5.5473f*VdotH - 6.98316)*VdotH;
	
	return F0 + (1.0f - F0)*exp2(ExponentFactor);
}

float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
	return F0 + (max(float3(1.0f - roughness, 1.0f - roughness, 1.0f - roughness), F0) - F0)*pow(1.0f - cosTheta, 5.0f);
}

float4 PbrPS(VS_OUTPUT pin) :SV_TARGET
{
	//float4 Albedo2 = pow(g_AlbedoMap.Sample(g_SamplerLinear,float2(pin.TextureUV.x,1.0f - pin.TextureUV.y)).rgba,float4(2.2f,2.2f,2.2f,2.2f));
	float3 Albedo = g_AlbedoMap.Sample(g_SamplerLinear,pin.TextureUV.xy).rgb;
	float Metallic = g_MetallicMap.Sample(g_SamplerLinear, pin.TextureUV).r;
	float Roughness = g_RoughnessMap.Sample(g_SamplerLinear, pin.TextureUV).r;
	float AO = g_AmbientOverlapMap.Sample(g_SamplerLinear, pin.TextureUV).r;

	//float3 Albedo = pow(pow(g_AlbedoMap.Sample(g_SamplerLinear, pin.TextureUV.xy).rgb, float3(2.2f, 2.2f, 2.2f)), float3(2.2f, 2.2f, 2.2f));
	//float Metallic = pow(g_MetallicMap.Sample(g_SamplerLinear, pin.TextureUV).r, 2.2f);
	//float Roughness = pow(g_RoughnessMap.Sample(g_SamplerLinear, pin.TextureUV).r, 2.2f);;
	//float AO = pow(g_AmbientOverlapMap.Sample(g_SamplerLinear, pin.TextureUV).r, 2.2f);;

	// input lighting data
	float3 N = GetNormalFromMap(pin);
	float3 V = normalize(g_vCameraPos.xyz - pin.PositionW.xyz);
	float3 R = reflect(-V, N);

	// calculate reflectance at normal incidence;if dia-electric(like plastic) use F0
	// of 0.04 and if it's a metal,use the albedo color as F0(metallic workflow)
	float3 F0 = float3(0.04f, 0.04f, 0.04f);
	F0 = lerp(F0, Albedo, Metallic);
	
	//reflectance equation
	float3 Lo = float3(0.0f, 0.0f, 0.0f);
	for (int i = 0; i < 4; ++i)
	{
		// calculate per-light radiance
		float3 L = normalize(g_LightPositions[i].xyz - pin.PositionW.xyz);
		float3 H = normalize(V + L);
		float distance = length(g_LightPositions[i].xyz - pin.PositionW.xyz);
		float attenuation = 1.0f / (distance*distance);
		float radiance = g_LightColors[i] * attenuation;

		// Cook-Torrance BRDF
		float NDF = DistributionGGX(N, H, Roughness);
		float G = GeometrySmith(N, V, L, Roughness);
		float3 F = FresnelSchlick(max(dot(H, V), 0.0f), F0);

		float3 nominator = NDF*G*F;
		float denominator = 4 * max(dot(N, V), 0.0f)*max(dot(N, L), 0.0f) + 0.001f;//0.001f to prevent divide by zero
		float3 specular = nominator / denominator;

		// kS is equal to Fresnel
		float3 kS = F;

		// for enery conservation,the diffuse and specular light can't
		// be above 1.0f (unless the surface emits light); to preserve this
		// relationship the diffuse component (kD) should equal 1.0f-kS.
		float3 kD = float3(1.0f, 1.0f, 1.0f) - kS;

		// multiply kD by the inverse metalness such that only non-metals
		// have diffuse lighting, or a linear blend if partly metal (pure
		// metals have no diffuse light).
		kD *= 1.0f - Metallic;

		// scale light by NdotL
		float NdotL = max(dot(N, L), 0.0f);

		// add to outgoing radiance Lo
		Lo += (kD*Albedo / PI + specular)*radiance*NdotL;// note that we already multiplied the BRDF by the Fresnel(kS) so we won't mulptiply by kS again


	}

	// ambient lighting (we now use IBL as the ambient term)
	float3 F = FresnelSchlickRoughness(max(dot(N, V), 0.0), F0, Roughness);

	float3 kS = F;
	float3 kD = 1.0f - kS;
	kD *= 1.0f - Metallic;

	float3 irradiance = g_IrradianceMap.Sample(g_SamplerLinear, N).rgb;
	float3 diffuse = irradiance * Albedo;

	// sample both the pre-filter map and the BRDF lut and combine them together as per the Split-Sum approximation to get the IBL specular part.
	const float MAX_REFLECTION_LOD = 4.0f;
	float3 prefilteredColor = g_PrefilterMap.SampleLevel(g_SamplerLinear, R, Roughness*MAX_REFLECTION_LOD).rgb;
	float2 brdf = g_BrdfLUT.Sample(g_SamplerLinear, float2(max(dot(N,V),0.0f), Roughness)).rg;
	float3 specular = prefilteredColor * (F*brdf.x + brdf.y);

	float3 ambient = (kD*diffuse + specular)*AO;

	float3 color = ambient + Lo;

	//HDR toneMapping
	//color = color / (color + float3(1.0f, 1.0f, 1.0f));

	//gamma correct
	color = pow(color, float3(1.0f / 2.2f, 1.0f / 2.2f, 1.0f / 2.2f));
	//float3 tangentNormal = g_NormalMap.Sample(g_SamplerLinear, pin.TextureUV).xyz * 2.0f - 1.0f;
	/*float3 tangentNormal = GetNormalFromMap(pin);
	return float4(tangentNormal,1.0f);*/
	return float4(color,1.0f);

}

//float4 PbrPS(VS_OUTPUT pin) :SV_TARGET
//{
//	return float4(0.01f,0.0f, 0.0f,1.0f);
//}