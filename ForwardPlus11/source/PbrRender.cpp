#include "PbrRender.h"
#include "../../DXUT/Core/WICTextureLoader.h"
#include "../../DXUT/Core/DDSTextureLoader.h"
#include "stb_image.h"

using namespace DirectX;

ForwardRender::PbrRender::PbrRender()
	:m_pMeshIB(nullptr), m_pMeshVB(nullptr),
	m_pShaderInputLayout(nullptr), m_pShaderVS(nullptr), m_pShaderPS(nullptr),
	m_pDepthStencilState(nullptr), m_pRasterizerState(nullptr), m_pBlendState(nullptr), m_pSamplerLinear(nullptr), m_pSamplerPoint(nullptr),
	m_pConstantBufferPerObject(nullptr), m_pConstantBufferPerFrame(nullptr),
	m_uSizeConstantBufferPerObject(sizeof(CB_PER_OBJECT)), m_uSizeConstantBufferPerFrame(sizeof(CB_PER_FRAME))
{
	m_MeshData = GeometryHelper::CreateSphere(1.0, 500.0f, 500.0f);
	//m_MeshData = GeometryHelper::CreateScreenQuad();
	//m_MeshData = GeometryHelper::CreateBox(2,2,2);
}

ForwardRender::PbrRender::~PbrRender()
{
}

void ForwardRender::PbrRender::AddShadersToCache(AMD::ShaderCache * pShaderCache)
{
	if (!m_bShaderInited)
	{
		const D3D11_INPUT_ELEMENT_DESC layout[] =
		{
			{ "POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0 },
			{ "NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0 },
			{ "TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0 },
			{ "TANGENT",0,DXGI_FORMAT_R32G32B32_FLOAT,0,32,D3D11_INPUT_PER_VERTEX_DATA,0 },
		};


		INT32 size = ARRAYSIZE(layout);
		this->AddShadersToCache(pShaderCache, L"PbrVS", L"PbrPS", L"pbr.hlsl", layout, size);



		m_bShaderInited = true;
	}

}

HRESULT ForwardRender::PbrRender::OnD3DDeviceCreated(ID3D11Device * pD3dDevice, const DXGI_SURFACE_DESC * pBackBufferSurfaceDesc, void * pUserContext)
{

	HRESULT hr;

	V_RETURN(this->CreateCommonBuffers(pD3dDevice, m_MeshData, m_uSizeConstantBufferPerObject, m_uSizeConstantBufferPerFrame));
	V_RETURN(this->CreateOtherRenderStateResources(pD3dDevice));

	return hr;
}

void ForwardRender::PbrRender::OnD3D11DestroyDevice(void * pUserContext)
{
	ReleaseAllD3D11COM();
}

void ForwardRender::PbrRender::OnReleasingSwapChain(void)
{
	this->ReleaseSwapChainAssociatedCOM();
}

void ForwardRender::PbrRender::OnResizedSwapChain(ID3D11Device * pD3dDevice, const DXGI_SURFACE_DESC * pBackBufferSurfaceDesc)
{
	this->ReleaseSwapChainAssociatedCOM();
}

void ForwardRender::PbrRender::OnRender(ID3D11Device * pD3dDevice, ID3D11DeviceContext * pD3dImmediateContext, const DXGI_SURFACE_DESC * pBackBufferDesc, CBaseCamera * pCamera, ID3D11RenderTargetView * pRTV, ID3D11DepthStencilView * pDepthStencilView, 
	ID3D11ShaderResourceView * pIrradianceSRV, ID3D11ShaderResourceView * pPrefilterSRV)
{
	HRESULT hr;

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

	{

		//开始渲染
		pD3dImmediateContext->OMSetDepthStencilState(m_pDepthStencilState, 1);

		float BlendFactor[4] = { 0.0f,0.0f,0.0f,0.0f };
		pD3dImmediateContext->OMSetBlendState(m_pBlendState, BlendFactor, 0xFFFFFFFF);
		pD3dImmediateContext->RSSetState(m_pRasterizerState);


		pD3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pD3dImmediateContext->IASetIndexBuffer(m_pMeshIB, DXGI_FORMAT_R32_UINT, 0);
		pD3dImmediateContext->IASetInputLayout(m_pShaderInputLayout);
		UINT uStride = sizeof(GeometryHelper::Vertex);
		UINT uOffset = 0;
		pD3dImmediateContext->IASetVertexBuffers(0, 1, &m_pMeshVB, &uStride, &uOffset);

		pD3dImmediateContext->VSSetShader(m_pShaderVS, nullptr, 0);


		pD3dImmediateContext->PSSetShader(m_pShaderPS, nullptr, 0);
		pD3dImmediateContext->PSSetShaderResources(0, 1, &pIrradianceSRV);
		pD3dImmediateContext->PSSetShaderResources(1, 1, &pPrefilterSRV);
		pD3dImmediateContext->PSSetShaderResources(2, 1, &m_pBrdfSRV);
		pD3dImmediateContext->PSSetShaderResources(3, 1, &m_pAlbedoSRV);
		pD3dImmediateContext->PSSetShaderResources(4, 1, &m_pNormalSRV);
		pD3dImmediateContext->PSSetShaderResources(5, 1, &m_pMetallicSRV);
		pD3dImmediateContext->PSSetShaderResources(6, 1, &m_pRoughnessSRV);
		pD3dImmediateContext->PSSetShaderResources(7, 1, &m_pAmbientOverlapSRV);


		pD3dImmediateContext->PSSetSamplers(0, 1, &m_pSamplerLinear);
		pD3dImmediateContext->PSSetSamplers(1, 1, &m_pSamplerPoint);

		DirectX::XMMATRIX mWorld = DirectX::XMMatrixIdentity();

	

		// lights
		// ------
		XMFLOAT3 lightPositions[] = {
			XMFLOAT3(-10.0f,  10.0f, 10.0f),
			XMFLOAT3(10.0f,  10.0f, 10.0f),
			XMFLOAT3(-10.0f, -10.0f, 10.0f),
			XMFLOAT3(10.0f, -10.0f, 10.0f),
		};
		XMFLOAT3 lightColors[] = {
			XMFLOAT3(300.0f, 300.0f, 300.0f),
			XMFLOAT3(300.0f, 300.0f, 300.0f),
			XMFLOAT3(300.0f, 300.0f, 300.0f),
			XMFLOAT3(300.0f, 300.0f, 300.0f),
		};


		D3D11_MAPPED_SUBRESOURCE MappedResource;
		V(pD3dImmediateContext->Map(m_pConstantBufferPerFrame, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
		CB_PER_FRAME* pPerFrame = (CB_PER_FRAME*)MappedResource.pData;


		XMFLOAT4 CameraPosAndAlphaTest;
		XMStoreFloat4(&CameraPosAndAlphaTest, pCamera->GetEyePt());
		//different alpha test for msaa vs diabled
		CameraPosAndAlphaTest.w = 0.05f;
		pPerFrame->vCameraPos3AndAlphaTest = XMLoadFloat4(&CameraPosAndAlphaTest);

		XMFLOAT3 temp;
		temp.x = g_Viewport.Width;
		temp.y = g_Viewport.Height;
		temp.z = 0.0f;
		pPerFrame->vScreenSize = XMLoadFloat3(&temp);

		for (int i = 0;i<4;++i)
		{
			pPerFrame->vLightPostion[i] = XMLoadFloat3(&lightPositions[i]);
			pPerFrame->vLightColors[i] = XMLoadFloat3(&lightColors[i]);
		}
		
		pD3dImmediateContext->Unmap(m_pConstantBufferPerFrame, 0);

		pD3dImmediateContext->VSSetConstantBuffers(1, 1, &m_pConstantBufferPerFrame);
		pD3dImmediateContext->PSSetConstantBuffers(1, 1, &m_pConstantBufferPerFrame);



		////Get the projection & view matrix from the camera class
		XMMATRIX mProj = pCamera->GetProjMatrix();
		XMMATRIX mWorldViewPrjection = mWorld*pCamera->GetViewMatrix() * mProj;

		//Set the constant buffers
		{
			D3D11_MAPPED_SUBRESOURCE MappedResource;
			V(pD3dImmediateContext->Map(m_pConstantBufferPerObject, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
			CB_PER_OBJECT* pPerObject = (CB_PER_OBJECT*)MappedResource.pData;
			pPerObject->mWorldViewProjection = XMMatrixTranspose(mWorldViewPrjection);
			pPerObject->mWorld = XMMatrixTranspose(mWorld);

			pD3dImmediateContext->Unmap(m_pConstantBufferPerObject, 0);
		}

		pD3dImmediateContext->VSSetConstantBuffers(0, 1, &m_pConstantBufferPerObject);
		pD3dImmediateContext->PSSetConstantBuffers(0, 1, &m_pConstantBufferPerObject);


		//Bind cube map face as render target.
		pD3dImmediateContext->OMSetRenderTargets(1, &pRTV, pDepthStencilView);

		//Clear cube map face and depth buffer.
		//pD3dImmediateContext->ClearRenderTargetView(pRTV, reinterpret_cast<const float*>(&Colors::Black));
		//pD3dImmediateContext->ClearDepthStencilView(pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

		//Draw the scene with the exception of the center sphere to this cube map face.
		pD3dImmediateContext->DrawIndexed(m_MeshData.Indices32.size(), 0, 0);

	}



	//还原状态
	pD3dImmediateContext->RSSetState(pPreRasterizerState);
	pD3dImmediateContext->OMSetBlendState(pPreBlendStateStored11, BlendFactorStored11, uSampleMaskStored11);
	pD3dImmediateContext->OMSetDepthStencilState(pPreDepthStencilStateStored11, uStencilRefStored11);

	SAFE_RELEASE(pPreRasterizerState);
	SAFE_RELEASE(pPreBlendStateStored11);
	SAFE_RELEASE(pPreDepthStencilStateStored11);


}

void ForwardRender::PbrRender::AddShadersToCache(AMD::ShaderCache * pShaderCache, const wchar_t * pwsNameVS, const wchar_t * pwsNamePS, const wchar_t * pwsSourceFileName, const D3D11_INPUT_ELEMENT_DESC layout[], UINT size)
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
}

HRESULT ForwardRender::PbrRender::CreateCommonBuffers(ID3D11Device * pD3dDevice, GeometryHelper::MeshData & meshData, UINT constBufferPerObjectSize, UINT constBufferPerFrameSize)
{
	HRESULT hr;

	{
		D3D11_SUBRESOURCE_DATA InitData;

		//Create index buffer
		D3D11_BUFFER_DESC indexBufferDesc;
		ZeroMemory(&indexBufferDesc, sizeof(indexBufferDesc));
		indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		indexBufferDesc.ByteWidth = sizeof(GeometryHelper::uint32)*m_MeshData.Indices32.size();

		InitData.pSysMem = &m_MeshData.Indices32[0];
		V_RETURN(pD3dDevice->CreateBuffer(&indexBufferDesc, &InitData, &m_pMeshIB));
	}

	{
		D3D11_SUBRESOURCE_DATA InitData;

		//Create vertex buffer
		D3D11_BUFFER_DESC vertexBufferDesc;
		ZeroMemory(&vertexBufferDesc, sizeof(vertexBufferDesc));
		vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		vertexBufferDesc.ByteWidth = sizeof(GeometryHelper::Vertex)*m_MeshData.Vertices.size();

		InitData.pSysMem = &m_MeshData.Vertices[0];

		V_RETURN(pD3dDevice->CreateBuffer(&vertexBufferDesc, &InitData, &m_pMeshVB));
	}


	//  Create constant buffers
	D3D11_BUFFER_DESC ConstantBufferDesc;
	ZeroMemory(&ConstantBufferDesc, sizeof(ConstantBufferDesc));
	ConstantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	ConstantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	ConstantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	ConstantBufferDesc.ByteWidth = constBufferPerObjectSize > 4 ? constBufferPerObjectSize : 16;
	V_RETURN(pD3dDevice->CreateBuffer(&ConstantBufferDesc, nullptr, &m_pConstantBufferPerObject));

	ConstantBufferDesc.ByteWidth = constBufferPerFrameSize > 4 ? constBufferPerFrameSize : 16;
	V_RETURN(pD3dDevice->CreateBuffer(&ConstantBufferDesc, nullptr, &m_pConstantBufferPerFrame));

	return hr;
}

HRESULT ForwardRender::PbrRender::CreateOtherRenderStateResources(ID3D11Device * pD3dDevice)
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
	DepthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	DepthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	DepthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	DepthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_INCR;
	DepthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	DepthStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	DepthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	DepthStencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_INCR;
	V_RETURN(pD3dDevice->CreateDepthStencilState(&DepthStencilDesc, &m_pDepthStencilState));


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
	RasterizerDesc.AntialiasedLineEnable = TRUE;
	V_RETURN(pD3dDevice->CreateRasterizerState(&RasterizerDesc, &m_pRasterizerState));

	//Create Sampler
	D3D11_SAMPLER_DESC SamplerDesc;
	ZeroMemory(&SamplerDesc, sizeof(SamplerDesc));
	SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	SamplerDesc.AddressU = SamplerDesc.AddressV = SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	SamplerDesc.MaxAnisotropy = 1;
	SamplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
	V_RETURN(pD3dDevice->CreateSamplerState(&SamplerDesc, &m_pSamplerLinear));
	m_pSamplerLinear->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("LINEAR"), "LINEAR");

	SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	V_RETURN(pD3dDevice->CreateSamplerState(&SamplerDesc, &m_pSamplerPoint));
	m_pSamplerPoint->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("Point"), "Point");

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



	V_RETURN(DirectX::CreateWICTextureFromFile(pD3dDevice, L"../media/hdr/brdfLUT.png", (ID3D11Resource**)&m_pBrdfLUT, &m_pBrdfSRV,false));


	//stbi_set_flip_vertically_on_load(true);
	//int width, height, nrComponents;
	//
	////unsigned char *data = stbi_load("../../1333380921_3046.png", &width, &height, &nrComponents, 0);
	////unsigned char *data = stbi_load("../media/hdr/newport_loft.hdr", &width, &height, &nrComponents, 0);
	//unsigned char *data = stbi_load("../media/hdr/albedo.png", &width, &height, &nrComponents, 0);

	//D3D11_TEXTURE2D_DESC texDesc;
	//texDesc.Width = width;
	//texDesc.Height = height;
	//texDesc.MipLevels = 1;
	//texDesc.ArraySize = 1;//arraysize
	//texDesc.SampleDesc.Count = 1;
	//texDesc.SampleDesc.Quality = 0;
	//texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	//texDesc.Usage = D3D11_USAGE_DEFAULT;
	//texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	//texDesc.CPUAccessFlags = 0;
	//texDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;//Texture Cube 


	//D3D11_SUBRESOURCE_DATA InitData;
	//InitData.pSysMem = data;
	//InitData.SysMemPitch = width*nrComponents;
	//InitData.SysMemSlicePitch = width*height*nrComponents;

	//V_RETURN(pD3dDevice->CreateTexture2D(&texDesc, &InitData, &m_pAlbedo));

	//D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	//srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	//srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	//srvDesc.Texture2D.MipLevels = 1;
	//srvDesc.Texture2D.MostDetailedMip = 0;
	//V_RETURN(pD3dDevice->CreateShaderResourceView(m_pAlbedo, &srvDesc, &m_pAlbedoSRV));

	//stbi_image_free(data);
	//ID3D11DeviceContext * pImmediateContext;
	//pD3dDevice->GetImmediateContext(&pImmediateContext);
	//pImmediateContext->GenerateMips(m_pAlbedoSRV);
	//SAFE_RELEASE(pImmediateContext);

	//V_RETURN(DirectX::CreateWICTextureFromFile(pD3dDevice, L"../media/hdr/albedo.png", (ID3D11Resource**)&m_pAlbedo, &m_pAlbedoSRV));
	//V_RETURN(DirectX::CreateWICTextureFromFile(pD3dDevice, L"../media/hdr/normal.png", (ID3D11Resource**)&m_pNormal, &m_pNormalSRV));
	//V_RETURN(DirectX::CreateWICTextureFromFile(pD3dDevice, L"../media/hdr/metallic.png", (ID3D11Resource**)&m_pMetallic, &m_pMetallicSRV));
	//V_RETURN(DirectX::CreateWICTextureFromFile(pD3dDevice, L"../media/hdr/roughness.png", (ID3D11Resource**)&m_pRoughness, &m_pRoughnessSRV));
	//V_RETURN(DirectX::CreateWICTextureFromFile(pD3dDevice, L"../media/hdr/ao.png", (ID3D11Resource**)&m_pAmbientOverlap, &m_pAmbientOverlapSRV));

	V_RETURN(DirectX::CreateDDSTextureFromFileEx(pD3dDevice, L"../media/hdr/albedo.dds",0, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE,0,0,true,(ID3D11Resource**)&m_pAlbedo, &m_pAlbedoSRV));
	V_RETURN(DirectX::CreateDDSTextureFromFileEx(pD3dDevice, L"../media/hdr/normal.dds", 0, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0, false, (ID3D11Resource**)&m_pNormal, &m_pNormalSRV));
	V_RETURN(DirectX::CreateDDSTextureFromFileEx(pD3dDevice, L"../media/hdr/metallic.dds", 0, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0, false, (ID3D11Resource**)&m_pMetallic, &m_pMetallicSRV));
	V_RETURN(DirectX::CreateDDSTextureFromFileEx(pD3dDevice, L"../media/hdr/roughness.dds", 0, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0, false, (ID3D11Resource**)&m_pRoughness, &m_pRoughnessSRV));
	V_RETURN(DirectX::CreateDDSTextureFromFileEx(pD3dDevice, L"../media/hdr/ao.dds", 0, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0, false, (ID3D11Resource**)&m_pAmbientOverlap, &m_pAmbientOverlapSRV));

	return hr;
}

void ForwardRender::PbrRender::ReleaseAllD3D11COM(void)
{
	this->ReleaseSwapChainAssociatedCOM();
	this->ReleaseOneTimeInitedCOM();
}

void ForwardRender::PbrRender::ReleaseSwapChainAssociatedCOM(void)
{
}

void ForwardRender::PbrRender::ReleaseOneTimeInitedCOM(void)
{
	SAFE_RELEASE(m_pMeshIB);
	SAFE_RELEASE(m_pMeshVB);

	SAFE_RELEASE(m_pShaderInputLayout);
	SAFE_RELEASE(m_pShaderVS);
	SAFE_RELEASE(m_pShaderPS);

	SAFE_RELEASE(m_pDepthStencilState);
	SAFE_RELEASE(m_pRasterizerState);
	SAFE_RELEASE(m_pBlendState);
	SAFE_RELEASE(m_pSamplerLinear);
	SAFE_RELEASE(m_pSamplerPoint);

	SAFE_RELEASE(m_pConstantBufferPerObject);
	SAFE_RELEASE(m_pConstantBufferPerFrame);

	SAFE_RELEASE(m_pSrcTextureSRV);

	SAFE_RELEASE(g_pDepthStencilTexture);
	SAFE_RELEASE(g_pDepthStencilView);
	SAFE_RELEASE(g_pDepthStencilSRV);

	SAFE_RELEASE(m_pBrdfLUT);
	SAFE_RELEASE(m_pBrdfSRV);

	SAFE_RELEASE(m_pAlbedo);
	SAFE_RELEASE(m_pAlbedoSRV);

	SAFE_RELEASE(m_pNormal);
	SAFE_RELEASE(m_pNormalSRV);

	SAFE_RELEASE(m_pMetallic);
	SAFE_RELEASE(m_pMetallicSRV);

	SAFE_RELEASE(m_pRoughness);
	SAFE_RELEASE(m_pRoughnessSRV);

	SAFE_RELEASE(m_pAmbientOverlap);
	SAFE_RELEASE(m_pAmbientOverlapSRV);
}

HRESULT ForwardRender::PbrRender::CreateSwapChainAssociatedResource(ID3D11Device * pD3dDevice, const DXGI_SURFACE_DESC * pBackBufferSurfaceDesc)
{
	return E_NOTIMPL;
}
