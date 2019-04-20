#include "BasePostProcessRender.h"

PostProcess::BasePostProcessRender::BasePostProcessRender()
	:m_Position4(0.0f, 0.0f, 0.0f, 0.0f),m_pMeshIB(nullptr),m_pMeshVB(nullptr),
	m_pShaderInputLayout(nullptr),m_pShaderVS(nullptr),m_pShaderPS(nullptr),
	m_pDepthStencilState(nullptr), m_pRasterizerState(nullptr), m_pBlendState(nullptr), m_pSamplerState(nullptr),
	m_pConstantBufferPerObject(nullptr),m_pConstantBufferPerFrame(nullptr),
	m_uSizeConstantBufferPerObject(sizeof(CB_PER_OBJECT)),
	m_uSizeConstantBufferPerFrame(sizeof(CB_PER_FRAME)),
	m_pSrcTextureSRV(nullptr),
	m_bShaderInited(false)
{
}

PostProcess::BasePostProcessRender::~BasePostProcessRender()
{
	ReleaseAllD3D11COM();
}

void PostProcess::BasePostProcessRender::CalculateSceneMinMax(GeometryHelper::MeshData & meshData, DirectX::XMVECTOR * pBBoxMinOut, DirectX::XMVECTOR * pBBoxMaxOut)
{
	auto temp = meshData.Vertices[0].Position;
	*pBBoxMinOut = DirectX::XMVectorSet(temp.x, temp.y, temp.z, 1.0f);
	*pBBoxMaxOut = DirectX::XMVectorSet(temp.x, temp.y, temp.z, 1.0f);

	for (int i = 1; i < meshData.Vertices.size(); ++i)
	{
		auto temp = meshData.Vertices[i].Position;
		DirectX::XMVECTOR vNew = DirectX::XMVectorSet(temp.x, temp.y, temp.z, 1.0f);
		*pBBoxMaxOut = DirectX::XMVectorMax(*pBBoxMaxOut, vNew);
		*pBBoxMinOut = DirectX::XMVectorMin(*pBBoxMinOut, vNew);
	}
}

void PostProcess::BasePostProcessRender::AddShadersToCache(AMD::ShaderCache * pShaderCache)
{
	const D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0 },
		{ "NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0 },
		{ "TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0 },
		{ "TANGENT",0,DXGI_FORMAT_R32G32B32_FLOAT,0,32,D3D11_INPUT_PER_VERTEX_DATA,0 },
	};

	INT32 size = ARRAYSIZE(layout);

	this->AddShadersToCache(pShaderCache, L"DeferVoxelCutoutVS", L"DeferVoxelCutoutPS", L"DeferredVoxelSphereCutout.hlsl", layout, size);
}

HRESULT PostProcess::BasePostProcessRender::OnD3DDeviceCreated(ID3D11Device * pD3dDevice, const DXGI_SURFACE_DESC * pBackBufferSurfaceDesc, void * pUserContext)
{
	HRESULT hr;
	hr = this->CreateCommonBuffers(pD3dDevice,m_MeshData, m_uSizeConstantBufferPerObject, m_uSizeConstantBufferPerFrame);
	hr = this->CreateOtherRenderStateResources(pD3dDevice);

	return hr;
}

void PostProcess::BasePostProcessRender::OnD3D11DestroyDevice(void * pUserContext)
{
	ReleaseAllD3D11COM();
}

void PostProcess::BasePostProcessRender::OnReleasingSwapChain(void)
{
	this->ReleaseSwapChainAssociatedCOM();
}

void PostProcess::BasePostProcessRender::OnResizedSwapChain(ID3D11Device * pD3dDevice, const DXGI_SURFACE_DESC * pBackBufferSurfaceDesc)
{
	this->ReleaseSwapChainAssociatedCOM();
}

void PostProcess::BasePostProcessRender::OnRender(ID3D11Device * pD3dDevice, ID3D11DeviceContext * pD3dImmediateContext, const DXGI_SURFACE_DESC * pBackBufferDesc, CBaseCamera * pCamera, ID3D11RenderTargetView * pRTV, ID3D11DepthStencilView * pDepthStencilView, ID3D11ShaderResourceView * pDepthStencilCopySRV)
{

}

void PostProcess::BasePostProcessRender::AddShadersToCache(AMD::ShaderCache * pShaderCache, const wchar_t * pwsNameVS, const wchar_t * pwsNamePS, const wchar_t * pwsSourceFileName, const D3D11_INPUT_ELEMENT_DESC layout[], UINT size)
{
	if (!m_bShaderInited)
	{
		SAFE_RELEASE(m_pShaderInputLayout);
		SAFE_RELEASE(m_pShaderVS);
		SAFE_RELEASE(m_pShaderPS);

		//Add vertex shader
		pShaderCache->AddShader((ID3D11DeviceChild**)&m_pShaderVS, AMD::ShaderCache::SHADER_TYPE::SHADER_TYPE_VERTEX,
			L"vs_5_0", pwsNameVS, pwsSourceFileName, 0, nullptr, &m_pShaderInputLayout, (D3D11_INPUT_ELEMENT_DESC*)layout, size);

		//Add pixel shader

		pShaderCache->AddShader((ID3D11DeviceChild**)&m_pShaderPS, AMD::ShaderCache::SHADER_TYPE::SHADER_TYPE_PIXEL,
			L"ps_5_0", pwsNamePS, pwsSourceFileName, 0, nullptr, nullptr, nullptr, 0);

		m_bShaderInited = true;

	}
}

HRESULT PostProcess::BasePostProcessRender::CreateCommonBuffers(ID3D11Device * pD3dDevice, GeometryHelper::MeshData & meshData, UINT constBufferPerObjectSize, UINT constBufferPerFrameSize)
{
	HRESULT hr;
	D3D11_SUBRESOURCE_DATA InitData;

	//Create index buffer
	D3D11_BUFFER_DESC indexBufferDesc;
	ZeroMemory(&indexBufferDesc, sizeof(indexBufferDesc));
	indexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	indexBufferDesc.ByteWidth = sizeof(GeometryHelper::uint32)*m_MeshData.Indices32.size();
	InitData.pSysMem = &m_MeshData.Indices32[0];
	V_RETURN(pD3dDevice->CreateBuffer(&indexBufferDesc, &InitData, &m_pMeshIB));


	//Create vertex buffer
	D3D11_BUFFER_DESC vertexBufferDesc;
	ZeroMemory(&vertexBufferDesc, sizeof(vertexBufferDesc));
	vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.ByteWidth = sizeof(GeometryHelper::Vertex)*m_MeshData.Vertices.size();
	InitData.pSysMem = &m_MeshData.Vertices[0];
	V_RETURN(pD3dDevice->CreateBuffer(&vertexBufferDesc, &InitData, &m_pMeshVB));


	//  Create constant buffers
	D3D11_BUFFER_DESC ConstantBufferDesc;
	ZeroMemory(&ConstantBufferDesc, sizeof(ConstantBufferDesc));
	ConstantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	ConstantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	ConstantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	ConstantBufferDesc.ByteWidth = constBufferPerObjectSize>4? constBufferPerObjectSize :16;
	V_RETURN(pD3dDevice->CreateBuffer(&ConstantBufferDesc, nullptr, &m_pConstantBufferPerObject));

	ConstantBufferDesc.ByteWidth = constBufferPerFrameSize>4 ? constBufferPerFrameSize : 16; 
	V_RETURN(pD3dDevice->CreateBuffer(&ConstantBufferDesc, nullptr, &m_pConstantBufferPerFrame));

	return hr;
}

HRESULT PostProcess::BasePostProcessRender::CreateOtherRenderStateResources(ID3D11Device * pD3dDevice)
{
	HRESULT hr;

	//Create DepthStencilState
	D3D11_DEPTH_STENCIL_DESC DepthStencilDesc;
	DepthStencilDesc.DepthEnable = TRUE;
	DepthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;
	DepthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	DepthStencilDesc.StencilEnable = TRUE;
	DepthStencilDesc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
	DepthStencilDesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
	DepthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_GREATER;
	DepthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	DepthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	DepthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_INCR;
	DepthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_NEVER;
	DepthStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	DepthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	DepthStencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	V_RETURN(pD3dDevice->CreateDepthStencilState(&DepthStencilDesc, &m_pDepthStencilState));


	D3D11_RASTERIZER_DESC RasterizerDesc;
	RasterizerDesc.FillMode = D3D11_FILL_SOLID;
	RasterizerDesc.CullMode = D3D11_CULL_BACK;
	RasterizerDesc.FrontCounterClockwise = FALSE;
	RasterizerDesc.DepthBias = 0.0f;
	RasterizerDesc.DepthBiasClamp = 0.0f;
	RasterizerDesc.SlopeScaledDepthBias = 0.0f;
	RasterizerDesc.DepthClipEnable = TRUE;
	RasterizerDesc.ScissorEnable = FALSE;
	RasterizerDesc.MultisampleEnable = TRUE;
	RasterizerDesc.AntialiasedLineEnable = FALSE;
	V_RETURN(pD3dDevice->CreateRasterizerState(&RasterizerDesc, &m_pRasterizerState));

	//Create Sampler
	D3D11_SAMPLER_DESC SamplerDesc;
	ZeroMemory(&SamplerDesc, sizeof(SamplerDesc));
	SamplerDesc.Filter = D3D11_FILTER_ANISOTROPIC;
	SamplerDesc.AddressU = SamplerDesc.AddressV = SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	SamplerDesc.MaxAnisotropy = 16;
	SamplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
	V_RETURN(pD3dDevice->CreateSamplerState(&SamplerDesc, &m_pSamplerState));
	m_pSamplerState->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("Anisotropic"), "Anisotropic");

	// Create blend states
	D3D11_BLEND_DESC BlendStateDesc;
	BlendStateDesc.AlphaToCoverageEnable = TRUE;
	BlendStateDesc.IndependentBlendEnable = FALSE;
	BlendStateDesc.RenderTarget[0].BlendEnable = TRUE;
	BlendStateDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	BlendStateDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	BlendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;

	BlendStateDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	BlendStateDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	BlendStateDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	BlendStateDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	V_RETURN(pD3dDevice->CreateBlendState(&BlendStateDesc, &m_pBlendState));


	return hr;
}

void PostProcess::BasePostProcessRender::ReleaseAllD3D11COM(void)
{
	this->ReleaseSwapChainAssociatedCOM();
	this->ReleaseOneTimeInitedCOM();
}

void PostProcess::BasePostProcessRender::ReleaseSwapChainAssociatedCOM(void)
{
	SAFE_RELEASE(m_pSrcTextureSRV);
}

void PostProcess::BasePostProcessRender::ReleaseOneTimeInitedCOM(void)
{
	SAFE_RELEASE(m_pMeshIB);
	SAFE_RELEASE(m_pMeshVB);

	SAFE_RELEASE(m_pShaderInputLayout);
	SAFE_RELEASE(m_pShaderVS);
	SAFE_RELEASE(m_pShaderPS);

	SAFE_RELEASE(m_pDepthStencilState);
	SAFE_RELEASE(m_pRasterizerState);
	SAFE_RELEASE(m_pBlendState);
	SAFE_RELEASE(m_pSamplerState);

	SAFE_RELEASE(m_pConstantBufferPerObject);
	SAFE_RELEASE(m_pConstantBufferPerFrame);
}

HRESULT PostProcess::BasePostProcessRender::CreateSwapChainAssociatedResource(ID3D11Device * pD3dDevice, const DXGI_SURFACE_DESC * pBackBufferSurfaceDesc)
{
	HRESULT hr = S_OK;

	return hr;

	//ID3D11Texture2D* pTempTexture2D = nullptr;

	//D3D11_TEXTURE2D_DESC desc;
	//
	//std::memcpy(&desc, pBackBufferSurfaceDesc, sizeof(*pBackBufferSurfaceDesc));
	//desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

	//HRESULT hr = S_OK;

	//hr = pD3dDevice->CreateTexture2D(&desc, nullptr, &m_pTempTexture2D);
	//m_pTempTexture2D->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("GodRays Texture2D"), "GodRays Texture2D");


	//// Create the render target view and SRV
	//hr = pD3dDevice->CreateRenderTargetView(m_pTempTexture2D, nullptr, &m_pTempRTV);
	//hr = pD3dDevice->CreateShaderResourceView(m_pTempTexture2D, nullptr, &m_pTempSRV);
}
