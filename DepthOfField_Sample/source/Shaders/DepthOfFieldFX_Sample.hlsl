struct S_MODEL_DESC
{
	float4x4 m_World;
	float4x4 m_World_Inv;
	float4x4 m_WorldView;
	float4x4 m_WorldView_Inv;
	float4x4 m_WorldViewProjection;
	float4x4 m_WorldViewProjection_Inv;
	float4 m_Position;
	float4 m_Orientation;
	float4 m_Scale;
	float4 m_Ambient;
	float4 m_Diffuse;
	float4 m_Specular;
};

struct S_CAMERA_DESC
{
	float4x4 m_View;
	float4x4 m_Projection;
	float4x4 m_ViewProjection;
	float4x4 m_View_Inv;
	float4x4 m_Projection_Inv;
	float4x4 m_ViewProjection_Inv;
	float3 m_Position;
	float m_Fov;
	float3 m_Direction;
	float m_FarPlane;
	float3 m_Right;
	float m_NearPlane;
	float3 m_Up;
	float m_Aspect;
	float4 m_Color;
};


struct DirectionalLightInfo
{
	float3 direction;
	float3 color;
	float specPower;
};

static DirectionalLightInfo directionalLights[] =
{
	{ { -0.7943764, -0.32935333, 0.5103845 },{ 1.0, 0.7, 0.6 }, 50.0 },
};

cbuffer CB_MODEL_DATA:register(b0)
{
	S_MODEL_DESC g_Model;
}

cbuffer CB_VIEWER_DATA : register(b1)
{
	S_CAMERA_DESC g_Viewer;
}

//----------------
// Buffers,Texture and Samplers
Texture2D g_t2dDiffuse:register(t0);

SamplerState g_ssLinear:register(s0);

// Shader Structures

struct VS_RenderInput
{
	float3 m_Position:POSITION;
	float3 m_Normal:NORMAL;
	float2 m_TexCoord:TEXCOORD;
};

struct PS_RenderInput
{
	float4 m_Position:SV_Position;
	float3 m_WorldPos:WORLDPOS;
	float3 m_Normal:NORMAL;
	float2 m_TexCoord:TEXCOORD;
};

struct RS_RenderOutput
{
	float4 m_Color:SV_Target0;
};


//
//
//
PS_RenderInput VS_RenderModel(VS_RenderInput In)
{
	PS_RenderInput Out;

	// Transform the position from object space to homogeneous projection space
	Out.m_Position = mul(float4(In.m_Position, 1.0f), g_Model.m_WorldViewProjection);
	Out.m_WorldPos = mul(float4(In.m_Position, 1.0f), g_Model.m_World).xyz;

	// Transform the normal from object space to world space
	Out.m_Normal = normalize(mul(In.m_Normal, (float3x3)g_Model.m_World));

	// Pass through texture coords
	Out.m_TexCoord = In.m_TexCoord;

	return Out;

}

void CalcDirectionalLight(in const DirectionalLightInfo lightInfo,
	in const float3 worldPos, in const float3 normal, in out float3 lightColor)
{
	lightColor += saturate(dot(normal, -lightInfo.direction))*lightInfo.color* g_Model.m_Diffuse.rgb;
	float3 lightReflect = normalize(reflect(lightInfo.direction, normal));
	float spec = saturate(dot(normalize(g_Viewer.m_Position - worldPos), lightReflect));
	float specFactor = pow(spec, lightInfo.specPower);
	lightColor += specFactor*lightInfo.color*g_Model.m_Specular.rgb;
}

RS_RenderOutput PS_RenderModel(PS_RenderInput In)
{
	float4 texColor = float4(1, 1, 1, 1);
	texColor = g_t2dDiffuse.Sample(g_ssLinear, In.m_TexCoord);

	float3 lightColor = float3(0, 0, 0);

	CalcDirectionalLight(directionalLights[0], In.m_WorldPos, In.m_Normal, lightColor);

	float3 ambient = float3(0, 0, 0);
	ambient += lerp(float3(0.08, 0.08, 0.05), float3(0.09, 0.1, 0.33), In.m_Normal.y*0.5 + 0.5);

	float3 color = (lightColor + ambient);
	color *= texColor.xyz;

	RS_RenderOutput Out;

	Out.m_Color = float4(color, 1.0f);

	return Out;
}