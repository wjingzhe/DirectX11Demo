#include "DeferredVoxelCutoutRender.h"
using namespace DirectX;

PostProcess::DeferredVoxelCutoutRender::DeferredVoxelCutoutRender()
	:m_pMeshIB(nullptr), m_pMeshVB(nullptr), m_pPosAndNormalAndTextureInputLayout(nullptr),
	m_pScenePosAndNormalAndTextureVS(nullptr), m_pScenePosAndNormalAndTexturePS(nullptr),
	m_pConstantBufferPerObject(nullptr), m_pConstantBufferPerFrame(nullptr), m_bShaderInited(false),
	m_pRasterizerState(nullptr), m_pDepthLessAndStencilOnlyOneTime(nullptr),

	m_Position4(0.0f, 0.0f, 0.0f, 1.0f),
	//SamplerState
	m_pSamAnisotropic(nullptr),
	m_VoxelParam(100.0f, 100.0f, 100.0f, 1.0f),
	m_CommonColor(0.52f,0.5f,0.5f,0.5f),
	m_OverlapMaskedColor(0.0f,0.0f,0.0f,0.0f)
{
	GenerateMeshData(100, 200, 200, 6);

}

PostProcess::DeferredVoxelCutoutRender::~DeferredVoxelCutoutRender()
{
	ReleaseAllD3D11COM();
}

void PostProcess::DeferredVoxelCutoutRender::GenerateMeshData(float width, float height, float depth, unsigned int numSubdivision)
{
	m_VoxelParam = DirectX::XMFLOAT4(130.0f, 0.0f, 0.0f, width);
	//m_VoxelExtend.w = width;

	m_MeshData = GeometryHelper::CreateSphere(width,20,20, DirectX::XMFLOAT3(130.0f, 0.0f, 0.0f));
}

void PostProcess::DeferredVoxelCutoutRender::AddShadersToCache(AMD::ShaderCache * pShaderCache)
{
	if (!m_bShaderInited)
	{
		SAFE_RELEASE(m_pPosAndNormalAndTextureInputLayout);
		SAFE_RELEASE(m_pScenePosAndNormalAndTextureVS);
		SAFE_RELEASE(m_pScenePosAndNormalAndTexturePS);

		const D3D11_INPUT_ELEMENT_DESC layout[] =
		{
			{ "POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0 },
			{ "NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0 },
			{ "TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0 },
			{ "TANGENT",0,DXGI_FORMAT_R32G32B32_FLOAT,0,32,D3D11_INPUT_PER_VERTEX_DATA,0 },
		};

		//Add vertex shader
		pShaderCache->AddShader((ID3D11DeviceChild**)&m_pScenePosAndNormalAndTextureVS, AMD::ShaderCache::SHADER_TYPE::SHADER_TYPE_VERTEX,
			L"vs_5_0", L"DeferVoxelCutoutVS", L"DeferredVoxelCutout.hlsl", 0, nullptr, &m_pPosAndNormalAndTextureInputLayout, (D3D11_INPUT_ELEMENT_DESC*)layout, ARRAYSIZE(layout));

		//Add pixel shader

		pShaderCache->AddShader((ID3D11DeviceChild**)&m_pScenePosAndNormalAndTexturePS, AMD::ShaderCache::SHADER_TYPE::SHADER_TYPE_PIXEL,
			L"ps_5_0", L"DeferVoxelCutoutPS", L"DeferredVoxelCutout.hlsl", 0, nullptr, nullptr, nullptr, 0);

		m_bShaderInited = true;

	}



}

HRESULT PostProcess::DeferredVoxelCutoutRender::OnD3DDeviceCreated(ID3D11Device * pD3dDevice, const DXGI_SURFACE_DESC * pBackBufferSurfaceDesc, void * pUserContext)
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

	ConstantBufferDesc.ByteWidth = sizeof(CB_PER_OBJECT);
	V_RETURN(pD3dDevice->CreateBuffer(&ConstantBufferDesc, nullptr, &m_pConstantBufferPerObject));

	ConstantBufferDesc.ByteWidth = sizeof(CB_PER_FRAME);
	V_RETURN(pD3dDevice->CreateBuffer(&ConstantBufferDesc, nullptr, &m_pConstantBufferPerFrame));



	//Create DepthStencilState
	D3D11_DEPTH_STENCIL_DESC DepthStencilDesc;
	DepthStencilDesc.DepthEnable = TRUE;
	DepthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;
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
	V_RETURN(pD3dDevice->CreateDepthStencilState(&DepthStencilDesc, &m_pDepthLessAndStencilOnlyOneTime));


	D3D11_RASTERIZER_DESC RasterizerDesc;
	RasterizerDesc.FillMode = D3D11_FILL_SOLID;
	RasterizerDesc.CullMode = D3D11_CULL_NONE;
	RasterizerDesc.FrontCounterClockwise = FALSE;
	RasterizerDesc.DepthBias = 0.0f;
	RasterizerDesc.DepthBiasClamp = 0.0f;
	RasterizerDesc.SlopeScaledDepthBias = 0.0f;
	RasterizerDesc.DepthClipEnable = TRUE;
	RasterizerDesc.ScissorEnable = FALSE;
	RasterizerDesc.MultisampleEnable = TRUE;
	RasterizerDesc.AntialiasedLineEnable = FALSE;
	V_RETURN(pD3dDevice->CreateRasterizerState(&RasterizerDesc, &m_pRasterizerState));

	//Create state objects
	D3D11_SAMPLER_DESC SamplerDesc;
	ZeroMemory(&SamplerDesc, sizeof(SamplerDesc));
	SamplerDesc.Filter = D3D11_FILTER_ANISOTROPIC;
	SamplerDesc.AddressU = SamplerDesc.AddressV = SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	SamplerDesc.MaxAnisotropy = 16;
	SamplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
	V_RETURN(pD3dDevice->CreateSamplerState(&SamplerDesc, &m_pSamAnisotropic));
	m_pSamAnisotropic->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("Anisotropic"), "Anisotropic");


	return hr;
}

void PostProcess::DeferredVoxelCutoutRender::OnD3D11DestroyDevice(void * pUserContext)
{
	ReleaseAllD3D11COM();
}

void PostProcess::DeferredVoxelCutoutRender::OnReleasingSwapChain(void)
{
	ReleaseSwapChainAssociatedCOM();
	return;
}

HRESULT PostProcess::DeferredVoxelCutoutRender::OnResizedSwapChain(ID3D11Device * pD3dDevice, const DXGI_SURFACE_DESC * pBackBufferSurfaceDesc)
{
	HRESULT hr;
	ReleaseSwapChainAssociatedCOM();

	hr = CreateTempShaderRenderTarget(pD3dDevice, pBackBufferSurfaceDesc);

	return hr;
}

void PostProcess::DeferredVoxelCutoutRender::OnRender(ID3D11Device * pD3dDevice, ID3D11DeviceContext * pD3dImmediateContext, const DXGI_SURFACE_DESC * pBackBufferDesc, CBaseCamera * pCamera, ID3D11RenderTargetView * pRTV, ID3D11DepthStencilView * pDepthStencilView, ID3D11ShaderResourceView * pDepthStencilCopySRV)
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

	//清理背景颜色
	float ClearColor[4] = { 0.0f,0.0f,0.0f,0.0f, };
	pD3dImmediateContext->ClearRenderTargetView(pRTV, ClearColor);
	pD3dImmediateContext->ClearDepthStencilView(pDepthStencilView, D3D11_CLEAR_STENCIL, 1.0f, 0);

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
	pPerObject->vBoxExtend = m_VoxelParam;
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
		
		pPerFrame->vCommonColor4 = m_CommonColor;
		pPerFrame->vMaskedColor4 = m_OverlapMaskedColor;


		pD3dImmediateContext->Unmap(m_pConstantBufferPerFrame, 0);
		pD3dImmediateContext->VSSetConstantBuffers(1, 1, &m_pConstantBufferPerFrame);
		pD3dImmediateContext->PSSetConstantBuffers(1, 1, &m_pConstantBufferPerFrame);
		//	pD3dImmediateContext->CSSetConstantBuffers(1, 1, &g_pConstantBufferPerFrame);
	}



	pD3dImmediateContext->OMSetRenderTargets(1, &pRTV, pDepthStencilView);
	pD3dImmediateContext->OMSetDepthStencilState(m_pDepthLessAndStencilOnlyOneTime, 1);
	float BlendFactor[4] = { 0.0f,0.0f,0.0f,0.0f };
	pD3dImmediateContext->OMSetBlendState(nullptr, BlendFactor, 0xFFFFFFFF);
	pD3dImmediateContext->RSSetState(m_pRasterizerState);

	pD3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	pD3dImmediateContext->IASetIndexBuffer(m_pMeshIB, DXGI_FORMAT_R32_UINT, 0);
	pD3dImmediateContext->IASetInputLayout(m_pPosAndNormalAndTextureInputLayout);
	UINT uStride = sizeof(GeometryHelper::Vertex);
	UINT uOffset = 0;
	pD3dImmediateContext->IASetVertexBuffers(0, 1, &m_pMeshVB, &uStride, &uOffset);

	pD3dImmediateContext->VSSetShader(m_pScenePosAndNormalAndTextureVS, nullptr, 0);
	pD3dImmediateContext->VSSetConstantBuffers(0, 1, &m_pConstantBufferPerObject);
	pD3dImmediateContext->VSSetConstantBuffers(1, 1, &m_pConstantBufferPerFrame);

	pD3dImmediateContext->PSSetShader(m_pScenePosAndNormalAndTexturePS, nullptr, 0);
	pD3dImmediateContext->PSSetConstantBuffers(0, 1, &m_pConstantBufferPerObject);
	pD3dImmediateContext->PSSetConstantBuffers(1, 1, &m_pConstantBufferPerFrame);
	pD3dImmediateContext->PSSetShaderResources(0, 1, &pDepthStencilCopySRV);
	//pD3dImmediateContext->PSSetShaderResources(1, 1, &m_pDecalTextureSRV);
	pD3dImmediateContext->PSSetSamplers(0, 1, &m_pSamAnisotropic);


	pD3dImmediateContext->DrawIndexed(m_MeshData.Indices32.size(), 0, 0);


	pD3dImmediateContext->RSSetState(pPreRasterizerState);
	pD3dImmediateContext->OMSetBlendState(pPreBlendStateStored11, afBlendFactorStored11, uSampleMaskStored11);
	pD3dImmediateContext->OMSetDepthStencilState(pPreDepthStencilStateStored11, uStencilRefStored11);

	SAFE_RELEASE(pPreRasterizerState);
	SAFE_RELEASE(pPreBlendStateStored11);
	SAFE_RELEASE(pPreDepthStencilStateStored11);
}

void PostProcess::DeferredVoxelCutoutRender::ReleaseAllD3D11COM(void)
{
	ReleaseSwapChainAssociatedCOM();
	ReleaseOneTimeInitedCOM();
}

void PostProcess::DeferredVoxelCutoutRender::ReleaseSwapChainAssociatedCOM(void)
{
}

void PostProcess::DeferredVoxelCutoutRender::ReleaseOneTimeInitedCOM(void)
{
	SAFE_RELEASE(m_pPosAndNormalAndTextureInputLayout);

	SAFE_RELEASE(m_pMeshIB);
	SAFE_RELEASE(m_pMeshVB);

	SAFE_RELEASE(m_pScenePosAndNormalAndTextureVS);
	SAFE_RELEASE(m_pScenePosAndNormalAndTexturePS);

	SAFE_RELEASE(m_pConstantBufferPerObject);
	SAFE_RELEASE(m_pConstantBufferPerFrame);

	SAFE_RELEASE(m_pDepthLessAndStencilOnlyOneTime);
	SAFE_RELEASE(m_pRasterizerState);

	//SAFE_RELEASE(m_pDecalTextureSRV);
	//SamplerState
	SAFE_RELEASE(m_pSamAnisotropic);
}

HRESULT PostProcess::DeferredVoxelCutoutRender::CreateTempShaderRenderTarget(ID3D11Device * pD3dDevice, const DXGI_SURFACE_DESC * pBackBufferSurfaceDesc)
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
