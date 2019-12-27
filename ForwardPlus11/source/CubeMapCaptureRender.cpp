#include "CubeMapCaptureRender.h"
#include "stb_image.h"
#include "../../DXUT/Core/WICTextureLoader.h"
#include "../../DXUT/Core/DDSTextureLoader.h"



//// Does not work with compressed formats.
//static HRESULT CreateTexture2DArraySRV(
//	ID3D11Device * device,
//	ID3D11DeviceContext * context,
//	std::vector<std::wstring> &fileName,
//	ID3D11ShaderResourceView **pID3D11ShaderResourceView
//);



using namespace DirectX;

#define CUBEMAP_SIZE 512

namespace ForwardRender
{
	CubeMapCaptureRender::CubeMapCaptureRender()
		:m_pMeshIB(nullptr),m_pMeshVB(nullptr),
		m_pShaderInputLayout(nullptr),m_pShaderVS(nullptr),m_pShaderPS(nullptr), 
		m_pShaderInputLayout2(nullptr), m_pShaderVS2(nullptr), m_pShaderPS2(nullptr),
		m_pShaderInputLayout3(nullptr), m_pShaderVS3(nullptr), m_pShaderPS3(nullptr),
		m_pDepthStencilState(nullptr), m_pDisableDepthStencilState(nullptr),
		m_pRasterizerState(nullptr),m_pBlendState(nullptr),m_pSamplerLinear(nullptr), m_pSamplerPoint(nullptr),
		m_pConstantBufferPerObject(nullptr),m_pConstantBufferPerFrame(nullptr),
		m_uSizeConstantBufferPerObject(sizeof(CB_PER_OBJECT)),m_uSizeConstantBufferPerFrame(sizeof(CB_PER_FRAME)),
		//m_pHdrTexture(nullptr),
		m_pSrcTextureSRV(nullptr),
		m_bShaderInited(false)
	{

		g_TempCubeMapCamera.SetProjParams(XM_PI / 2, 1.0f, 1.0f, 1000.0f);
		

		g_Viewport32.Width = 32;
		g_Viewport32.Height = 32;
		g_Viewport32.MinDepth = 0;
		g_Viewport32.MaxDepth = 1;
		g_Viewport32.TopLeftX = 0;
		g_Viewport32.TopLeftY = 0;

		g_Viewport128.Width = 128;
		g_Viewport128.Height = 128;
		g_Viewport128.MinDepth = 0;
		g_Viewport128.MaxDepth = 1;
		g_Viewport128.TopLeftX = 0;
		g_Viewport128.TopLeftY = 0;

		g_Viewport512.Width = CUBEMAP_SIZE;
		g_Viewport512.Height = CUBEMAP_SIZE;
		g_Viewport512.MinDepth = 0;
		g_Viewport512.MaxDepth = 1;
		g_Viewport512.TopLeftX = 0;
		g_Viewport512.TopLeftY = 0;


		//Use world up vector(0,1,0) for all direction
		XMVECTOR UpVector[6] =
		{
			XMVectorSet(0.0f,1.0f,0.0f, 1.0f),//+x
			XMVectorSet(0.0f,1.0f,0.0f, 1.0f),//-x
			XMVectorSet(0.0f,0.0f,-1.0f, 1.0f),//+Y
			XMVectorSet(0.0f,0.0f,+1.0f, 1.0f),//-Y
			XMVectorSet(0.0f,1.0f,0.0f, 1.0f),//+Z
			XMVectorSet(0.0f,1.0f,0.0f, 1.0f),//-Z
		};

		XMVECTOR vEye = XMVectorSet(0.0, 0.0f, 0.0f, 1.0f);

		{
			XMVECTOR vLookAtPos = XMVectorSet(100.0f, 0.0f, 0.0f, 1.0f);
			g_TempCubeMapCamera.SetViewParams(vEye, vLookAtPos, UpVector[0]);
			m_ViewMatrix[0] = g_TempCubeMapCamera.GetViewMatrix();
		}

		{
			XMVECTOR vLookAtPos = XMVectorSet(-100.0f, 0.0f, 0.0f, 1.0f);
			g_TempCubeMapCamera.SetViewParams(vEye, vLookAtPos, UpVector[1]);
			m_ViewMatrix[1] = g_TempCubeMapCamera.GetViewMatrix();
		}
		{
			XMVECTOR vLookAtPos = XMVectorSet(0.0f, 100.0f, 0.0f, 1.0f);
			g_TempCubeMapCamera.SetViewParams(vEye, vLookAtPos, UpVector[2]);
			m_ViewMatrix[2] = g_TempCubeMapCamera.GetViewMatrix();
		}

		{
			XMVECTOR vLookAtPos = XMVectorSet(0.0f, -100.0f, 0.0f, 1.0f);
			g_TempCubeMapCamera.SetViewParams(vEye, vLookAtPos, UpVector[3]);
			m_ViewMatrix[3] = g_TempCubeMapCamera.GetViewMatrix();
		}
		{
			XMVECTOR vLookAtPos = XMVectorSet(0.0f, 0.0f, 100.0f, 1.0f);
			g_TempCubeMapCamera.SetViewParams(vEye, vLookAtPos, UpVector[4]);
			m_ViewMatrix[4] = g_TempCubeMapCamera.GetViewMatrix();
		}
		{
			XMVECTOR vLookAtPos = XMVectorSet(0.0f, 0.0f, -100.0f, 1.0f);
			g_TempCubeMapCamera.SetViewParams(vEye, vLookAtPos, UpVector[5]);
			m_ViewMatrix[5] = g_TempCubeMapCamera.GetViewMatrix();
		}


		m_MeshData = GeometryHelper::CreateCubePlane(CUBEMAP_SIZE, CUBEMAP_SIZE, CUBEMAP_SIZE);
		//m_MeshData = GeometryHelper::CreateBox(100, 100, 100,6,0,0,0);
		m_uSizeConstantBufferPerObject = sizeof(CB_PER_OBJECT);
		m_uSizeConstantBufferPerFrame = sizeof(CB_PER_FRAME);
	}

	CubeMapCaptureRender::~CubeMapCaptureRender()
	{
	}

	void CubeMapCaptureRender::AddShadersToCache(AMD::ShaderCache * pShaderCache)
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
			this->AddShadersToCache(pShaderCache, L"CubeMapCaptureVS", L"CubeMapCapturePS", L"CubeMapCapture.hlsl", layout, size);


			this->AddShadersToCache2(pShaderCache, L"CubeMapCaptureVS", L"CubeMapCapturePS", L"irradiance_convolution.hlsl", layout, size);

			this->AddShadersToCache3(pShaderCache, L"CubeMapCaptureVS", L"CubeMapCapturePS", L"prefilter.hlsl", layout, size);


			m_bShaderInited = true;
		}



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
			texDesc.Width = CUBEMAP_SIZE;
			texDesc.Height = CUBEMAP_SIZE;
			texDesc.MipLevels = 1;
			texDesc.ArraySize = 6;//arraysize
			texDesc.SampleDesc.Count = 1;
			texDesc.SampleDesc.Quality = 0;
			texDesc.Format = pBackBufferSurfaceDesc->Format;
			texDesc.Usage = D3D11_USAGE_DEFAULT;
			texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			texDesc.CPUAccessFlags = 0;
			texDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS | D3D11_RESOURCE_MISC_TEXTURECUBE;//Texture Cube 

			V_RETURN(pD3dDevice->CreateTexture2D(&texDesc, nullptr, &g_pCubeTexture));

			D3D11_TEXTURE2D_DESC IrradianceCubeTextureDesc;
			memcpy(&IrradianceCubeTextureDesc, &texDesc, sizeof(D3D11_TEXTURE2D_DESC));
			IrradianceCubeTextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			IrradianceCubeTextureDesc.Width = 32;
			IrradianceCubeTextureDesc.Height = 32;
			V_RETURN(pD3dDevice->CreateTexture2D(&IrradianceCubeTextureDesc, nullptr, &g_pIrradianceCubeTexture));

			D3D11_TEXTURE2D_DESC PrefilterCubeTextureDesc;
			memcpy(&PrefilterCubeTextureDesc, &texDesc, sizeof(D3D11_TEXTURE2D_DESC));
			PrefilterCubeTextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			PrefilterCubeTextureDesc.Width = 128;
			PrefilterCubeTextureDesc.Height = 128;
			PrefilterCubeTextureDesc.MipLevels = 4;
			V_RETURN(pD3dDevice->CreateTexture2D(&PrefilterCubeTextureDesc, nullptr, &g_pPrefilterCubeTexture));

			//create our own depth stencil surface that'bindable as a shader
			V_RETURN(AMD::CreateDepthStencilSurface(&g_pDepthStencilTexture, &g_pDepthStencilSRV, &g_pDepthStencilView,
				DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_R24_UNORM_X8_TYPELESS, PrefilterCubeTextureDesc.Width, PrefilterCubeTextureDesc.Height, pBackBufferSurfaceDesc->SampleDesc.Count));


			D3D11_RENDER_TARGET_VIEW_DESC EnvRTVDesc;
			EnvRTVDesc.Format = texDesc.Format;
			EnvRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
			EnvRTVDesc.Texture2DArray.ArraySize = 1;
			EnvRTVDesc.Texture2D.MipSlice = 0;

			D3D11_RENDER_TARGET_VIEW_DESC IrradianceRTVDesc;
			ZeroMemory(&IrradianceRTVDesc, sizeof(D3D11_RENDER_TARGET_VIEW_DESC));
			IrradianceRTVDesc.Format = IrradianceCubeTextureDesc.Format;
			IrradianceRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;// D3D11_RTV_DIMENSION_TEXTURE2D);
			IrradianceRTVDesc.Texture2DArray.ArraySize = 1;
			IrradianceRTVDesc.Texture2D.MipSlice = 0;

			D3D11_RENDER_TARGET_VIEW_DESC PrefilterRTVDesc;
			ZeroMemory(&PrefilterRTVDesc, sizeof(D3D11_RENDER_TARGET_VIEW_DESC));
			PrefilterRTVDesc.Format = IrradianceCubeTextureDesc.Format;
			PrefilterRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;// D3D11_RTV_DIMENSION_TEXTURE2D);
			PrefilterRTVDesc.Texture2DArray.ArraySize = 1;
			PrefilterRTVDesc.Texture2D.MipSlice = 0;


			for (int i = 0; i < 6; ++i)
			{
				EnvRTVDesc.Texture2DArray.FirstArraySlice = i;
				V_RETURN(pD3dDevice->CreateRenderTargetView(g_pCubeTexture, &EnvRTVDesc, &g_pEnvCubeMapRTVs[i]));
				g_pEnvCubeMapRTVs[i]->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("CubeRT"), "CubeRT");

				IrradianceRTVDesc.Texture2DArray.FirstArraySlice = i;
				V_RETURN(pD3dDevice->CreateRenderTargetView(g_pIrradianceCubeTexture, &IrradianceRTVDesc, &g_pIrradianceCubeMapRTVs[i]));
				g_pIrradianceCubeMapRTVs[i]->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("IrranceRT"), "IrranceRT");


				PrefilterRTVDesc.Texture2DArray.FirstArraySlice = i;
				V_RETURN(pD3dDevice->CreateRenderTargetView(g_pPrefilterCubeTexture, &PrefilterRTVDesc, &g_pPrefilterCubeMapRTVs[i]));
				g_pPrefilterCubeMapRTVs[i]->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("PrefilterRT"), "PrefilterRT");

			}

			D3D11_SHADER_RESOURCE_VIEW_DESC CubeDesc;
			CubeDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			CubeDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
			CubeDesc.TextureCube.MipLevels = texDesc.MipLevels;
			CubeDesc.TextureCube.MostDetailedMip = 0;

			V_RETURN(pD3dDevice->CreateShaderResourceView(g_pIrradianceCubeTexture, &CubeDesc, &g_pIrradianceSRV));

			V_RETURN(pD3dDevice->CreateShaderResourceView(g_pPrefilterCubeTexture, &CubeDesc, &g_pPrefilterSRV));
		}

		V_RETURN(CreateDDSTextureFromFile(pD3dDevice, L"D:/SelfWorkSpace/directx11demo/ForwardPlus11/media/hdr/IceCubeMapDX.dds", &g_pIceCubemapTexture,&g_pEnvCubeMapSRV));

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

		UINT count = 1;
		D3D11_VIEWPORT PreViewport;
		pD3dImmediateContext->RSGetViewports(&count, &PreViewport);

		//开始渲染
		pD3dImmediateContext->RSSetViewports(1, &g_Viewport512);

		pD3dImmediateContext->OMSetDepthStencilState(m_pDisableDepthStencilState, 1);

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
		pD3dImmediateContext->PSSetSamplers(0, 1, &m_pSamplerLinear);




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



		{
			D3D11_MAPPED_SUBRESOURCE MappedResource;
			V(pD3dImmediateContext->Map(m_pConstantBufferPerFrame, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
			CB_PER_FRAME* pPerFrame = (CB_PER_FRAME*)MappedResource.pData;
			XMFLOAT2 temp;
			temp.x = g_Viewport512.Width;
			temp.y = g_Viewport512.Height;
			pPerFrame->vScreenSize = XMLoadFloat2(&temp);
			pD3dImmediateContext->Unmap(m_pConstantBufferPerFrame, 0);
		}





		pD3dImmediateContext->VSSetConstantBuffers(1, 1, &m_pConstantBufferPerFrame);
		pD3dImmediateContext->PSSetConstantBuffers(1, 1, &m_pConstantBufferPerFrame);

		for (int i = 0; i < 6; ++i)
		{

			////Get the projection & view matrix from the camera class
			//XMMATRIX mView = g_TempCubeMapCamera.GetViewMatrix();
			XMMATRIX mProj = g_TempCubeMapCamera.GetProjMatrix();
			XMMATRIX mWorldViewPrjection = mWorld*m_ViewMatrix[i] * mProj;

			//Set the constant buffers
			{
				D3D11_MAPPED_SUBRESOURCE MappedResource;
				V(pD3dImmediateContext->Map(m_pConstantBufferPerObject, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
				CB_PER_OBJECT* pPerObject = (CB_PER_OBJECT*)MappedResource.pData;
				pPerObject->mWorldViewProjection = XMMatrixTranspose(mWorldViewPrjection);
				pPerObject->mWorld = XMMatrixTranspose(mProj);

				pD3dImmediateContext->Unmap(m_pConstantBufferPerObject, 0);
			}

			pD3dImmediateContext->VSSetConstantBuffers(0, 1, &m_pConstantBufferPerObject);
			pD3dImmediateContext->PSSetConstantBuffers(0, 1, &m_pConstantBufferPerObject);

			//Bind cube map face as render target.
			pD3dImmediateContext->OMSetRenderTargets(1, &g_pEnvCubeMapRTVs[i], nullptr);

			//Clear cube map face and depth buffer.
			pD3dImmediateContext->ClearRenderTargetView(g_pEnvCubeMapRTVs[i], reinterpret_cast<const float*>(&Colors::Silver));
			pD3dImmediateContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

			//Draw the scene with the exception of the center sphere to this cube map face.
			pD3dImmediateContext->DrawIndexed(m_MeshData.Indices32.size(), 0, 0);

		}
		//Have harware generate lower mimap levels of cube map.
		//m_pD3dImmediateContext->GenerateMips(m_pDynamicCubeMapSRV);


		//还原状态
		pD3dImmediateContext->RSSetViewports(count, &PreViewport);
		pD3dImmediateContext->RSSetState(pPreRasterizerState);
		pD3dImmediateContext->OMSetBlendState(pPreBlendStateStored11, BlendFactorStored11, uSampleMaskStored11);
		pD3dImmediateContext->OMSetDepthStencilState(pPreDepthStencilStateStored11, uStencilRefStored11);

		SAFE_RELEASE(pPreRasterizerState);
		SAFE_RELEASE(pPreBlendStateStored11);
		SAFE_RELEASE(pPreDepthStencilStateStored11);
	}

	void CubeMapCaptureRender::RenderIrradiance(ID3D11Device * pD3dDevice, ID3D11DeviceContext * pD3dImmediateContext, const DXGI_SURFACE_DESC * pBackBufferDesc,
		CBaseCamera * pCamera)
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


		D3D11_VIEWPORT PreViewport; 
		UINT count = 1;
		pD3dImmediateContext->RSGetViewports(&count,&PreViewport);


		//开始渲染
		pD3dImmediateContext->RSSetViewports(1, &g_Viewport32);

		pD3dImmediateContext->OMSetDepthStencilState(m_pDisableDepthStencilState, 1);

		float BlendFactor[4] = { 0.0f,0.0f,0.0f,0.0f };
		pD3dImmediateContext->OMSetBlendState(m_pBlendState, BlendFactor, 0xFFFFFFFF);
		pD3dImmediateContext->RSSetState(m_pRasterizerState);



		pD3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pD3dImmediateContext->IASetIndexBuffer(m_pMeshIB, DXGI_FORMAT_R32_UINT, 0);
		pD3dImmediateContext->IASetInputLayout(m_pShaderInputLayout2);
		UINT uStride = sizeof(GeometryHelper::Vertex);
		UINT uOffset = 0;
		pD3dImmediateContext->IASetVertexBuffers(0, 1, &m_pMeshVB, &uStride, &uOffset);

		pD3dImmediateContext->VSSetShader(m_pShaderVS2, nullptr, 0);


		pD3dImmediateContext->PSSetShader(m_pShaderPS2, nullptr, 0);
		pD3dImmediateContext->PSSetShaderResources(0, 1, &g_pEnvCubeMapSRV);
		pD3dImmediateContext->PSSetSamplers(0, 1, &m_pSamplerLinear);

		XMMATRIX mWorld = XMMatrixIdentity();

		{
			D3D11_MAPPED_SUBRESOURCE MappedResource;
			V(pD3dImmediateContext->Map(m_pConstantBufferPerFrame, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
			CB_PER_FRAME* pPerFrame = (CB_PER_FRAME*)MappedResource.pData;
			XMFLOAT2 temp;
			temp.x = g_Viewport32.Width;
			temp.y = g_Viewport32.Height;
			pPerFrame->vScreenSize = XMLoadFloat2(&temp);
			pD3dImmediateContext->Unmap(m_pConstantBufferPerFrame, 0);
		}





		pD3dImmediateContext->VSSetConstantBuffers(1, 1, &m_pConstantBufferPerFrame);
		pD3dImmediateContext->PSSetConstantBuffers(1, 1, &m_pConstantBufferPerFrame);

		for (int i = 0; i < 6; ++i)
		{

			////Get the projection & view matrix from the camera class
			XMMATRIX mProj = g_TempCubeMapCamera.GetProjMatrix();
			XMMATRIX mWorldViewPrjection = mWorld*m_ViewMatrix[i] * mProj;

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
			pD3dImmediateContext->OMSetRenderTargets(1, &g_pIrradianceCubeMapRTVs[i], nullptr);

			//Clear cube map face and depth buffer.
			pD3dImmediateContext->ClearRenderTargetView(g_pIrradianceCubeMapRTVs[i], reinterpret_cast<const float*>(&Colors::Silver));
			pD3dImmediateContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

			//Draw the scene with the exception of the center sphere to this cube map face.
			pD3dImmediateContext->DrawIndexed(m_MeshData.Indices32.size(), 0, 0);

		}
		//Have harware generate lower mimap levels of cube map.
		pD3dImmediateContext->GenerateMips(g_pIrradianceSRV);


		//还原状态

		pD3dImmediateContext->RSSetViewports(count, &PreViewport);
		pD3dImmediateContext->RSSetState(pPreRasterizerState);
		pD3dImmediateContext->OMSetBlendState(pPreBlendStateStored11, BlendFactorStored11, uSampleMaskStored11);
		pD3dImmediateContext->OMSetDepthStencilState(pPreDepthStencilStateStored11, uStencilRefStored11);

		SAFE_RELEASE(pPreRasterizerState);
		SAFE_RELEASE(pPreBlendStateStored11);
		SAFE_RELEASE(pPreDepthStencilStateStored11);
	}

	void CubeMapCaptureRender::RenderPrefilter(ID3D11Device * pD3dDevice, ID3D11DeviceContext * pD3dImmediateContext, const DXGI_SURFACE_DESC * pBackBufferDesc, CBaseCamera * pCamera, ID3D11RenderTargetView * g_pTempTextureRenderTargetView, ID3D11DepthStencilView * g_pTempDepthStencilView)
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

		D3D11_VIEWPORT PreViewport;
		UINT count = 1;
		pD3dImmediateContext->RSGetViewports(&count, &PreViewport);

		//开始渲染
		pD3dImmediateContext->RSSetViewports(1, &g_Viewport128);

		pD3dImmediateContext->OMSetDepthStencilState(m_pDisableDepthStencilState, 1);

		float BlendFactor[4] = { 0.0f,0.0f,0.0f,0.0f };
		pD3dImmediateContext->OMSetBlendState(m_pBlendState, BlendFactor, 0xFFFFFFFF);
		pD3dImmediateContext->RSSetState(m_pRasterizerState);



		pD3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pD3dImmediateContext->IASetIndexBuffer(m_pMeshIB, DXGI_FORMAT_R32_UINT, 0);
		pD3dImmediateContext->IASetInputLayout(m_pShaderInputLayout3);
		UINT uStride = sizeof(GeometryHelper::Vertex);
		UINT uOffset = 0;
		pD3dImmediateContext->IASetVertexBuffers(0, 1, &m_pMeshVB, &uStride, &uOffset);

		pD3dImmediateContext->VSSetShader(m_pShaderVS3, nullptr, 0);


		pD3dImmediateContext->PSSetShader(m_pShaderPS3, nullptr, 0);
		pD3dImmediateContext->PSSetShaderResources(0, 1, &g_pEnvCubeMapSRV);
		pD3dImmediateContext->PSSetSamplers(0, 1, &m_pSamplerPoint);

		XMMATRIX mWorld = XMMatrixIdentity();

		{
			D3D11_MAPPED_SUBRESOURCE MappedResource;
			V(pD3dImmediateContext->Map(m_pConstantBufferPerFrame, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
			CB_PER_FRAME* pPerFrame = (CB_PER_FRAME*)MappedResource.pData;
			XMFLOAT3 temp;
			temp.x = g_Viewport128.Width;
			temp.y = g_Viewport128.Height;
			temp.z = 0.0f;
			pPerFrame->vScreenSize = XMLoadFloat3(&temp);
			pD3dImmediateContext->Unmap(m_pConstantBufferPerFrame, 0);
		}





		pD3dImmediateContext->VSSetConstantBuffers(1, 1, &m_pConstantBufferPerFrame);
		pD3dImmediateContext->PSSetConstantBuffers(1, 1, &m_pConstantBufferPerFrame);

		for (int i = 0; i < 6; ++i)
		{

			////Get the projection & view matrix from the camera class
			XMMATRIX mProj = g_TempCubeMapCamera.GetProjMatrix();
			XMMATRIX mWorldViewPrjection = mWorld*m_ViewMatrix[i] * mProj;

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
			pD3dImmediateContext->OMSetRenderTargets(1, &g_pPrefilterCubeMapRTVs[i], nullptr);

			//Clear cube map face and depth buffer.
			pD3dImmediateContext->ClearRenderTargetView(g_pPrefilterCubeMapRTVs[i], reinterpret_cast<const float*>(&Colors::Silver));
			pD3dImmediateContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

			//Draw the scene with the exception of the center sphere to this cube map face.
			pD3dImmediateContext->DrawIndexed(m_MeshData.Indices32.size(), 0, 0);

		}
		//Have harware generate lower mimap levels of cube map.
		//m_pD3dImmediateContext->GenerateMips(m_pDynamicCubeMapSRV);


		//还原状态

		pD3dImmediateContext->RSSetViewports(count, &PreViewport);
		pD3dImmediateContext->RSSetState(pPreRasterizerState);
		pD3dImmediateContext->OMSetBlendState(pPreBlendStateStored11, BlendFactorStored11, uSampleMaskStored11);
		pD3dImmediateContext->OMSetDepthStencilState(pPreDepthStencilStateStored11, uStencilRefStored11);

		SAFE_RELEASE(pPreRasterizerState);
		SAFE_RELEASE(pPreBlendStateStored11);
		SAFE_RELEASE(pPreDepthStencilStateStored11);
	}



	void CubeMapCaptureRender::AddShadersToCache(AMD::ShaderCache * pShaderCache, const wchar_t * pwsNameVS, const wchar_t * pwsNamePS, const wchar_t * pwsSourceFileName, const D3D11_INPUT_ELEMENT_DESC layout[], UINT size)
	{
		//if (!m_bShaderInited)
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

		//	m_bShaderInited = true;

		}

	}

	void CubeMapCaptureRender::AddShadersToCache2(AMD::ShaderCache * pShaderCache, const wchar_t * pwsNameVS, const wchar_t * pwsNamePS, const wchar_t * pwsSourceFileName, const D3D11_INPUT_ELEMENT_DESC layout[], UINT size)
	{
		//if (!m_bShaderInited)
		{
			SAFE_RELEASE(m_pShaderInputLayout2);
			SAFE_RELEASE(m_pShaderVS2);
			SAFE_RELEASE(m_pShaderPS2);

			//Add vertex shader
			pShaderCache->AddShader((ID3D11DeviceChild**)&m_pShaderVS2, AMD::ShaderCache::SHADER_TYPE::SHADER_TYPE_VERTEX,
				L"vs_5_0", pwsNameVS, pwsSourceFileName, 0, nullptr, &m_pShaderInputLayout2, (D3D11_INPUT_ELEMENT_DESC*)layout, size);

			//Add pixel shader

			pShaderCache->AddShader((ID3D11DeviceChild**)&m_pShaderPS2, AMD::ShaderCache::SHADER_TYPE::SHADER_TYPE_PIXEL,
				L"ps_5_0", pwsNamePS, pwsSourceFileName, 0, nullptr, nullptr, nullptr, 0);

		//	m_bShaderInited = true;

		}

	}

	void CubeMapCaptureRender::AddShadersToCache3(AMD::ShaderCache * pShaderCache, const wchar_t * pwsNameVS, const wchar_t * pwsNamePS, const wchar_t * pwsSourceFileName, const D3D11_INPUT_ELEMENT_DESC layout[], UINT size)
	{
		{
			SAFE_RELEASE(m_pShaderInputLayout3);
			SAFE_RELEASE(m_pShaderVS3);
			SAFE_RELEASE(m_pShaderPS3);

			//Add vertex shader
			pShaderCache->AddShader((ID3D11DeviceChild**)&m_pShaderVS3, AMD::ShaderCache::SHADER_TYPE::SHADER_TYPE_VERTEX,
				L"vs_5_0", pwsNameVS, pwsSourceFileName, 0, nullptr, &m_pShaderInputLayout3, (D3D11_INPUT_ELEMENT_DESC*)layout, size);

			//Add pixel shader

			pShaderCache->AddShader((ID3D11DeviceChild**)&m_pShaderPS3, AMD::ShaderCache::SHADER_TYPE::SHADER_TYPE_PIXEL,
				L"ps_5_0", pwsNamePS, pwsSourceFileName, 0, nullptr, nullptr, nullptr, 0);

		//	m_bShaderInited = true;

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

		{
			//Create DepthStencilState
			D3D11_DEPTH_STENCIL_DESC DepthStencilDesc;
			DepthStencilDesc.DepthEnable = FALSE;
			DepthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;
			DepthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
			DepthStencilDesc.StencilEnable = FALSE;
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
			V_RETURN(pD3dDevice->CreateDepthStencilState(&DepthStencilDesc, &m_pDisableDepthStencilState));
		}




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
		SAFE_RELEASE(m_pShaderInputLayout2);
		SAFE_RELEASE(m_pShaderVS2);
		SAFE_RELEASE(m_pShaderPS2);
		SAFE_RELEASE(m_pShaderInputLayout3);
		SAFE_RELEASE(m_pShaderVS3);
		SAFE_RELEASE(m_pShaderPS3);


		SAFE_RELEASE(m_pDepthStencilState);
		SAFE_RELEASE(m_pDisableDepthStencilState);
		SAFE_RELEASE(m_pRasterizerState);
		SAFE_RELEASE(m_pBlendState);
		SAFE_RELEASE(m_pSamplerLinear);
		SAFE_RELEASE(m_pSamplerPoint);
		

		SAFE_RELEASE(m_pConstantBufferPerObject);
		SAFE_RELEASE(m_pConstantBufferPerFrame);

		SAFE_RELEASE(m_pSrcTextureSRV);

		SAFE_RELEASE(g_pCubeTexture);
		SAFE_RELEASE(g_pEnvCubeMapSRV);
		for (int i = 0; i < 6; ++i)
		{
			SAFE_RELEASE(g_pEnvCubeMapRTVs[i]);
		}

		SAFE_RELEASE(g_pDepthStencilView);
		SAFE_RELEASE(g_pDepthStencilSRV);
		SAFE_RELEASE(g_pDepthStencilTexture);


		SAFE_RELEASE(g_pIceCubemapTexture);
		
		SAFE_RELEASE(g_pIrradianceCubeTexture);
		SAFE_RELEASE(g_pIrradianceSRV);
		for (int i = 0; i < 6; ++i)
		{
			SAFE_RELEASE(g_pIrradianceCubeMapRTVs[i]);
		}

		SAFE_RELEASE(g_pPrefilterCubeTexture);
		SAFE_RELEASE(g_pPrefilterSRV);
		for (int i = 0; i < 6; ++i)
		{
			SAFE_RELEASE(g_pPrefilterCubeMapRTVs[i]);
		}

	}

	HRESULT CubeMapCaptureRender::CreateSwapChainAssociatedResource(ID3D11Device * pD3dDevice, const DXGI_SURFACE_DESC * pBackBufferSurfaceDesc)
	{
		return E_NOTIMPL;
	}
}


//#ifndef ReleaseCOM
//#define ReleaseCOM(x) {if(x){x->Release();x = nullptr;}}
//#endif
//
//#ifndef SAFE_RELEASE
//#define SAFE_RELEASE(p)      { if (p) { (p)->Release(); (p) = nullptr; } }
//#endif
//
//#ifndef V_RETURN
//#define V_RETURN(x)    { hr = (x); if( FAILED(hr) ) { return DXUTTrace( __FILE__, (DWORD)__LINE__, hr, L#x, true ); } }
//#endif
//
//	HRESULT CreateTexture2DArraySRV(ID3D11Device * device, ID3D11DeviceContext * context, std::vector<std::wstring>& fileName, ID3D11ShaderResourceView **pID3D11ShaderResourceView)
//	{
//		//
//		// Load the texture elements individually from file.  These textures
//		// won't be used by the GPU (0 bind flags), they are just used to 
//		// load the image data from file.  We use the STAGING usage so the
//		// CPU can read the resource.
//		//
//		//jingz //todo
//
//		HRESULT hr;
//
//		UINT size = fileName.size();
//
//		std::vector<ID3D11Texture2D*> srcTex(size);
//		ID3D11ShaderResourceView* g_pDecalTextureSRV[6] = { nullptr,nullptr,nullptr,nullptr ,nullptr,nullptr };
//
//
//		for (UINT i = 0; i < size; ++i)
//		{
//			V_RETURN(CreateWICTextureFromFile(
//				device, fileName[i].c_str(),
//				(ID3D11Resource**)&srcTex[i], &g_pDecalTextureSRV[i]
//			));
//
//		}
//
//		D3D11_TEXTURE2D_DESC texElementDesc;
//		srcTex[0]->GetDesc(&texElementDesc);
//
//		D3D11_TEXTURE2D_DESC texArrayDesc;
//		texArrayDesc.Width = texElementDesc.Width;
//		texArrayDesc.Height = texElementDesc.Height;
//		texArrayDesc.MipLevels = texElementDesc.MipLevels;
//		texArrayDesc.ArraySize = size;
//		texArrayDesc.Format = texElementDesc.Format;
//		texArrayDesc.SampleDesc.Count = 1;
//		texArrayDesc.SampleDesc.Quality = 0;
//		texArrayDesc.Usage = D3D11_USAGE_DEFAULT;
//		texArrayDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
//		texArrayDesc.CPUAccessFlags = 0;
//		texArrayDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
//
//		ID3D11Texture2D * texArray = 0;
//		device->CreateTexture2D(&texArrayDesc, 0, &texArray);
//
//		//for each texture element
//		for (UINT texElementIndex = 0; texElementIndex < size; ++texElementIndex)
//		{
//			//for each mipmap level
//			for (UINT mipLevel = 0; mipLevel < texElementDesc.MipLevels; ++mipLevel)
//			{
//				D3D11_MAPPED_SUBRESOURCE mappedTex2D;
//				(context->Map(srcTex[texElementIndex], mipLevel, D3D11_MAP_READ, 0, &mappedTex2D));
//
//				context->UpdateSubresource(texArray,
//					D3D11CalcSubresource(mipLevel, texElementIndex, texElementDesc.MipLevels),
//					0, mappedTex2D.pData, mappedTex2D.RowPitch, mappedTex2D.DepthPitch
//				);
//				context->Unmap(srcTex[texElementIndex], mipLevel);
//			}
//		}
//
//		//
//		// Create a resource view to the texture array.
//		//
//		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc;
//		viewDesc.Format = texArrayDesc.Format;
//		viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
//		viewDesc.Texture2DArray.MostDetailedMip = 0;
//		viewDesc.Texture2DArray.MipLevels = texArrayDesc.MipLevels;
//		viewDesc.Texture2DArray.FirstArraySlice = 0;
//		viewDesc.Texture2DArray.ArraySize = size;
//
//		device->CreateShaderResourceView(texArray, &viewDesc, pID3D11ShaderResourceView);
//
//		//
//		// Cleanup -- we only need the resource view
//		//
//
//		ReleaseCOM(texArray);
//		for (UINT i = 0; i < size; ++i)
//		{
//			ReleaseCOM(srcTex[i]);
//		}
//
//		for (int i = 0; i < 6; ++i)
//		{
//			SAFE_RELEASE(g_pDecalTextureSRV[i]);
//		}
//
//		return hr;
//	}


