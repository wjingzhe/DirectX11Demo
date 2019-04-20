#include "SphereRender.h"
using namespace DirectX;

PostProcess::SphereRender::SphereRender():BasePostProcessRender()
{
	m_MeshData = GeometryHelper::CreateSphere(160,20,20, DirectX::XMFLOAT3(130.0f, 0.0f, 0.0f));
}

PostProcess::SphereRender::~SphereRender()
{
}

void PostProcess::SphereRender::AddShadersToCache(AMD::ShaderCache * pShaderCache)
{
	const D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0 },
		{ "NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0 },
		{ "TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0 },
		{ "TANGENT",0,DXGI_FORMAT_R32G32B32_FLOAT,0,32,D3D11_INPUT_PER_VERTEX_DATA,0 },
	};

	UINT size = ARRAYSIZE(layout);

	BasePostProcessRender::AddShadersToCache(pShaderCache, L"BaseGeometryVS", L"BaseGeometryPS", L"DrawBasicGeometry.hlsl", layout, size);

}

void PostProcess::SphereRender::OnRender(ID3D11Device * pD3dDevice, ID3D11DeviceContext * pD3dImmediateContext, const DXGI_SURFACE_DESC * pBackBufferDesc, CBaseCamera * pCamera, ID3D11RenderTargetView * pRTV, ID3D11DepthStencilView * pDepthStencilView, ID3D11ShaderResourceView * pDepthStencilCopySRV)
{
	// save Rasterizer State (for later restore)
	ID3D11RasterizerState* pPreRasterizerState = nullptr;
	pD3dImmediateContext->RSGetState(&pPreRasterizerState);

	// save blend state(for later restore)
	ID3D11BlendState* pPreBlendStateStored11 = nullptr;
	FLOAT afBlendFactorStored11[4];
	UINT uSampleMaskStored11;
	pD3dImmediateContext->OMGetBlendState(&pPreBlendStateStored11, afBlendFactorStored11, &uSampleMaskStored11);

	// save depth state (for later restore)
	ID3D11DepthStencilState* pPreDepthStencilStateStored11 = nullptr;
	UINT uStencilRefStored11;
	pD3dImmediateContext->OMGetDepthStencilState(&pPreDepthStencilStateStored11, &uStencilRefStored11);

	//开始绘制在体素盒中的元素颜色


	XMMATRIX mWorld = XMMatrixIdentity();
	mWorld.r[3].m128_f32[0] = m_Position4.x;
	mWorld.r[3].m128_f32[1] = m_Position4.y;
	mWorld.r[3].m128_f32[2] = m_Position4.z;


	//Get the projection & view matrix from the camera class
	XMMATRIX mView = pCamera->GetViewMatrix();
	XMMATRIX mProj = pCamera->GetProjMatrix();
	XMMATRIX mWorldViewPrjection = mWorld*mView*mProj;

	XMFLOAT4 CameraPosAndAlphaTest;
	XMStoreFloat4(&CameraPosAndAlphaTest, pCamera->GetEyePt());
	//different alpha test for msaa vs diabled
	CameraPosAndAlphaTest.w = 0.05f;

	//Set the constant buffers
	HRESULT hr;
	D3D11_MAPPED_SUBRESOURCE MappedResource;


	XMVECTOR det;
	XMMATRIX mWorldViewInv = XMMatrixInverse(&det, mWorld*mView);

	V(pD3dImmediateContext->Map(m_pConstantBufferPerObject, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
	CB_PER_OBJECT* pPerObject = (CB_PER_OBJECT*)MappedResource.pData;
	pPerObject->mWorldViewProjection = XMMatrixTranspose(mWorldViewPrjection);
	pPerObject->mWorld = XMMatrixTranspose(mWorld);
	pPerObject->mWorldViewInv = XMMatrixTranspose(mWorldViewInv);

	pD3dImmediateContext->Unmap(m_pConstantBufferPerObject, 0);
	pD3dImmediateContext->VSSetConstantBuffers(0, 1, &m_pConstantBufferPerObject);
	pD3dImmediateContext->PSSetConstantBuffers(0, 1, &m_pConstantBufferPerObject);
	//	pD3dImmediateContext->CSSetConstantBuffers(0, 1, &m_pConstantBufferPerObject);

	{
		// we need the inverse proj matrix in the per-tile light culling compute shader
		XMVECTOR det;
		XMMATRIX mInvProj = XMMatrixInverse(&det, mProj);

		float nearZ = pCamera->GetNearClip();
		float farZ = pCamera->GetFarClip();
		float fRange = farZ / (farZ - nearZ);

		float FovRadY = pCamera->GetFOV();
		float tanHalfFovY = std::tanf(FovRadY*0.5f);


		V(pD3dImmediateContext->Map(m_pConstantBufferPerFrame, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
		CB_PER_FRAME* pPerFrame = (CB_PER_FRAME*)MappedResource.pData;
		pPerFrame->vCameraPos3AndAlphaTest = XMLoadFloat4(&CameraPosAndAlphaTest);
		pPerFrame->m_mProjection = XMMatrixTranspose(mProj);
		pPerFrame->m_mProjectionInv = XMMatrixTranspose(mInvProj);

		pPerFrame->ProjParams.x = tanHalfFovY;
		pPerFrame->ProjParams.y = pCamera->GetAspect();
		pPerFrame->ProjParams.z = nearZ;
		pPerFrame->ProjParams.w = fRange;
		pPerFrame->RenderTargetHalfSizeAndFarZ.x = pBackBufferDesc->Width / 2.0f;
		pPerFrame->RenderTargetHalfSizeAndFarZ.y = pBackBufferDesc->Height / 2.0f;
		pPerFrame->RenderTargetHalfSizeAndFarZ.z = farZ;


		pD3dImmediateContext->Unmap(m_pConstantBufferPerFrame, 0);
		pD3dImmediateContext->VSSetConstantBuffers(1, 1, &m_pConstantBufferPerFrame);
		pD3dImmediateContext->PSSetConstantBuffers(1, 1, &m_pConstantBufferPerFrame);
		//	pD3dImmediateContext->CSSetConstantBuffers(1, 1, &g_pConstantBufferPerFrame);
	}



	pD3dImmediateContext->OMSetRenderTargets(0, &pRTV, pDepthStencilView);
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

	pD3dImmediateContext->VSSetShader(m_pShaderVS, nullptr, 0);
	pD3dImmediateContext->VSSetConstantBuffers(0, 1, &m_pConstantBufferPerObject);
	pD3dImmediateContext->VSSetConstantBuffers(1, 1, &m_pConstantBufferPerFrame);

	pD3dImmediateContext->PSSetShader(m_pShaderPS, nullptr, 0);
	pD3dImmediateContext->PSSetConstantBuffers(0, 1, &m_pConstantBufferPerObject);
	pD3dImmediateContext->PSSetConstantBuffers(1, 1, &m_pConstantBufferPerFrame);
	pD3dImmediateContext->PSSetShaderResources(0, 1, &pDepthStencilCopySRV);
	//pD3dImmediateContext->PSSetShaderResources(1, 1, &m_pDecalTextureSRV);
	pD3dImmediateContext->PSSetSamplers(0, 1, &m_pSamplerState);


	pD3dImmediateContext->DrawIndexed(m_MeshData.Indices32.size(), 0, 0);


	pD3dImmediateContext->RSSetState(pPreRasterizerState);
	pD3dImmediateContext->OMSetBlendState(pPreBlendStateStored11, afBlendFactorStored11, uSampleMaskStored11);
	pD3dImmediateContext->OMSetDepthStencilState(pPreDepthStencilStateStored11, uStencilRefStored11);

	SAFE_RELEASE(pPreRasterizerState);
	SAFE_RELEASE(pPreBlendStateStored11);
	SAFE_RELEASE(pPreDepthStencilStateStored11);
}

HRESULT PostProcess::SphereRender::CreateOtherRenderStateResources(ID3D11Device * pD3dDevice)
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
