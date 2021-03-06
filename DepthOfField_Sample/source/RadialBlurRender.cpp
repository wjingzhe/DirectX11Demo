#include "RadialBlurRender.h"

PostProcess::RadialBlurRender::RadialBlurRender():BasePostProcessRender(), m_pRadialBlurTextureSRV(m_pSrcTextureSRV)
{
	m_RadialBlurCenterUV = DirectX::XMFLOAT2(0.5f, 0.5f);

	m_MeshData = GeometryHelper::CreateScreenQuad();
	m_uSizeConstantBufferPerObject = sizeof(CB_PER_OBJECT);
	m_uSizeConstantBufferPerFrame = sizeof(CB_PER_FRAME);
	m_fRadialBlurLength = 0.8f;
	m_iSampleCount = 100;
}

PostProcess::RadialBlurRender::~RadialBlurRender()
{
}

void PostProcess::RadialBlurRender::AddShadersToCache(AMD::ShaderCache * pShaderCache)
{
	const D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0 },
		{ "NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0 },
		{ "TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0 },
		{ "TANGENT",0,DXGI_FORMAT_R32G32B32_FLOAT,0,32,D3D11_INPUT_PER_VERTEX_DATA,0 },
	};

	UINT size = ARRAYSIZE(layout);

	BasePostProcessRender::AddShadersToCache(pShaderCache, L"RadialBlurVS", L"RadialBlurPS", L"RadialBlur.hlsl", layout, size);

}

void PostProcess::RadialBlurRender::OnRender(ID3D11Device * pD3dDevice, ID3D11DeviceContext * pD3dImmediateContext, const DXGI_SURFACE_DESC * pBackBufferDesc, CBaseCamera * pCamera, ID3D11RenderTargetView * pRTV, ID3D11DepthStencilView * pDepthStencilView, ID3D11ShaderResourceView * pDepthStencilCopySRV)
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
	
	//Clear the backBuffer
	float ClearColor[4] = { 0.000f, 0.000f, 0.000f, 0.0f };
	pD3dImmediateContext->ClearRenderTargetView(pRTV, ClearColor);


	pD3dImmediateContext->OMSetRenderTargets(1, &pRTV, pDepthStencilView);
	pD3dImmediateContext->OMSetDepthStencilState(m_pDepthStencilState, 1);
	float BlendFactor[4] = { 0.0f,0.0f,0.0f,0.0f };
	pD3dImmediateContext->OMSetBlendState(nullptr, BlendFactor, 0xFFFFFFFF);
	pD3dImmediateContext->RSSetState(m_pRasterizerState);


	pD3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	pD3dImmediateContext->IASetIndexBuffer(m_pMeshIB, DXGI_FORMAT_R32_UINT, 0);
	pD3dImmediateContext->IASetInputLayout(m_pShaderInputLayout);
	UINT uStride = sizeof(GeometryHelper::Vertex);
	UINT uOffset = 0;
	pD3dImmediateContext->IASetVertexBuffers(0, 1, &m_pMeshVB, &uStride, &uOffset);

	{
		//Set the constant buffers
		HRESULT hr;
		D3D11_MAPPED_SUBRESOURCE MappedResource;
		V(pD3dImmediateContext->Map(m_pConstantBufferPerObject, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
		CB_PER_OBJECT* pPerObject = (CB_PER_OBJECT*)MappedResource.pData;
		pPerObject->RadialBlurCenterUV = m_RadialBlurCenterUV;
		pPerObject->RadialBlurCenterUV = m_RadialBlurCenterUV;
		pPerObject->fRadialBlurLength = m_fRadialBlurLength;
		pPerObject->iSampleCount = m_iSampleCount;
		pD3dImmediateContext->Unmap(m_pConstantBufferPerObject, 0);
		pD3dImmediateContext->VSSetConstantBuffers(0, 1, &m_pConstantBufferPerObject);
		pD3dImmediateContext->PSSetConstantBuffers(0, 1, &m_pConstantBufferPerObject);


		//V(pD3dImmediateContext->Map(m_pConstantBufferPerFrame, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
		//CB_PER_FRAME* pPerFrame = (CB_PER_FRAME*)MappedResource.pData;
		////DO NOTHING
		//pD3dImmediateContext->Unmap(m_pConstantBufferPerFrame, 0);
		//pD3dImmediateContext->VSSetConstantBuffers(1, 1, &m_pConstantBufferPerFrame);
		//pD3dImmediateContext->PSSetConstantBuffers(1, 1, &m_pConstantBufferPerFrame);

	}


	pD3dImmediateContext->VSSetShader(m_pShaderVS, nullptr, 0);

	pD3dImmediateContext->PSSetShader(m_pShaderPS, nullptr, 0);
	pD3dImmediateContext->PSSetShaderResources(0, 1, &m_pRadialBlurTextureSRV);
	pD3dImmediateContext->PSSetSamplers(0, 1, &m_pSamplerState);


	pD3dImmediateContext->DrawIndexed(m_MeshData.Indices32.size(), 0, 0);

	pD3dImmediateContext->RSSetState(pPreRasterizerState);
	pD3dImmediateContext->OMSetBlendState(pPreBlendStateStored11, BlendFactorStored11, uSampleMaskStored11);
	pD3dImmediateContext->OMSetDepthStencilState(pPreDepthStencilStateStored11, uStencilRefStored11);

	SAFE_RELEASE(pPreRasterizerState);
	SAFE_RELEASE(pPreBlendStateStored11);
	SAFE_RELEASE(pPreDepthStencilStateStored11);
}

HRESULT PostProcess::RadialBlurRender::CreateOtherRenderStateResources(ID3D11Device * pD3dDevice)
{
	HRESULT hr;

	//Create DepthStencilState
	D3D11_DEPTH_STENCIL_DESC DepthStencilDesc;
	DepthStencilDesc.DepthEnable = TRUE;
	DepthStencilDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
	DepthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	DepthStencilDesc.StencilEnable = TRUE;
	DepthStencilDesc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
	DepthStencilDesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
	DepthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;
	DepthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	DepthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	DepthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
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
	SamplerDesc.AddressU = SamplerDesc.AddressV = SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	SamplerDesc.MaxAnisotropy = 16;
	SamplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
	V_RETURN(pD3dDevice->CreateSamplerState(&SamplerDesc, &m_pSamplerState));
	m_pSamplerState->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("Anisotropic"), "Anisotropic");

	//// Create blend states
	//D3D11_BLEND_DESC BlendStateDesc;
	//BlendStateDesc.AlphaToCoverageEnable = TRUE;
	//BlendStateDesc.IndependentBlendEnable = FALSE;
	//BlendStateDesc.RenderTarget[0].BlendEnable = FALSE;
	//BlendStateDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	//BlendStateDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	//BlendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;

	//BlendStateDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	//BlendStateDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	//BlendStateDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	//BlendStateDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	//V_RETURN(pD3dDevice->CreateBlendState(&BlendStateDesc, &m_pBlendState));




	return hr;
}
