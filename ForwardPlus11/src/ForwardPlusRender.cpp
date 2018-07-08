#include "stdafx.h"
#include "ForwardPlusRender.h"
#include "../../amd_sdk/inc/AMD_SDK.h"
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
	return (float)rand() / (RAND_MAX)*(fRangeMax - fRangeMin) + fRangeMin;
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



ForwardPlus11::ForwardPlusRender::ForwardPlusRender()
	:m_uWidth(0),
	m_uHeight(0),
	bSolutionSized(false),
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
	//Buffers for light culling
	m_pLightIndexBuffer(nullptr),
	m_pLightIndexBufferSRV(nullptr),
	m_pLightIndexBufferUAV(nullptr),
	//Vertex Shader
	m_pScenePositionOnlyVS(nullptr),
	m_pScenePositionAndTextureVS(nullptr),
	m_pSceneMeshVS(nullptr),
	//Pixel Shader
	m_pSceneMeshPS(nullptr),
	m_pSceneAlphaTestPS(nullptr),
	m_pSceneNoCullPS(nullptr),
	m_pSceneNoCullAlphaTestPS(nullptr),
	m_pSceneAlphaTestOnlyPS(nullptr),
	// Compute Shader
	m_pLightCullCS(nullptr),
	m_pLightCullMSAA_CS(nullptr),
	m_pLightCullNoDepthCS(nullptr),
	//InputLayout
	m_pPositionOnlyInputLayout(nullptr),
	m_pPositionAndTexInputLayout(nullptr),
	m_pMeshInputLayout(nullptr),
	//SamplerState
	m_pSamAnisotropic(nullptr),
	// Depth buffer data
	m_pDepthStencilTexture(nullptr),
	m_pDepthStencilView(nullptr),
	m_pDepthStencilSRV(nullptr),
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
	m_pCbPerObject(nullptr),
	m_pCbPerFrame(nullptr),

	bFirstPass(true)
{

}

ForwardPlus11::ForwardPlusRender::~ForwardPlusRender()
{
	ReleaseAllD3D11COM();
}



void ForwardPlus11::ForwardPlusRender::CalculateSceneMinMax(CDXUTSDKMesh& Mesh, DirectX::XMVECTOR * pBBoxMinOut, DirectX::XMVECTOR * pBBoxMaxOut)
{
	*pBBoxMinOut = Mesh.GetMeshBBoxCenter(0) - Mesh.GetMeshBBoxExtents(0);
	*pBBoxMaxOut = Mesh.GetMeshBBoxCenter(0) + Mesh.GetMeshBBoxExtents(0);

	for (unsigned int i = 0;i<Mesh.GetNumMeshes();++i)
	{
		XMVECTOR vNewMin = Mesh.GetMeshBBoxCenter(i) - Mesh.GetMeshBBoxExtents(i);
		XMVECTOR vNewMax = Mesh.GetMeshBBoxCenter(i) + Mesh.GetMeshBBoxExtents(i);

		*pBBoxMinOut = XMVectorMin(vNewMin, *pBBoxMinOut);
		*pBBoxMaxOut = XMVectorMax(vNewMax, *pBBoxMaxOut);

	}
}

//jingz 聚光灯相关就是没看懂
void ForwardPlus11::ForwardPlusRender::InitRandomLights(const DirectX::XMVECTOR & BBoxMin, const DirectX::XMVECTOR & BBoxMax)
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
	for (int i = 0;i<MAX_NUM_LIGHTS;++i)
	{
		g_PointLightCenterAndRadius[i] = XMFLOAT4(GetRandFloat(vBBoxMin.x, vBBoxMax.x), GetRandFloat(vBBoxMin.y, vBBoxMax.y), GetRandFloat(vBBoxMin.z, vBBoxMax.z), fRadius);
		g_PointLightColors[i] = GetRandColor();

		XMFLOAT3 vLightDir = GetRandLightDirection();

		// random direction,cosine of cone angle,falloff radius calculated above
		g_SpotLightSpotParams[i] = PackSpotParams(vLightDir, 0.816496580927726f, fSpotLightFalloffRadius);

	}

}

void ForwardPlus11::ForwardPlusRender::AddShaderToCache(AMD::ShaderCache * pShaderCache)
{
	//Ensure all shaders(and input layouts) are released
	ReleaseAllD3D11COM();

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
	pShaderCache->AddShader((ID3D11DeviceChild**)&m_pSceneMeshPS, AMD::ShaderCache::SHADER_TYPE_PIXEL, L"ps_5_0", L"RenderScenePS", L"ForwardPlus11.hlsl", 2, ShaderMacros, nullptr, nullptr, 0);

	ShaderMacros[0].m_iValue = 1;
	ShaderMacros[1].m_iValue = 1;
	pShaderCache->AddShader((ID3D11DeviceChild**)&m_pSceneAlphaTestPS, AMD::ShaderCache::SHADER_TYPE_PIXEL, L"ps_5_0", L"RenderScenePS", L"ForwardPlus11.hlsl", 2, ShaderMacros, nullptr, nullptr, 0);

	ShaderMacros[0].m_iValue = 0;
	ShaderMacros[1].m_iValue = 0;
	pShaderCache->AddShader((ID3D11DeviceChild**)&m_pSceneNoCullPS, AMD::ShaderCache::SHADER_TYPE_PIXEL, L"ps_5_0", L"RenderScenePS", L"ForwardPlus11.hlsl", 2, ShaderMacros, nullptr, nullptr, 0);

	ShaderMacros[0].m_iValue = 1;
	ShaderMacros[1].m_iValue = 0;
	pShaderCache->AddShader((ID3D11DeviceChild**)&m_pSceneNoCullPS, AMD::ShaderCache::SHADER_TYPE_PIXEL, L"ps_5_0", L"RenderScenePS", L"ForwardPlus11.hlsl", 2, ShaderMacros, nullptr, nullptr, 0);


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




HRESULT ForwardPlus11::ForwardPlusRender::OnCreateDevice(ID3D11Device * pD3DDeive,AMD::ShaderCache* pShaderCache, XMVECTOR SceneMin, XMVECTOR SceneMax)
{
	HRESULT hr;

	//Create state objects
	D3D11_SAMPLER_DESC SamplerDesc;
	ZeroMemory(&SamplerDesc,sizeof(SamplerDesc));
	SamplerDesc.Filter = D3D11_FILTER_ANISOTROPIC;
	SamplerDesc.AddressU = SamplerDesc.AddressV = SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	SamplerDesc.MaxAnisotropy = 16;
	SamplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
	V_RETURN(pD3DDeive->CreateSamplerState(&SamplerDesc, &m_pSamAnisotropic));
	DXUT_SetDebugName(m_pSamAnisotropic, "Anisotropic");

	//Create constant buffers
	D3D11_BUFFER_DESC CbDesc;
	ZeroMemory(&CbDesc, sizeof(CbDesc));
	CbDesc.Usage = D3D11_USAGE_DYNAMIC;
	CbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	CbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	CbDesc.ByteWidth = sizeof(CB_PER_OBJECT);
	V_RETURN(pD3DDeive->CreateBuffer(&CbDesc, nullptr, &m_pCbPerObject));
	DXUT_SetDebugName(m_pCbPerObject, "CB_PER_OBJECT");

	CbDesc.ByteWidth = sizeof(CB_PER_FRAME);
	V_RETURN(pD3DDeive->CreateBuffer(&CbDesc, nullptr, &m_pCbPerFrame));
	DXUT_SetDebugName(m_pCbPerFrame, "CB_PER_FRAME");

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
	V_RETURN(pD3DDeive->CreateRasterizerState(&RasterizerDesc,& m_pDisableCullingRS));


	//Create Depth states
	D3D11_DEPTH_STENCIL_DESC DepthStencilDesc;
	DepthStencilDesc.DepthEnable = TRUE;
	DepthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	DepthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;
	DepthStencilDesc.StencilEnable = TRUE;
	DepthStencilDesc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
	DepthStencilDesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
	V_RETURN(pD3DDeive->CreateDepthStencilState(&DepthStencilDesc, &m_pDepthLess));


	DepthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	DepthStencilDesc.DepthFunc = D3D11_COMPARISON_EQUAL;
	V_RETURN(pD3DDeive->CreateDepthStencilState(&DepthStencilDesc,&m_pDepthEqualAndDisableDepthWrite));

	if (bFirstPass)
	{
		AddShaderToCache(pShaderCache);
		pShaderCache->GenerateShaders(AMD::ShaderCache::CREATE_TYPE_COMPILE_CHANGES);
	}
	
	this->InitRandomLights(SceneMin, SceneMax);

	return S_OK;
}

void ForwardPlus11::ForwardPlusRender::OnDestroyDevice()
{
	ReleaseAllD3D11COM();
	
}

HRESULT ForwardPlus11::ForwardPlusRender::OnResizedSwapChain(ID3D11Device * pD3DDevie, const DXGI_SURFACE_DESC * pBackBufferSurfaceDesc, int nLineHeight)
{
	HRESULT hr;

	ResizeSolution(pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height);

	//Depends on m_uWidht and m_uHeight,so don't do this
	// until we have updated them (see above)
	unsigned uNumTile = GetNumTileX()*GetNumTileY();
	unsigned uMaxNumLightsPerTile = GetMaxNumLightsPerTile();


	D3D11_BUFFER_DESC BufferDesc;
	ZeroMemory(&BufferDesc, sizeof(BufferDesc));
	BufferDesc.Usage = D3D11_USAGE_DEFAULT;
	BufferDesc.ByteWidth = sizeof(unsigned int) * uMaxNumLightsPerTile* uNumTile;
	BufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	V_RETURN(pD3DDevie->CreateBuffer(&BufferDesc, nullptr, &m_pLightIndexBuffer));
	DXUT_SetDebugName(m_pLightIndexBuffer, "LightIndexBuffer");


	// Light index SRV
	D3D11_SHADER_RESOURCE_VIEW_DESC ShaderResourceViewDesc;
	ZeroMemory(&ShaderResourceViewDesc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
	ShaderResourceViewDesc.Format = DXGI_FORMAT_R32_UINT;
	ShaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	ShaderResourceViewDesc.Buffer.ElementOffset = 0;
	ShaderResourceViewDesc.Buffer.ElementWidth = uMaxNumLightsPerTile* uNumTile;
	V_RETURN(pD3DDevie->CreateShaderResourceView(m_pLightIndexBuffer, &ShaderResourceViewDesc, &m_pLightIndexBufferSRV));
	DXUT_SetDebugName(m_pLightIndexBufferSRV, "LightIndexSRV");

	//Light Index UAV
	D3D11_UNORDERED_ACCESS_VIEW_DESC UnoderedAccessViewDesc;
	ZeroMemory(&UnoderedAccessViewDesc, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));
	UnoderedAccessViewDesc.Format = DXGI_FORMAT_R32_UINT;
	UnoderedAccessViewDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	UnoderedAccessViewDesc.Buffer.FirstElement = 0;
	UnoderedAccessViewDesc.Buffer.NumElements = uMaxNumLightsPerTile*uNumTile;
	V_RETURN(pD3DDevie->CreateUnorderedAccessView(m_pLightIndexBuffer, &UnoderedAccessViewDesc, &m_pLightIndexBufferUAV));

	return S_OK;
}

void ForwardPlus11::ForwardPlusRender::OnReleasingSwapChain()
{
	SAFE_RELEASE(m_pLightIndexBuffer);
	SAFE_RELEASE(m_pLightIndexBufferSRV);
	SAFE_RELEASE(m_pLightIndexBufferUAV);


}

void ForwardPlus11::ForwardPlusRender::OnRender(float fElaspedTime, unsigned uNumPointLights, unsigned uNumSporLights)
{
}

void ForwardPlus11::ForwardPlusRender::ResizeSolution(UINT width, UINT height)
{
	m_uWidth = width;
	m_uHeight = height;
	bSolutionSized = true;
}

unsigned ForwardPlus11::ForwardPlusRender::GetNumTileX()
{
	assert(bSolutionSized);
	return (unsigned)((m_uWidth + TILE_RES - 1) / (float)TILE_RES);
}

unsigned ForwardPlus11::ForwardPlusRender::GetNumTileY()
{
	assert(bSolutionSized);
	return (unsigned)((m_uHeight + TILE_RES - 1) / (float)TILE_RES);
}

unsigned ForwardPlus11::ForwardPlusRender::GetMaxNumLightsPerTile()
{
	const unsigned kAdjustmentMultipier = 32;

	// I haven't tested at greater than 1080p,so cap it
	unsigned uHeight = (m_uHeight > 1080) ? 1080 : m_uHeight;

	// adjust max lights per tile down as height increases
	return (MAX_NUM_LIGHTS_PER_TILE - (kAdjustmentMultipier * (uHeight / 120)));
}

void ForwardPlus11::ForwardPlusRender::ReleaseAllD3D11COM(void)
{
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

	//Light Index
	SAFE_RELEASE(m_pLightIndexBuffer);
	SAFE_RELEASE(m_pLightIndexBufferSRV);
	SAFE_RELEASE(m_pLightIndexBufferUAV);

	//VS
	SAFE_RELEASE(m_pScenePositionOnlyVS);
	SAFE_RELEASE(m_pScenePositionAndTextureVS);
	SAFE_RELEASE(m_pSceneMeshVS);

	//PS
	SAFE_RELEASE(m_pSceneMeshPS);
	SAFE_RELEASE(m_pSceneAlphaTestPS);
	SAFE_RELEASE(m_pSceneNoCullPS);
	SAFE_RELEASE(m_pSceneNoCullAlphaTestPS);
	SAFE_RELEASE(m_pSceneAlphaTestOnlyPS);

	// Compute Shader
	SAFE_RELEASE(m_pLightCullCS);
	SAFE_RELEASE(m_pLightCullMSAA_CS);
	SAFE_RELEASE(m_pLightCullNoDepthCS);

	//InputLayout
	SAFE_RELEASE(m_pPositionOnlyInputLayout);
	SAFE_RELEASE(m_pPositionAndTexInputLayout);
	SAFE_RELEASE(m_pMeshInputLayout);

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

	SAFE_RELEASE(m_pCbPerObject);
	SAFE_RELEASE(m_pCbPerFrame);
}
