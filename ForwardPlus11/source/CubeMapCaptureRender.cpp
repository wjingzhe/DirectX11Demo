#include "CubeMapCaptureRender.h"
#include "stb_image.h"
using namespace DirectX;

namespace ForwardRender
{
	CubeMapCaptureRender::CubeMapCaptureRender()
		:m_pMeshIB(nullptr),m_pMeshVB(nullptr),
		m_pShaderInputLayout(nullptr),m_pShaderVS(nullptr),m_pShaderPS(nullptr),
		m_pDepthStencilState(nullptr),m_pRasterizerState(nullptr),m_pBlendState(nullptr),m_pSamplerState(nullptr),
		m_pConstantBufferPerObject(nullptr),m_pConstantBufferPerFrame(nullptr),
		m_uSizeConstantBufferPerObject(sizeof(CB_PER_OBJECT)),m_uSizeConstantBufferPerFrame(sizeof(CB_PER_FRAME)),
		//m_pHdrTexture(nullptr),
		m_pSrcTextureSRV(nullptr),
		m_bShaderInited(false)
	{

		g_TempCubeMapCamera.SetProjParams(XM_PI / 4, 1.0f, 1.0f, 1000.0f);
		


		m_MeshData = GeometryHelper::CreateCubePlane(100, 100, 400);
		//m_MeshData = GeometryHelper::CreateBox(100, 100, 100,6,0,0,0);
		m_uSizeConstantBufferPerObject = sizeof(CB_PER_OBJECT);
		m_uSizeConstantBufferPerFrame = sizeof(CB_PER_FRAME);
	}

	CubeMapCaptureRender::~CubeMapCaptureRender()
	{
	}
	void CubeMapCaptureRender::AddShadersToCache(AMD::ShaderCache * pShaderCache)
	{
		const D3D11_INPUT_ELEMENT_DESC layout[] =
		{
			{ "POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0 },
			{ "NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0 },
			{ "TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0 },
			{ "TANGENT",0,DXGI_FORMAT_R32G32B32_FLOAT,0,32,D3D11_INPUT_PER_VERTEX_DATA,0 },
		};


		INT32 size = ARRAYSIZE(layout);
		this->AddShadersToCache(pShaderCache, L"CubeMapCaptureVS", L"CubeMapCapturePS", L"CubeMapCapture.hlsl", layout, size);

	}


	HRESULT CubeMapCaptureRender::OnD3DDeviceCreated(ID3D11Device * pD3dDevice, const DXGI_SURFACE_DESC * pBackBufferSurfaceDesc, void * pUserContext)
	{
		HRESULT hr;

		V_RETURN(this->CreateCommonBuffers(pD3dDevice, m_MeshData, m_uSizeConstantBufferPerObject, m_uSizeConstantBufferPerFrame));
		V_RETURN(this->CreateOtherRenderStateResources(pD3dDevice));

		//stbi_set_flip_vertically_on_load(true);
		//int width, height, nrComponents;
		//float *data = stbi_loadf("hdr/newport_loft.hdr", &width, &height, &nrComponents, 0);

		//CubeMap RTV
		{
			D3D11_TEXTURE2D_DESC texDesc;
			texDesc.Width = 512;
			texDesc.Height = 512;
			texDesc.MipLevels = 0;
			texDesc.ArraySize = 1;
			texDesc.SampleDesc.Count = pBackBufferSurfaceDesc->SampleDesc.Count;
			texDesc.SampleDesc.Quality = pBackBufferSurfaceDesc->SampleDesc.Quality;
			texDesc.Format = pBackBufferSurfaceDesc->Format;
			texDesc.Usage = D3D11_USAGE_DEFAULT;
			texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			texDesc.CPUAccessFlags = 0;
			texDesc.MiscFlags = 0;

			V_RETURN(pD3dDevice->CreateTexture2D(&texDesc, nullptr, &g_pCubeTexture));


			D3D11_RENDER_TARGET_VIEW_DESC RTVDesc;
			ZeroMemory(&RTVDesc, sizeof(D3D11_RENDER_TARGET_VIEW_DESC));
			RTVDesc.Format = pBackBufferSurfaceDesc->Format;
			RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;// D3D11_RTV_DIMENSION_TEXTURE2D);
			RTVDesc.Texture2D.MipSlice = 0;

			D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
			rtvDesc.Format = texDesc.Format;
			rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			//rtvDesc.Texture2DArray.ArraySize = 1;
			//rtvDesc.Texture2D.MipSlice = 0;

			//for (int i = 0; i < 6; ++i)
			{
				//rtvDesc.Texture2DArray.FirstArraySlice = i;
				V_RETURN(pD3dDevice->CreateRenderTargetView(g_pCubeTexture, &RTVDesc, &g_pTextureForEnvCubeMapRTVs));
				g_pTextureForEnvCubeMapRTVs->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("CubeRT"), "CubeRT");
			}


		}

		//create our own depth stencil surface that'bindable as a shader
		V_RETURN(AMD::CreateDepthStencilSurface(&g_pDepthStencilTexture, &g_pDepthStencilSRV, &g_pDepthStencilView,
			DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_R24_UNORM_X8_TYPELESS, 100.0f, 100.0f, pBackBufferSurfaceDesc->SampleDesc.Count));


		return hr;
	}
	void CubeMapCaptureRender::OnD3D11DestroyDevice(void * pUserContext)
	{
		ReleaseAllD3D11COM();

	}
	void CubeMapCaptureRender::OnReleasingSwapChain(void)
	{
		this->ReleaseSwapChainAssociatedCOM();
	}

	void CubeMapCaptureRender::OnResizedSwapChain(ID3D11Device * pD3dDevice, const DXGI_SURFACE_DESC * pBackBufferSurfaceDesc)
	{
		//jingz todo

		this->ReleaseSwapChainAssociatedCOM();
	}

	void CubeMapCaptureRender::OnRender(ID3D11Device * pD3dDevice, ID3D11DeviceContext * pD3dImmediateContext, const DXGI_SURFACE_DESC * pBackBufferDesc, CBaseCamera * pCamera, ID3D11RenderTargetView * pRTV, ID3D11DepthStencilView * pDepthStencilView, ID3D11ShaderResourceView * pDepthStencilCopySRV)
	{	
		// save Rasterizer State (for later restore)
		ID3D11RasterizerState* pPreRasterizerState = nullptr;
		pD3dImmediateContext->RSGetState(&pPreRasterizerState);

		// save blend state(for later restore)
		ID3D11BlendState* pBlendStateStored11 = nullptr;
		FLOAT afBlendFactorStored11[4];
		UINT uSampleMaskStored11;
		pD3dImmediateContext->OMGetBlendState(&pBlendStateStored11, afBlendFactorStored11, &uSampleMaskStored11);

		// save depth state (for later restore)
		ID3D11DepthStencilState* pDepthStencilStateStored11 = nullptr;
		UINT uStencilRefStored11;
		pD3dImmediateContext->OMGetDepthStencilState(&pDepthStencilStateStored11, &uStencilRefStored11);


		//pD3dImmediateContext->OMSetRenderTargets(1,&pRTV,)



	}

	void CubeMapCaptureRender::RenderHDRtoCubeMap(ID3D11Device * pD3dDevice, ID3D11DeviceContext * pD3dImmediateContext, const DXGI_SURFACE_DESC * pBackBufferDesc,
		CBaseCamera * pCamera, ID3D11RenderTargetView* g_pTempTextureRenderTargetView, ID3D11DepthStencilView* g_pTempDepthStencilView)
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
		pD3dImmediateContext->PSSetShaderResources(0, 1, &m_pSrcTextureSRV);
		pD3dImmediateContext->PSSetSamplers(0, 1, &m_pSamplerState);




		//设置ConstantBuffers

		XMMATRIX mWorld = XMMatrixIdentity();
		//XMFLOAT3 pos;
		//XMStoreFloat3(&pos, pCamera->GetEyePt());
		//mWorld.r[3].m128_f32[0] = pos.x;
		//mWorld.r[3].m128_f32[1] = pos.y;
		//mWorld.r[3].m128_f32[2] = pos.z;


		//mWorld.r[0].m128_f32[0] = 999.0f;
		//mWorld.r[1].m128_f32[1] = 999.0f;
		//mWorld.r[2].m128_f32[2] = 999.0f;

		XMMATRIX ViewMatrix[6];
		XMVECTOR vEye = XMVectorSet(0.0, 0.0f, 0.0f, 1.0f);
		{
			XMVECTOR vLookAtPos = XMVectorSet(0.0f, 0.0f, 100.0f, 1.0f);
			g_TempCubeMapCamera.SetViewParams(vEye, vLookAtPos);
			ViewMatrix[0] = g_TempCubeMapCamera.GetViewMatrix();
		}
		

		

		

		////Get the projection & view matrix from the camera class
		//XMMATRIX mView = g_TempCubeMapCamera.GetViewMatrix();
		XMMATRIX mProj = g_TempCubeMapCamera.GetProjMatrix();
		XMMATRIX mWorldViewPrjection = mWorld*ViewMatrix[0] *mProj;

		//Set the constant buffers
		HRESULT hr;
		{
			D3D11_MAPPED_SUBRESOURCE MappedResource;
			V(pD3dImmediateContext->Map(m_pConstantBufferPerObject, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
			CB_PER_OBJECT* pPerObject = (CB_PER_OBJECT*)MappedResource.pData;
			pPerObject->mWorldViewProjection = XMMatrixTranspose(mWorldViewPrjection);
			pPerObject->mWorld = XMMatrixTranspose(mProj);

			pD3dImmediateContext->Unmap(m_pConstantBufferPerObject, 0);
		}

		{
			D3D11_MAPPED_SUBRESOURCE MappedResource;
			V(pD3dImmediateContext->Map(m_pConstantBufferPerFrame, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
			CB_PER_FRAME* pPerFrame = (CB_PER_FRAME*)MappedResource.pData;
			XMFLOAT2 temp;
			temp.x = 100.0f;
			temp.y = 100.0f;
			pPerFrame->vScreenSize = XMLoadFloat2(&temp);
			pD3dImmediateContext->Unmap(m_pConstantBufferPerFrame, 0);
		}


		pD3dImmediateContext->VSSetConstantBuffers(0, 1, &m_pConstantBufferPerObject);
		pD3dImmediateContext->PSSetConstantBuffers(0, 1, &m_pConstantBufferPerObject);


		pD3dImmediateContext->VSSetConstantBuffers(1, 1, &m_pConstantBufferPerFrame);
		pD3dImmediateContext->PSSetConstantBuffers(1, 1, &m_pConstantBufferPerFrame);

	//	for (int i = 0; i < 6; ++i)
		{
			//Bind cube map face as render target.
			pD3dImmediateContext->OMSetRenderTargets(1, &g_pTempTextureRenderTargetView, g_pTempDepthStencilView);

			//Clear cube map face and depth buffer.
			pD3dImmediateContext->ClearRenderTargetView(g_pTempTextureRenderTargetView, reinterpret_cast<const float*>(&Colors::Silver));
			pD3dImmediateContext->ClearDepthStencilView(g_pTempDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

			//Draw the scene with the exception of the center sphere to this cube map face.
			pD3dImmediateContext->DrawIndexed(m_MeshData.Indices32.size(), 0, 0);

		}
		//Have harware generate lower mimap levels of cube map.
		//m_pD3dImmediateContext->GenerateMips(m_pDynamicCubeMapSRV);


		//还原状态
		pD3dImmediateContext->RSSetState(pPreRasterizerState);
		pD3dImmediateContext->OMSetBlendState(pPreBlendStateStored11, BlendFactorStored11, uSampleMaskStored11);
		pD3dImmediateContext->OMSetDepthStencilState(pPreDepthStencilStateStored11, uStencilRefStored11);

		SAFE_RELEASE(pPreRasterizerState);
		SAFE_RELEASE(pPreBlendStateStored11);
		SAFE_RELEASE(pPreDepthStencilStateStored11);
	}

	void CubeMapCaptureRender::AddShadersToCache(AMD::ShaderCache * pShaderCache, const wchar_t * pwsNameVS, const wchar_t * pwsNamePS, const wchar_t * pwsSourceFileName, const D3D11_INPUT_ELEMENT_DESC layout[], UINT size)
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

	HRESULT CubeMapCaptureRender::CreateCommonBuffers(ID3D11Device * pD3dDevice, GeometryHelper::MeshData & meshData, UINT constBufferPerObjectSize, UINT constBufferPerFrameSize)
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
	HRESULT CubeMapCaptureRender::CreateOtherRenderStateResources(ID3D11Device * pD3dDevice)
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
		RasterizerDesc.AntialiasedLineEnable = FALSE;
		V_RETURN(pD3dDevice->CreateRasterizerState(&RasterizerDesc, &m_pRasterizerState));

		//Create Sampler
		D3D11_SAMPLER_DESC SamplerDesc;
		ZeroMemory(&SamplerDesc, sizeof(SamplerDesc));
		SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		SamplerDesc.AddressU = SamplerDesc.AddressV = SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		SamplerDesc.MaxAnisotropy = 16;
		SamplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
		SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
		V_RETURN(pD3dDevice->CreateSamplerState(&SamplerDesc, &m_pSamplerState));
		m_pSamplerState->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("LINEAR"), "LINEAR");

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

	void CubeMapCaptureRender::ReleaseAllD3D11COM(void)
	{
		this->ReleaseSwapChainAssociatedCOM();
		this->ReleaseOneTimeInitedCOM();
	}

	void CubeMapCaptureRender::ReleaseSwapChainAssociatedCOM(void)
	{
	}

	void CubeMapCaptureRender::ReleaseOneTimeInitedCOM(void)
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

		SAFE_RELEASE(m_pSrcTextureSRV);

		SAFE_RELEASE(g_pCubeTexture);
		SAFE_RELEASE(g_pEnvCubeMapSRV);
		//for (int i = 0; i < 6; ++i)
		{
			SAFE_RELEASE(g_pTextureForEnvCubeMapRTVs);
		}

		SAFE_RELEASE(g_pDepthStencilView);
		SAFE_RELEASE(g_pDepthStencilSRV);
		SAFE_RELEASE(g_pDepthStencilTexture);
		
	}

	HRESULT CubeMapCaptureRender::CreateSwapChainAssociatedResource(ID3D11Device * pD3dDevice, const DXGI_SURFACE_DESC * pBackBufferSurfaceDesc)
	{
		return E_NOTIMPL;
	}
}
