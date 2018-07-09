#include "stdafx.h"
#include "ForwardPlusRender.h"
#include "../../amd_sdk/inc/AMD_SDK.h"
#include "../../DXUT/Optional/DXUTcamera.h"
#include "../../DXUT/Optional/SDKmesh.h"
#include <stdlib.h>

using namespace DirectX;

// these are half-precision(i.e. 16-bit) float values,store as unsigned shorts
struct SpotParams
{
	unsigned short fLightDirX;
	unsigned short fLightDirY;
	unsigned short fCosineOfConeAngleAndLightDirZSign;
	unsigned short fFalloffRadius;
};

static XMFLOAT4 g_PointLightCenterAndRadius[ForwardPlus11::MAX_NUM_LIGHTS];
static DWORD g_PointLightColors[ForwardPlus11::MAX_NUM_LIGHTS];

static XMFLOAT4 g_SpotLightCenterAndRadius[ForwardPlus11::MAX_NUM_LIGHTS];
static DWORD g_SpotLightColors[ForwardPlus11::MAX_NUM_LIGHTS];
static SpotParams g_SpotLightSpotParams[ForwardPlus11::MAX_NUM_LIGHTS];

//miscellaneous constants
static const float TWO_PI = 6.28318530718f;


static const float GetRandFloat(float fRangeMin,float fRangeMax)
{
	//Generate random numbers in the half-closed interval
	//[RangeMin,RangeMax) In other words
	// rangeMin <= random number < randeMax
	return (float)rand() / (RAND_MAX+1)*(fRangeMax - fRangeMin) + fRangeMin;
}

static DWORD GetRandColor()
{
	static unsigned uCounter = 0;
	uCounter++;
	XMFLOAT4 color;
	if (uCounter % 2 == 0)
	{
		//Since green contributers the most to perceived brightness,
		// cap it's min value to avoid overly dim lights
		color = XMFLOAT4(GetRandFloat(0.0f, 1.0f), GetRandFloat(0.27f, 1.0f), GetRandFloat(0.0f, 1.0f), 1.0f);
	}
	else
	{
		// else ensure the red component has a large value,again to avoid overly dim lights
		color = XMFLOAT4(GetRandFloat(0.9f, 1.0f), GetRandFloat(0.0f, 1.0f), GetRandFloat(0.0f, 1.0f), 1.0f);
	}

	DWORD dwR = (DWORD)(color.x*255.0f + 0.5f);
	DWORD dwG = (DWORD)(color.y*255.0f + 0.5f);
	DWORD dwB = (DWORD)(color.z*255.0f + 0.5f);
	DWORD dwA = (DWORD)(color.w*255.0f + 0.5f);

	// A/B/G/R format(i.e. DXGI_FORMAT_R8G8B8A8_UNORM)
	return (dwA << 24) | (dwB << 16) | (dwG << 8) | dwR;
}

static XMFLOAT3 GetRandLightDirection()
{
	static unsigned uCounter = 0;
	uCounter++;

	XMFLOAT3 vLightDir;
	vLightDir.x = GetRandFloat(-1.0f, 1.0f);
	vLightDir.y = GetRandFloat(0.1f, 1.0f);
	vLightDir.z = GetRandFloat(-1.0f, 1.0f);

	if (uCounter % 2 == 0)
	{
		vLightDir.y = -vLightDir.y;
	}

	XMFLOAT3 vResult;
	XMVECTOR NormalizedLightDir = XMVector3Normalize(XMLoadFloat3(&vLightDir));
	XMStoreFloat3(&vResult, NormalizedLightDir);

	return vResult;
}

//jingz 反正没看懂
static SpotParams PackSpotParams(const XMFLOAT3& vLightDir, float fCosineOfConeAngle, float fFalloffRadius)
{
	assert(fCosineOfConeAngle > 0.0f);
	assert(fFalloffRadius > 0.0f);

	SpotParams PackedParams;
	PackedParams.fLightDirX = AMD::ConvertF32ToF16(vLightDir.x);
	PackedParams.fLightDirY = AMD::ConvertF32ToF16(vLightDir.y);
	PackedParams.fCosineOfConeAngleAndLightDirZSign = AMD::ConvertF32ToF16(fCosineOfConeAngle);
	PackedParams.fFalloffRadius = AMD::ConvertF32ToF16(fFalloffRadius);

	// put the sign bit for light dir z in the sign bit for the cone angle
	// (we can do this because we know the cone angle is always positive)
	if (vLightDir.z < 0.0f)
	{
		PackedParams.fCosineOfConeAngleAndLightDirZSign |= 0x8000;
	}
	else
	{
		PackedParams.fCosineOfConeAngleAndLightDirZSign &= 0x7FFF;
	}

	return PackedParams;
}
namespace ForwardPlus11
{
	ForwardPlusRender::ForwardPlusRender()
		:m_uWidth(0),
		m_uHeight(0),
		bSolutionSized(false),
		//Buffers for light culling
		m_pLightIndexBuffer(nullptr),
		m_pLightIndexBufferSRV(nullptr),
		m_pLightIndexBufferUAV(nullptr),
		//constant buffer
		m_pCbPerObject(nullptr),
		m_pCbPerFrame(nullptr),
		//Point Light
		m_pPointLightBufferCenterAndRadius(nullptr),
		m_pPointLightBufferCenterAndRadiusSRV(nullptr),
		m_pPointLightBufferColor(nullptr),
		m_pPointLightBufferColorSRV(nullptr),
		//Spot lights
		m_pSpotLightBufferCenterAndRadius(nullptr),
		m_pSpotLightBufferCenterAndRadiusSRV(nullptr),
		m_pSpotLightBufferColor(nullptr),
		m_pSpotLightBufferColorSRV(nullptr),
		m_pSpotLightBufferParams(nullptr),
		m_pSpotLightBufferParamsSRV(nullptr),
		//SamplerState
		m_pSamAnisotropic(nullptr),
		//DepthStencilState
		m_pDepthLess(nullptr),
		m_pDepthEqualAndDisableDepthWrite(nullptr),
		m_pDisableDepthWrite(nullptr),
		m_pDisableDepthTest(nullptr),
		// Blend States
		m_pAdditiveBS(nullptr),
		m_pOpaqueBS(nullptr),
		m_pDepthOnlyBS(nullptr),
		m_pDepthOnlyAlphaToCoverageBS(nullptr),
		//Rasterize states
		m_pDisableCullingRS(nullptr),
		//Vertex Shader
		m_pScenePositionOnlyVS(nullptr),
		m_pScenePositionAndTextureVS(nullptr),
		m_pSceneMeshVS(nullptr),
		//Pixel Shader
		m_pSceneNoAlphaTestAndLightCullPS(nullptr),
		m_pSceneAlphaTestAndLightCullPS(nullptr),
		m_pSceneNoAlphaTestAndNoLightCullPS(nullptr),
		m_pSceneAlphaTestAndNoLightCullPS(nullptr),
		m_pSceneAlphaTestOnlyPS(nullptr),
		// Compute Shader
		m_pLightCullCS(nullptr),
		m_pLightCullMSAA_CS(nullptr),
		m_pLightCullNoDepthCS(nullptr),
		//InputLayout
		m_pPositionOnlyInputLayout(nullptr),
		m_pPositionAndTexInputLayout(nullptr),
		m_pMeshInputLayout(nullptr),
		// Depth buffer data
		m_pDepthStencilTexture(nullptr),
		m_pDepthStencilView(nullptr),
		m_pDepthStencilSRV(nullptr),

		bShaderInited(false),
		bIsLightInited(false)
	{

	}

	ForwardPlusRender::~ForwardPlusRender()
	{
		ReleaseAllD3D11COM();
	}



	void ForwardPlusRender::CalculateSceneMinMax(CDXUTSDKMesh& Mesh, DirectX::XMVECTOR * pBBoxMinOut, DirectX::XMVECTOR * pBBoxMaxOut)
	{
		*pBBoxMinOut = Mesh.GetMeshBBoxCenter(0) - Mesh.GetMeshBBoxExtents(0);
		*pBBoxMaxOut = Mesh.GetMeshBBoxCenter(0) + Mesh.GetMeshBBoxExtents(0);

		for (unsigned int i = 0; i < Mesh.GetNumMeshes(); ++i)
		{
			XMVECTOR vNewMin = Mesh.GetMeshBBoxCenter(i) - Mesh.GetMeshBBoxExtents(i);
			XMVECTOR vNewMax = Mesh.GetMeshBBoxCenter(i) + Mesh.GetMeshBBoxExtents(i);

			*pBBoxMinOut = XMVectorMin(vNewMin, *pBBoxMinOut);
			*pBBoxMaxOut = XMVectorMax(vNewMax, *pBBoxMaxOut);

		}
	}

	//jingz 聚光灯相关就是没看懂
	void ForwardPlusRender::InitRandomLights(const DirectX::XMVECTOR & BBoxMin, const DirectX::XMVECTOR & BBoxMax)
	{
		// Init the random seed to 1, so that resultes are deterministic across different runs of the sample
		srand(1);

		//scale the size of the lights based on the size of scene
		XMVECTOR BBoxExtend = 0.5f * (BBoxMax - BBoxMin);
		float fRadius = 0.075f*XMVectorGetX(XMVector3Length(BBoxExtend));

		// For point lights, the radius of the bounding sphere for the light(used for culling)
		// and the falloff distance of the light(used for lighting) are the same.
		// Not so for spot lights.A spot light is a right circular cone,The height of the cone is the
		// falloff distance.We want to fit the cone of the spot light inside the bounding sphere.
		// From calculus,we know the cone with maximum volume that can fit inside a sphere has height
		//h_cone = (4/3)*r_sphere
		float fSpotLightFalloffRadius = 1.3333333333333333f* fRadius;

		XMFLOAT3 vBBoxMin, vBBoxMax;
		XMStoreFloat3(&vBBoxMin, BBoxMin);
		XMStoreFloat3(&vBBoxMax, BBoxMax);

		//Initialize the point light data
		for (int i = 0; i < MAX_NUM_LIGHTS; ++i)
		{
			g_PointLightCenterAndRadius[i] = XMFLOAT4(GetRandFloat(vBBoxMin.x, vBBoxMax.x), GetRandFloat(vBBoxMin.y, vBBoxMax.y), GetRandFloat(vBBoxMin.z, vBBoxMax.z), fRadius);
			g_PointLightColors[i] = GetRandColor();


		}

		for (int i = 0; i < MAX_NUM_LIGHTS; ++i)
		{
			g_SpotLightCenterAndRadius[i] = XMFLOAT4(GetRandFloat(vBBoxMin.x, vBBoxMax.x), GetRandFloat(vBBoxMin.y, vBBoxMax.y), GetRandFloat(vBBoxMin.z, vBBoxMax.z), fRadius);
			g_SpotLightColors[i] = GetRandColor();

			XMFLOAT3 vLightDir = GetRandLightDirection();

			// random direction,cosine of cone angle,falloff radius calculated above
			g_SpotLightSpotParams[i] = PackSpotParams(vLightDir, 0.816496580927726f, fSpotLightFalloffRadius);

		}

	}

	void ForwardPlusRender::AddShaderToCache(AMD::ShaderCache * pShaderCache)
	{
		//Ensure all shaders(and input layouts) are released
		ReleaseShaderCachedCOM();

		AMD::ShaderCache::Macro ShaderMacros[2];
		wcscpy_s(ShaderMacros[0].m_wsName, AMD::ShaderCache::m_uMACRO_MAX_LENGTH, L"USE_ALPHA_TEST");
		wcscpy_s(ShaderMacros[1].m_wsName, AMD::ShaderCache::m_uMACRO_MAX_LENGTH, L"USE_LIGHT_CULLING");



		const D3D11_INPUT_ELEMENT_DESC layout[] =
		{
			{ "POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0 },
			{ "NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0 },
			{ "TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0 },
			{ "TANGENT",0,DXGI_FORMAT_R32G32B32_FLOAT,0,32,D3D11_INPUT_PER_VERTEX_DATA,0 }
		};

		pShaderCache->AddShader((ID3D11DeviceChild**)&m_pScenePositionOnlyVS, AMD::ShaderCache::SHADER_TYPE_VERTEX, L"vs_5_0", L"RenderScenePositionOnlyVS",
			L"ForwardPlus11.hlsl", 0, nullptr, &m_pPositionOnlyInputLayout, (D3D11_INPUT_ELEMENT_DESC*)layout, ARRAYSIZE(layout));

		pShaderCache->AddShader((ID3D11DeviceChild**)&m_pScenePositionAndTextureVS, AMD::ShaderCache::SHADER_TYPE_VERTEX, L"vs_5_0", L"RenderScenePositionAndTexVS",
			L"ForwardPlus11.hlsl", 0, nullptr, &m_pPositionAndTexInputLayout, (D3D11_INPUT_ELEMENT_DESC*)layout, ARRAYSIZE(layout));

		pShaderCache->AddShader((ID3D11DeviceChild**)&m_pSceneMeshVS, AMD::ShaderCache::SHADER_TYPE_VERTEX, L"vs_5_0", L"RenderSceneVS",
			L"ForwardPlus11.hlsl", 0, nullptr, &m_pMeshInputLayout, (D3D11_INPUT_ELEMENT_DESC*)layout, ARRAYSIZE(layout));


		//Pixel Shader
		ShaderMacros[0].m_iValue = 0;
		ShaderMacros[1].m_iValue = 1;
		pShaderCache->AddShader((ID3D11DeviceChild**)&m_pSceneNoAlphaTestAndLightCullPS, AMD::ShaderCache::SHADER_TYPE_PIXEL, L"ps_5_0", L"RenderScenePS", L"ForwardPlus11.hlsl", 2, ShaderMacros, nullptr, nullptr, 0);

		ShaderMacros[0].m_iValue = 1;
		ShaderMacros[1].m_iValue = 1;
		pShaderCache->AddShader((ID3D11DeviceChild**)&m_pSceneAlphaTestAndLightCullPS, AMD::ShaderCache::SHADER_TYPE_PIXEL, L"ps_5_0", L"RenderScenePS", L"ForwardPlus11.hlsl", 2, ShaderMacros, nullptr, nullptr, 0);

		ShaderMacros[0].m_iValue = 0;
		ShaderMacros[1].m_iValue = 0;
		pShaderCache->AddShader((ID3D11DeviceChild**)&m_pSceneNoAlphaTestAndNoLightCullPS, AMD::ShaderCache::SHADER_TYPE_PIXEL, L"ps_5_0", L"RenderScenePS", L"ForwardPlus11.hlsl", 2, ShaderMacros, nullptr, nullptr, 0);

		ShaderMacros[0].m_iValue = 1;
		ShaderMacros[1].m_iValue = 0;
		pShaderCache->AddShader((ID3D11DeviceChild**)&m_pSceneAlphaTestAndNoLightCullPS, AMD::ShaderCache::SHADER_TYPE_PIXEL, L"ps_5_0", L"RenderScenePS", L"ForwardPlus11.hlsl", 2, ShaderMacros, nullptr, nullptr, 0);


		pShaderCache->AddShader((ID3D11DeviceChild**)&m_pSceneAlphaTestOnlyPS, AMD::ShaderCache::SHADER_TYPE_PIXEL, L"ps_5_0", L"RenderSceneAlphaTestOnlyPS",
			L"ForwardPlus11.hlsl", 0, nullptr, nullptr, nullptr, 0);



		AMD::ShaderCache::Macro ShaderMacroUseDepthBounds;
		wcscpy_s(ShaderMacroUseDepthBounds.m_wsName, AMD::ShaderCache::m_uMACRO_MAX_LENGTH, L"USE_DEPTH_BOUNDS");
		// Compute Shader
		ShaderMacroUseDepthBounds.m_iValue = 1;
		pShaderCache->AddShader((ID3D11DeviceChild**)&m_pLightCullCS, AMD::ShaderCache::SHADER_TYPE_COMPUTE, L"cs_5_0", L"CullLightsCS", L"ForwardPlus11Tiling.hlsl", 1, &ShaderMacroUseDepthBounds, nullptr, nullptr, 0);

		ShaderMacroUseDepthBounds.m_iValue = 2;
		pShaderCache->AddShader((ID3D11DeviceChild**)&m_pLightCullMSAA_CS, AMD::ShaderCache::SHADER_TYPE_COMPUTE, L"cs_5_0", L"CullLightsCS", L"ForwardPlus11Tiling.hlsl", 1, &ShaderMacroUseDepthBounds, nullptr, nullptr, 0);

		ShaderMacroUseDepthBounds.m_iValue = 0;
		pShaderCache->AddShader((ID3D11DeviceChild**)&m_pLightCullNoDepthCS, AMD::ShaderCache::SHADER_TYPE_COMPUTE, L"cs_5_0", L"CullLightsCS", L"ForwardPlus11Tiling.hlsl", 1, &ShaderMacroUseDepthBounds, nullptr, nullptr, 0);

	}




	HRESULT ForwardPlusRender::OnCreateDevice(ID3D11Device * pD3DDeive, AMD::ShaderCache* pShaderCache, XMVECTOR SceneMin, XMVECTOR SceneMax)
	{
		HRESULT hr;

		if (!bIsLightInited)
		{
			this->InitRandomLights(SceneMin, SceneMax);
			bIsLightInited = true;
		}


		D3D11_SUBRESOURCE_DATA InitData;

		//create the point light(center and radius)
		D3D11_BUFFER_DESC LightBufferDesc;
		ZeroMemory(&LightBufferDesc, sizeof(LightBufferDesc));
		LightBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
		LightBufferDesc.ByteWidth = sizeof(g_PointLightCenterAndRadius);
		LightBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		InitData.pSysMem = g_PointLightCenterAndRadius;
		V_RETURN(pD3DDeive->CreateBuffer(&LightBufferDesc, &InitData, &m_pPointLightBufferCenterAndRadius));
		m_pPointLightBufferCenterAndRadius->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("PointLightCenterAndRadius"), "PointLightCenterAndRadius");


		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
		ZeroMemory(&SRVDesc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
		SRVDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		SRVDesc.Buffer.ElementOffset = 0;
		SRVDesc.Buffer.ElementWidth = MAX_NUM_LIGHTS;
		V_RETURN(pD3DDeive->CreateShaderResourceView(m_pPointLightBufferCenterAndRadius, &SRVDesc, &m_pPointLightBufferCenterAndRadiusSRV));
		m_pPointLightBufferCenterAndRadiusSRV->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("PointLightCenterAndRadiusSRV"), "PointLightCenterAndRadiusSRV");


		// create the point light buffer(color)
		LightBufferDesc.ByteWidth = sizeof(g_PointLightCenterAndRadius);
		InitData.pSysMem = g_PointLightColors;
		V_RETURN(pD3DDeive->CreateBuffer(&LightBufferDesc, &InitData, &m_pPointLightBufferColor));
		m_pPointLightBufferColor->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("PointLightBufferColor"), "PointLightBufferColor");

		SRVDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		V_RETURN(pD3DDeive->CreateShaderResourceView(m_pPointLightBufferColor, &SRVDesc, &m_pPointLightBufferColorSRV));
		m_pPointLightBufferColorSRV->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("PointLightBufferColorSRV"), "PointLightBufferColorSRV");


		// Create spot light buffer(center and radius)
		ZeroMemory(&LightBufferDesc, sizeof(LightBufferDesc));
		LightBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
		LightBufferDesc.ByteWidth = sizeof(g_SpotLightCenterAndRadius);
		LightBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		InitData.pSysMem = g_SpotLightCenterAndRadius;
		V_RETURN(pD3DDeive->CreateBuffer(&LightBufferDesc, &InitData, &m_pSpotLightBufferCenterAndRadius));
		m_pSpotLightBufferCenterAndRadius->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("SpotLightBufferCenterAndRadius"), "SpotLightBufferCenterAndRadius");

		ZeroMemory(&SRVDesc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
		SRVDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		SRVDesc.Buffer.ElementOffset = 0;
		SRVDesc.Buffer.ElementWidth = MAX_NUM_LIGHTS;
		V_RETURN(pD3DDeive->CreateShaderResourceView(m_pSpotLightBufferCenterAndRadius, &SRVDesc, &m_pSpotLightBufferCenterAndRadiusSRV));
		m_pSpotLightBufferCenterAndRadiusSRV->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("SpotLightBufferCenterAndRadiusSRV"), "SpotLightBufferCenterAndRadiusSRV");


		// Create the spot light buffer(color)
		LightBufferDesc.ByteWidth = sizeof(g_SpotLightColors);
		InitData.pSysMem = g_SpotLightColors;
		V_RETURN(pD3DDeive->CreateBuffer(&LightBufferDesc, &InitData, &m_pSpotLightBufferColor));
		m_pSpotLightBufferColor->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("SpotLightBufferColor"), "SpotLightBufferColor");

		SRVDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		V_RETURN(pD3DDeive->CreateShaderResourceView(m_pSpotLightBufferColor, &SRVDesc, &m_pSpotLightBufferColorSRV));
		m_pSpotLightBufferColorSRV->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("SpotLightBufferColorSRV"), "SpotLightBufferColorSRV");


		// create the spot light buffer(spot light parameters)
		LightBufferDesc.ByteWidth = sizeof(g_SpotLightSpotParams);
		InitData.pSysMem = g_SpotLightSpotParams;
		V_RETURN(pD3DDeive->CreateBuffer(&LightBufferDesc, &InitData, &m_pSpotLightBufferParams));
		m_pSpotLightBufferParams->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("SpotLightBufferParams"), "SpotLightBufferParams");


		SRVDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		V_RETURN(pD3DDeive->CreateShaderResourceView(m_pSpotLightBufferParams, &SRVDesc, &m_pSpotLightBufferParamsSRV));
		m_pSpotLightBufferParamsSRV->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("SpotLightBufferParamsSRV"), "SpotLightBufferParamsSRV");


		//Create constant buffers
		D3D11_BUFFER_DESC CbDesc;
		ZeroMemory(&CbDesc, sizeof(CbDesc));
		CbDesc.Usage = D3D11_USAGE_DYNAMIC;
		CbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		CbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		CbDesc.ByteWidth = sizeof(CB_PER_OBJECT);
		V_RETURN(pD3DDeive->CreateBuffer(&CbDesc, nullptr, &m_pCbPerObject));
		m_pCbPerObject->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("CB_PER_OBJECT"), "CB_PER_OBJECT");

		CbDesc.ByteWidth = sizeof(CB_PER_FRAME);
		V_RETURN(pD3DDeive->CreateBuffer(&CbDesc, nullptr, &m_pCbPerFrame));
		m_pCbPerFrame->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("CB_PER_FRAME"), "CB_PER_FRAME");

		//Create state objects
		D3D11_SAMPLER_DESC SamplerDesc;
		ZeroMemory(&SamplerDesc, sizeof(SamplerDesc));
		SamplerDesc.Filter = D3D11_FILTER_ANISOTROPIC;
		SamplerDesc.AddressU = SamplerDesc.AddressV = SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		SamplerDesc.MaxAnisotropy = 16;
		SamplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
		SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
		V_RETURN(pD3DDeive->CreateSamplerState(&SamplerDesc, &m_pSamAnisotropic));
		m_pSamAnisotropic->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("Anisotropic"), "Anisotropic");


		//Create Depth states
		D3D11_DEPTH_STENCIL_DESC DepthStencilDesc;
		DepthStencilDesc.DepthEnable = TRUE;
		DepthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		DepthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;
		DepthStencilDesc.StencilEnable = TRUE;
		DepthStencilDesc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
		DepthStencilDesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
		DepthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
		DepthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		DepthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		DepthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_INCR_SAT;
		DepthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_NEVER;
		DepthStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		DepthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		DepthStencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		V_RETURN(pD3DDeive->CreateDepthStencilState(&DepthStencilDesc, &m_pDepthLess));


		DepthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		DepthStencilDesc.DepthFunc = D3D11_COMPARISON_EQUAL;
		DepthStencilDesc.StencilEnable = FALSE;
		V_RETURN(pD3DDeive->CreateDepthStencilState(&DepthStencilDesc, &m_pDepthEqualAndDisableDepthWrite));


		// Create Rasterize states
		D3D11_RASTERIZER_DESC RasterizerDesc;
		RasterizerDesc.FillMode = D3D11_FILL_SOLID;
		RasterizerDesc.CullMode = D3D11_CULL_NONE; // Disable Culling
		RasterizerDesc.FrontCounterClockwise = FALSE;
		RasterizerDesc.DepthBias = 0;
		RasterizerDesc.DepthBiasClamp = 0.0f;
		RasterizerDesc.SlopeScaledDepthBias = 0.0f;
		RasterizerDesc.DepthClipEnable = TRUE;
		RasterizerDesc.ScissorEnable = FALSE;
		RasterizerDesc.MultisampleEnable = FALSE;
		RasterizerDesc.AntialiasedLineEnable = FALSE;
		V_RETURN(pD3DDeive->CreateRasterizerState(&RasterizerDesc, &m_pDisableCullingRS));


		// Create blend states
		D3D11_BLEND_DESC BlendStateDesc;
		BlendStateDesc.AlphaToCoverageEnable = FALSE;
		BlendStateDesc.IndependentBlendEnable = FALSE;
		BlendStateDesc.RenderTarget[0].BlendEnable = FALSE;
		BlendStateDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		BlendStateDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		BlendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
		BlendStateDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		BlendStateDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		BlendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
		BlendStateDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		V_RETURN(pD3DDeive->CreateBlendState(&BlendStateDesc, &m_pOpaqueBS));

		BlendStateDesc.RenderTarget[0].RenderTargetWriteMask = 0;
		V_RETURN(pD3DDeive->CreateBlendState(&BlendStateDesc, &m_pDepthOnlyBS));

		BlendStateDesc.RenderTarget[0].RenderTargetWriteMask = 0;
		BlendStateDesc.AlphaToCoverageEnable = TRUE;
		V_RETURN(pD3DDeive->CreateBlendState(&BlendStateDesc, &m_pDepthOnlyAlphaToCoverageBS));


		if (!bShaderInited)
		{
			AddShaderToCache(pShaderCache);
			bShaderInited = true;
		}


		return hr;
	}

	void ForwardPlusRender::OnDestroyDevice(void * pUserContext)
	{
		ReleaseAllD3D11COM();

	}

	HRESULT ForwardPlusRender::OnResizedSwapChain(ID3D11Device * pD3DDevie, const DXGI_SURFACE_DESC * pBackBufferSurfaceDesc)
	{
		HRESULT hr;

		ResizeSolution(pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height);

		//Depends on m_uWidht and m_uHeight,so don't do this
		// until we have updated them (see above)
		unsigned uNumTile = GetNumTilesX()*GetNumTilesY();
		unsigned uMaxNumLightsPerTile = GetMaxNumLightsPerTile();


		D3D11_BUFFER_DESC BufferDesc;
		ZeroMemory(&BufferDesc, sizeof(BufferDesc));
		BufferDesc.Usage = D3D11_USAGE_DEFAULT;
		BufferDesc.ByteWidth = sizeof(unsigned int) * uMaxNumLightsPerTile* uNumTile;
		BufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		V_RETURN(pD3DDevie->CreateBuffer(&BufferDesc, nullptr, &m_pLightIndexBuffer));
		m_pLightIndexBuffer->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("LightIndexBuffer"), "LightIndexBuffer");


		// Light index SRV
		D3D11_SHADER_RESOURCE_VIEW_DESC ShaderResourceViewDesc;
		ZeroMemory(&ShaderResourceViewDesc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
		ShaderResourceViewDesc.Format = DXGI_FORMAT_R32_UINT;
		ShaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		ShaderResourceViewDesc.Buffer.ElementOffset = 0;
		ShaderResourceViewDesc.Buffer.ElementWidth = uMaxNumLightsPerTile* uNumTile;
		V_RETURN(pD3DDevie->CreateShaderResourceView(m_pLightIndexBuffer, &ShaderResourceViewDesc, &m_pLightIndexBufferSRV));
		m_pLightIndexBufferSRV->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("LightIndexSRV"), "LightIndexSRV");

		//Light Index UAV
		D3D11_UNORDERED_ACCESS_VIEW_DESC UnoderedAccessViewDesc;
		ZeroMemory(&UnoderedAccessViewDesc, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));
		UnoderedAccessViewDesc.Format = DXGI_FORMAT_R32_UINT;
		UnoderedAccessViewDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		UnoderedAccessViewDesc.Buffer.FirstElement = 0;
		UnoderedAccessViewDesc.Buffer.NumElements = uMaxNumLightsPerTile*uNumTile;
		V_RETURN(pD3DDevie->CreateUnorderedAccessView(m_pLightIndexBuffer, &UnoderedAccessViewDesc, &m_pLightIndexBufferUAV));
		m_pLightIndexBufferUAV->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("LightIndexUAV"), "LightIndexUAV");

		return hr;
	}

	void ForwardPlusRender::OnReleasingSwapChain()
	{
		ReleaseSwapChainAssociatedCOM();
	}

	void ForwardPlusRender::OnRender(ID3D11Device* pD3dDevice, ID3D11DeviceContext* pD3dImmediateContext, const CBaseCamera* pCBaseCamera, ID3D11RenderTargetView* pRenderTargetView, const DXGI_SURFACE_DESC* pBackBufferDesc,
		ID3D11Texture2D* pDepthStencilTexture,
		ID3D11DepthStencilView* pDepthStencilView,
		ID3D11ShaderResourceView* pDepthStencilSRV, float fElaspedTime, CDXUTSDKMesh** pSceneMeshes, int iCountMesh, CDXUTSDKMesh** pAlphaSceneMeshes, int iCountAlphaMesh)
	{
		if (!bShaderInited)
		{
			return;
		}

		// Default pixel shader
		ID3D11PixelShader* pScenePS = m_pSceneNoAlphaTestAndLightCullPS;
		ID3D11PixelShader* pAlphaScenePS = m_pSceneAlphaTestAndLightCullPS;

		// Default compute shader
		bool bMsaaEnabled = (pBackBufferDesc->SampleDesc.Count > 1);
		ID3D11ComputeShader* pLightCullCS = bMsaaEnabled ? m_pLightCullMSAA_CS : m_pLightCullCS;


		// Clear the backBuffer and depth stencil
		float ClearColor[4] = { 0.0013f,0.0015f,0.0050f,0.0f };
		ID3D11RenderTargetView* pRTV = pRenderTargetView;
		pD3dImmediateContext->ClearRenderTargetView(pRTV, ClearColor);
		pD3dImmediateContext->ClearDepthStencilView(pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0); // we are using inverted depth,so cleat to zero

		XMMATRIX mWorld = XMMatrixIdentity();

		// Get the projection & view matrix from the camera class
		XMMATRIX mView = pCBaseCamera->GetViewMatrix();
		XMMATRIX mProj = pCBaseCamera->GetProjMatrix();
		XMMATRIX mWorldView = mWorld*mView;
		XMMATRIX mWorldViewProjection = mWorld*mView*mProj;

		// we need the inverse proj matrix in the per-tile light culling compute shader
		XMVECTOR det;
		XMMATRIX mInvProj = XMMatrixInverse(&det, mProj);

		XMFLOAT4 CameraPosAndAlphaTest;
		XMStoreFloat4(&CameraPosAndAlphaTest, pCBaseCamera->GetEyePt());
		// different alpha test for MSAA enabled vs disabled
		CameraPosAndAlphaTest.w = bMsaaEnabled ? 0.003f : 0.5f;


		// Set the constant buffers
		HRESULT hr;
		D3D11_MAPPED_SUBRESOURCE MappedResource;

		V(pD3dImmediateContext->Map(m_pCbPerFrame, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
		CB_PER_FRAME* pPerFrame = (CB_PER_FRAME*)MappedResource.pData;
		pPerFrame->m_mProjection = XMMatrixTranspose(mProj);
		pPerFrame->m_mProjectionInv = XMMatrixTranspose(mInvProj);
		pPerFrame->m_vCameraPosAndAlphaTest = XMLoadFloat4(&CameraPosAndAlphaTest);
		pPerFrame->m_uNumLights = (((unsigned)g_iNumActiveSpotLights & 0xFFFFu) << 16) | ((unsigned)g_iNumActivePointLights & 0xFFFFu);
		pPerFrame->m_uWindowWidth = pBackBufferDesc->Width;
		pPerFrame->m_uWindowHeight = pBackBufferDesc->Height;
		pPerFrame->m_uMaxNumLightsPerTile = GetMaxNumLightsPerTile();
		pD3dImmediateContext->Unmap(m_pCbPerFrame, 0);
		pD3dImmediateContext->VSSetConstantBuffers(1, 1, &m_pCbPerFrame);
		pD3dImmediateContext->PSSetConstantBuffers(1, 1, &m_pCbPerFrame);
		pD3dImmediateContext->CSSetConstantBuffers(1, 1, &m_pCbPerFrame);


		V(pD3dImmediateContext->Map(m_pCbPerObject, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
		CB_PER_OBJECT* pPerObject = (CB_PER_OBJECT*)MappedResource.pData;
		pPerObject->m_mWorldViewProjection = XMMatrixTranspose(mWorldViewProjection);
		pPerObject->m_mWorldView = XMMatrixTranspose(mWorldView);
		pPerObject->m_mWorld = XMMatrixTranspose(mWorld);
		pPerObject->m_MaterialAmbientColorUp = XMVectorSet(0.013f, 0.015f, 0.050f, 1.0f);
		pPerObject->m_MaterialAmbientColorDown = XMVectorSet(0.0013f, 0.0015f, 0.0050f, 1.0f);
		pD3dImmediateContext->Unmap(m_pCbPerObject, 0);
		pD3dImmediateContext->VSSetConstantBuffers(0, 1, &m_pCbPerObject);
		pD3dImmediateContext->PSSetConstantBuffers(0, 1, &m_pCbPerObject);
		pD3dImmediateContext->CSSetConstantBuffers(0, 1, &m_pCbPerObject);



		// Switch off alpha blending
		float BlendFactor[1] = { 0.0f };
		pD3dImmediateContext->OMSetBlendState(m_pOpaqueBS, BlendFactor, 0xFFFFFFFF);

		{
			ID3D11RenderTargetView* pNullRTV = nullptr;
			ID3D11DepthStencilView* pNullDSV = nullptr;
			ID3D11ShaderResourceView* pNullSRV = nullptr;
			ID3D11UnorderedAccessView* pNullUAV = nullptr;
			ID3D11SamplerState* pNullSampler = nullptr;

			TIMER_Begin(0, L"ForwardPlus");
			{
				TIMER_Begin(0, L"Depth pre-pass");
				{
					// Depth pre-pass (to eliminate pixel overdraw during forward rendering)
					pD3dImmediateContext->OMSetRenderTargets(1, &pNullRTV, pDepthStencilView); //null color buffer
					pD3dImmediateContext->OMSetDepthStencilState(m_pDepthLess, 0x00);
					pD3dImmediateContext->IASetInputLayout(m_pPositionOnlyInputLayout);
					pD3dImmediateContext->VSSetShader(m_pScenePositionOnlyVS, nullptr, 0);
					pD3dImmediateContext->PSSetShader(nullptr, nullptr, 0); // null pixel shader
					pD3dImmediateContext->PSSetShaderResources(0, 1, &pNullSRV);
					pD3dImmediateContext->PSSetShaderResources(1, 1, &pNullSRV);
					pD3dImmediateContext->PSSetSamplers(0, 1, &pNullSampler);

					for (int i = 0; i < iCountMesh; ++i)
					{
						pSceneMeshes[i]->Render(pD3dImmediateContext);
					}



					// More depth pre-pass,for test geometry
					pD3dImmediateContext->OMSetRenderTargets(1, (ID3D11RenderTargetView*const*)&pRTV, pDepthStencilView);
					if (bMsaaEnabled)
					{
						pD3dImmediateContext->OMSetBlendState(m_pDepthOnlyAlphaToCoverageBS, BlendFactor, 0xFFFFFFFF);
					}
					else
					{
						pD3dImmediateContext->OMSetBlendState(m_pDepthOnlyBS, BlendFactor, 0xFFFFFFFF);
					}

					pD3dImmediateContext->RSSetState(m_pDisableCullingRS);
					pD3dImmediateContext->IASetInputLayout(m_pPositionOnlyInputLayout);
					pD3dImmediateContext->VSSetShader(m_pScenePositionAndTextureVS, nullptr, 0);
					pD3dImmediateContext->PSSetShader(m_pSceneAlphaTestOnlyPS, nullptr, 0);
					pD3dImmediateContext->PSSetSamplers(0, 1, &m_pSamAnisotropic);

					for (int i = 0; i < iCountAlphaMesh; ++i)
					{
						pAlphaSceneMeshes[i]->Render(pD3dImmediateContext, 0);
					}

					//还原
					pD3dImmediateContext->RSSetState(nullptr);
					pD3dImmediateContext->OMSetBlendState(m_pOpaqueBS, BlendFactor, 0xFFFFFFFF);
				}
				TIMER_End(); //Depth pre-pass



				TIMER_Begin(0, L"Light culling");
				{
					// Cull lights on the GPU,using a Compute Shader
					pD3dImmediateContext->OMSetRenderTargets(1, &pNullRTV, pNullDSV); // null color buffer and depth-stencil
					pD3dImmediateContext->VSSetShader(nullptr, nullptr, 0);// null vertex shader
					pD3dImmediateContext->PSSetShader(nullptr, nullptr, 0); // null pixel shader
					pD3dImmediateContext->PSSetShaderResources(0, 1, &pNullSRV);
					pD3dImmediateContext->PSSetShaderResources(1, 1, &pNullSRV);
					pD3dImmediateContext->PSSetSamplers(0, 1, &pNullSampler);
					pD3dImmediateContext->CSSetShader(pLightCullCS, nullptr, 0);
					pD3dImmediateContext->CSSetShaderResources(0, 1, GetPointLightBufferCenterAndRadiusSRV());
					pD3dImmediateContext->CSSetShaderResources(1, 1, GetSpotLightBufferCenterAndRadiusSRV());
					pD3dImmediateContext->CSSetShaderResources(2, 1, &pDepthStencilSRV);
					pD3dImmediateContext->CSSetUnorderedAccessViews(0, 1, GetLightIndexBufferUAV(), nullptr); // target UAV;
					pD3dImmediateContext->Dispatch(GetNumTilesX(), GetNumTilesY(), 1);

					//clear state
					pD3dImmediateContext->CSSetShader(nullptr, nullptr, 0);
					pD3dImmediateContext->CSSetShaderResources(0, 1, &pNullSRV);
					pD3dImmediateContext->CSSetShaderResources(1, 1, &pNullSRV);
					pD3dImmediateContext->CSSetShaderResources(2, 1, &pNullSRV);
					pD3dImmediateContext->CSSetUnorderedAccessViews(0, 1, &pNullUAV, nullptr);
				}
				TIMER_End(); //Light culling


				TIMER_Begin(0, L"Forward rendering");
				{
					// forward rendering
					pD3dImmediateContext->OMSetRenderTargets(1, (ID3D11RenderTargetView*const*)&pRTV, pDepthStencilView);
					pD3dImmediateContext->OMSetDepthStencilState(m_pDepthEqualAndDisableDepthWrite, 0x00);
					pD3dImmediateContext->IASetInputLayout(m_pMeshInputLayout);
					pD3dImmediateContext->VSSetShader(m_pSceneMeshVS, nullptr, 0);
					pD3dImmediateContext->PSSetShader(pScenePS, nullptr, 0);
					pD3dImmediateContext->PSSetSamplers(0, 1, &m_pSamAnisotropic);
					pD3dImmediateContext->PSSetShaderResources(2, 1, GetPointLightBufferCenterAndRadiusSRV());
					pD3dImmediateContext->PSSetShaderResources(3, 1, GetPointLightBufferColorSRV());
					pD3dImmediateContext->PSSetShaderResources(4, 1, GetSpotLightBufferCenterAndRadiusSRV());
					pD3dImmediateContext->PSSetShaderResources(5, 1, GetSpotLightBufferColorSRV());
					pD3dImmediateContext->PSSetShaderResources(6, 1, GetSpotLightBufferParamsSRV());
					pD3dImmediateContext->PSSetShaderResources(7, 1, GetLightIndexBufferSRV());


					for (int i = 0; i < iCountMesh; ++i)
					{
						pSceneMeshes[i]->Render(pD3dImmediateContext, 0, 1);
					}

					// More forward rendering,for alpha test geometry
					pD3dImmediateContext->RSSetState(m_pDisableCullingRS);
					pD3dImmediateContext->PSSetShader(pAlphaScenePS, nullptr, 0);
					for (int i = 0; i < iCountAlphaMesh; ++i)
					{
						pAlphaSceneMeshes[i]->Render(pD3dImmediateContext, 0, 1);
					}
					pD3dImmediateContext->RSSetState(nullptr);


					// restore to default
					pD3dImmediateContext->PSSetShaderResources(2, 1, &pNullSRV);
					pD3dImmediateContext->PSSetShaderResources(3, 1, &pNullSRV);
					pD3dImmediateContext->PSSetShaderResources(4, 1, &pNullSRV);
					pD3dImmediateContext->PSSetShaderResources(5, 1, &pNullSRV);
					pD3dImmediateContext->PSSetShaderResources(6, 1, &pNullSRV);
					pD3dImmediateContext->PSSetShaderResources(7, 1, &pNullSRV);
					pD3dImmediateContext->OMSetDepthStencilState(nullptr, 0x00);
				}
				TIMER_End();// Forward rendering

			}
			TIMER_End();//ForwardPlus

		}


	}

	void ForwardPlusRender::ResizeSolution(UINT width, UINT height)
	{
		m_uWidth = width;
		m_uHeight = height;
		bSolutionSized = true;
	}

	unsigned ForwardPlusRender::GetNumTilesX()
	{
		assert(bSolutionSized);
		return (unsigned)((m_uWidth + TILE_RES - 1) / (float)TILE_RES);
	}

	unsigned ForwardPlusRender::GetNumTilesY()
	{
		assert(bSolutionSized);
		return (unsigned)((m_uHeight + TILE_RES - 1) / (float)TILE_RES);
	}

	unsigned ForwardPlusRender::GetMaxNumLightsPerTile()
	{
		const unsigned kAdjustmentMultipier = 32;

		// I haven't tested at greater than 1080p,so cap it
		unsigned uHeight = (m_uHeight > 1080) ? 1080 : m_uHeight;

		// adjust max lights per tile down as height increases
		return (MAX_NUM_LIGHTS_PER_TILE - (kAdjustmentMultipier * (uHeight / 120)));
	}

	void ForwardPlusRender::ReleaseAllD3D11COM(void)
	{
		ReleaseSwapChainAssociatedCOM();
		ReleaseOneTimeInitedCOM();
	}

	void ForwardPlusRender::ReleaseSwapChainAssociatedCOM(void)
	{
		//Light Index
		SAFE_RELEASE(m_pLightIndexBuffer);
		SAFE_RELEASE(m_pLightIndexBufferSRV);
		SAFE_RELEASE(m_pLightIndexBufferUAV);
	}
	void ForwardPlusRender::ReleaseOneTimeInitedCOM(void)
	{
		SAFE_RELEASE(m_pCbPerObject);
		SAFE_RELEASE(m_pCbPerFrame);

		//Point Light
		SAFE_RELEASE(m_pPointLightBufferCenterAndRadius);
		SAFE_RELEASE(m_pPointLightBufferCenterAndRadiusSRV);
		SAFE_RELEASE(m_pPointLightBufferColor);
		SAFE_RELEASE(m_pPointLightBufferColorSRV);

		//Spot Light
		SAFE_RELEASE(m_pSpotLightBufferCenterAndRadius);
		SAFE_RELEASE(m_pSpotLightBufferCenterAndRadiusSRV);
		SAFE_RELEASE(m_pSpotLightBufferColor);
		SAFE_RELEASE(m_pSpotLightBufferColorSRV);
		SAFE_RELEASE(m_pSpotLightBufferParams);
		SAFE_RELEASE(m_pSpotLightBufferParamsSRV);



		//SamplerState
		SAFE_RELEASE(m_pSamAnisotropic);

		//DepthStencilState
		SAFE_RELEASE(m_pDepthLess);
		SAFE_RELEASE(m_pDepthEqualAndDisableDepthWrite);
		SAFE_RELEASE(m_pDisableDepthWrite);
		SAFE_RELEASE(m_pDisableDepthTest);

		// Blend States
		SAFE_RELEASE(m_pAdditiveBS);
		SAFE_RELEASE(m_pOpaqueBS);
		SAFE_RELEASE(m_pDepthOnlyBS);
		SAFE_RELEASE(m_pDepthOnlyAlphaToCoverageBS);

		//Rasterize states
		SAFE_RELEASE(m_pDisableCullingRS);



		ReleaseShaderCachedCOM();
	}
	void ForwardPlusRender::ReleaseShaderCachedCOM(void)
	{
		//VS
		SAFE_RELEASE(m_pScenePositionOnlyVS);
		SAFE_RELEASE(m_pScenePositionAndTextureVS);
		SAFE_RELEASE(m_pSceneMeshVS);

		//PS
		SAFE_RELEASE(m_pSceneNoAlphaTestAndLightCullPS);
		SAFE_RELEASE(m_pSceneAlphaTestAndLightCullPS);
		SAFE_RELEASE(m_pSceneNoAlphaTestAndNoLightCullPS);
		SAFE_RELEASE(m_pSceneAlphaTestAndNoLightCullPS);
		SAFE_RELEASE(m_pSceneAlphaTestOnlyPS);

		// Compute Shader
		SAFE_RELEASE(m_pLightCullCS);
		SAFE_RELEASE(m_pLightCullMSAA_CS);
		SAFE_RELEASE(m_pLightCullNoDepthCS);

		//InputLayout
		SAFE_RELEASE(m_pPositionOnlyInputLayout);
		SAFE_RELEASE(m_pPositionAndTexInputLayout);
		SAFE_RELEASE(m_pMeshInputLayout);
	}

}
