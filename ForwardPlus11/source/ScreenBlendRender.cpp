
#include "ScreenBlendRender.h"

PostProcess::ScreenBlendRender::ScreenBlendRender():BasePostProcessRender(), m_pBlendTextureSRV(m_pSrcTextureSRV)
{
	m_MeshData = GeometryHelper::CreateScreenQuad();
	m_uSizeConstantBufferPerObject = 0;
	m_uSizeConstantBufferPerFrame = 0;
}

PostProcess::ScreenBlendRender::~ScreenBlendRender()
{
}

void PostProcess::ScreenBlendRender::AddShadersToCache(AMD::ShaderCache * pShaderCache)
{
	const D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0 },
		{ "NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0 },
		{ "TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0 },
		{ "TANGENT",0,DXGI_FORMAT_R32G32B32_FLOAT,0,32,D3D11_INPUT_PER_VERTEX_DATA,0 },
	};

	UINT size = ARRAYSIZE(layout);

	BasePostProcessRender::AddShadersToCache(pShaderCache, L"ScreenBlendVS", L"ScreenBlendPS", L"ScreenBlend.hlsl", layout, size);
}

void PostProcess::ScreenBlendRender::OnRender(ID3D11Device * pD3dDevice, ID3D11DeviceContext * pD3dImmediateContext, const DXGI_SURFACE_DESC * pBackBufferDesc, CBaseCamera * pCamera, ID3D11RenderTargetView * pRTV, ID3D11DepthStencilView * pDepthStencilView, ID3D11ShaderResourceView * pDepthStencilCopySRV)
{
	// save Rasterizer State (for later restore)
	ID3D11RasterizerState* pPreRasterizerState = nullptr;
	pD3dImmediateContext->RSGetState(&pPreRasterizerState);

	// save blend state(for later restore)
	ID3D11BlendState* pPreBlendStateStored11 = nullptr;
	FLOAT BlendFactorStored11[4];
	UINT uSampleMaskStored11;
	pD3dImmediateContext->OMGetBlendState(&pPreBlendStateStored11, BlendFactorStored11, &uSampleMaskStored11);

	// save depth state (for later restore)
	ID3D11DepthStencilState* pPreDepthStencilStateStored11 = nullptr;
	UINT uStencilRefStored11;
	pD3dImmediateContext->OMGetDepthStencilState(&pPreDepthStencilStateStored11, &uStencilRefStored11);



	pD3dImmediateContext->OMSetRenderTargets(1, &pRTV, pDepthStencilView);
	pD3dImmediateContext->OMSetDepthStencilState(m_pDepthStencilState, 1);
	//pD3dImmediateContext->ClearDepthStencilView(pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);


	float BlendFactor[4] = { 0.0f,0.0f,0.0f,0.0f };
	pD3dImmediateContext->OMSetBlendState(m_pBlendState, BlendFactor, 0xFFFFFFFF);
	pD3dImmediateContext->RSSetState(m_pRasterizerState);


	pD3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	pD3dImmediateContext->IASetIndexBuffer(m_pMeshIB, DXGI_FORMAT_R32_UINT, 0);
	pD3dImmediateContext->IASetInputLayout(m_pShaderInputLayout);
	UINT uStride = sizeof(GeometryHelper::Vertex);
	UINT uOffset = 0;
	pD3dImmediateContext->IASetVertexBuffers(0, 1, &m_pMeshVB, &uStride, &uOffset);

	pD3dImmediateContext->VSSetShader(m_pShaderVS_Pos_Normal_UV, nullptr, 0);

	pD3dImmediateContext->PSSetShader(m_pShaderPS_Pos_Normal_UV, nullptr, 0);
	pD3dImmediateContext->PSSetShaderResources(0, 1, &m_pBlendTextureSRV);
	pD3dImmediateContext->PSSetSamplers(0, 1, &m_pSamplerState);


	pD3dImmediateContext->DrawIndexed(m_MeshData.Indices32.size(), 0, 0);

	pD3dImmediateContext->RSSetState(pPreRasterizerState);
	pD3dImmediateContext->OMSetBlendState(pPreBlendStateStored11, BlendFactorStored11, uSampleMaskStored11);
	pD3dImmediateContext->OMSetDepthStencilState(pPreDepthStencilStateStored11, uStencilRefStored11);

	SAFE_RELEASE(pPreRasterizerState);
	SAFE_RELEASE(pPreBlendStateStored11);
	SAFE_RELEASE(pPreDepthStencilStateStored11);
}

HRESULT PostProcess::ScreenBlendRender::CreateOtherRenderStateResources(ID3D11Device * pD3dDevice)
{
	HRESULT hr;

	//Create DepthStencilState
	D3D11_DEPTH_STENCIL_DESC DepthStencilDesc;
	DepthStencilDesc.DepthEnable = TRUE;
	DepthStencilDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
	DepthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	DepthStencilDesc.StencilEnable = FALSE;
	DepthStencilDesc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
	DepthStencilDesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
	DepthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;
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
	RasterizerDesc.DepthBias = 0;
	RasterizerDesc.DepthBiasClamp = 0.0f;
	RasterizerDesc.SlopeScaledDepthBias = 0.0f;
	RasterizerDesc.DepthClipEnable = TRUE;
	RasterizerDesc.ScissorEnable = FALSE;
	RasterizerDesc.MultisampleEnable = TRUE;
	RasterizerDesc.AntialiasedLineEnable = TRUE;
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
	BlendStateDesc.AlphaToCoverageEnable = FALSE;//jingz 这个参数导致一些bug
	BlendStateDesc.IndependentBlendEnable = FALSE;
	BlendStateDesc.RenderTarget[0].BlendEnable = TRUE;
	BlendStateDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	BlendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	BlendStateDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;

	BlendStateDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	BlendStateDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	BlendStateDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	BlendStateDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	V_RETURN(pD3dDevice->CreateBlendState(&BlendStateDesc, &m_pBlendState));




	return hr;
}

