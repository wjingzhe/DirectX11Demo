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
		m_pRasterizerState(nullptr),m_pBlendState(nullptr),m_pSamplerLinearClamp(nullptr), m_pSamplerPoint(nullptr), m_pSamplerLinearWrap(nullptr),
		m_pConstantBufferPerObject(nullptr),m_pConstantBufferPerFrame(nullptr),
		m_uSizeConstantBufferPerObject(sizeof(CB_PER_OBJECT)),m_uSizeConstantBufferPerFrame(sizeof(CB_PER_FRAME)),
		m_pHdrTexture(nullptr),m_pHdrTextureSRV(nullptr),
		m_pSrcTextureSRV(nullptr),
		m_bShaderInited(false)
	{

#ifdef EXPORT_CUBEMAP

		IrradianceTextureArray32x32.resize(6);
		IrradianceRenderTarget32x32.resize(6);

		TextureArray8x8.resize(6);
		TextureArray16x16.resize(6);
		TextureArray32x32.resize(6);
		TextureArray64x64.resize(6);
		TextureArray128x128.resize(6);

		RenderTargetArray8x8.resize(6);
		RenderTargetArray16x16.resize(6);
		RenderTargetArray32x32.resize(6);
		RenderTargetArray64x64.resize(6);
		RenderTargetArray128x128.resize(6);


		for (int i = 0; i < 6; ++i)
		{
			IrradianceTextureArray32x32[i] = nullptr;
			IrradianceRenderTarget32x32[i] = nullptr;

			TextureArray8x8[i] = nullptr;
			TextureArray16x16[i] = nullptr;
			TextureArray32x32[i] = nullptr;
			TextureArray64x64[i] = nullptr;
			TextureArray128x128[i] = nullptr;

			RenderTargetArray8x8[i] = nullptr;
			RenderTargetArray16x16[i] = nullptr;
			RenderTargetArray32x32[i] = nullptr;
			RenderTargetArray64x64[i] = nullptr;
			RenderTargetArray128x128[i] = nullptr;

		}
#endif


		PrefilterRenderTarget8x8.resize(6);
		PrefilterRenderTarget16x16.resize(6);
		PrefilterRenderTarget32x32.resize(6);
		PrefilterRenderTarget64x64.resize(6);
		PrefilterRenderTarget128x128.resize(6);

		
		for (int i = 0;i<6;++i)
		{
			PrefilterRenderTarget8x8[i] = nullptr;
			PrefilterRenderTarget16x16[i] = nullptr;
			PrefilterRenderTarget32x32[i] = nullptr;
			PrefilterRenderTarget64x64[i] = nullptr;
			PrefilterRenderTarget128x128[i] = nullptr;

		}


		g_TempCubeMapCamera.SetProjParams(XM_PI / 2, 1.0f, 0.1f, 10.0f);
		
		
		g_Viewport8.Width = 8;
		g_Viewport8.Height = 8;
		g_Viewport8.MinDepth = 0;
		g_Viewport8.MaxDepth = 1;
		g_Viewport8.TopLeftX = 0;
		g_Viewport8.TopLeftY = 0;

		g_Viewport16.Width = 16;
		g_Viewport16.Height = 16;
		g_Viewport16.MinDepth = 0;
		g_Viewport16.MaxDepth = 1;
		g_Viewport16.TopLeftX = 0;
		g_Viewport16.TopLeftY = 0;

		g_Viewport32.Width = 32;
		g_Viewport32.Height = 32;
		g_Viewport32.MinDepth = 0;
		g_Viewport32.MaxDepth = 1;
		g_Viewport32.TopLeftX = 0;
		g_Viewport32.TopLeftY = 0;

		g_Viewport64.Width = 64;
		g_Viewport64.Height = 64;
		g_Viewport64.MinDepth = 0;
		g_Viewport64.MaxDepth = 1;
		g_Viewport64.TopLeftX = 0;
		g_Viewport64.TopLeftY = 0;

		g_Viewport128.Width = 128;
		g_Viewport128.Height = 128;
		g_Viewport128.MinDepth = 0;
		g_Viewport128.MaxDepth = 1;
		g_Viewport128.TopLeftX = 0;
		g_Viewport128.TopLeftY = 0;

		g_Viewport512.Width = 512;
		g_Viewport512.Height = 512;
		g_Viewport512.MinDepth = 0;
		g_Viewport512.MaxDepth = 1;
		g_Viewport512.TopLeftX = 0;
		g_Viewport512.TopLeftY = 0;


		//Use world up vector(0,1,0) for all direction
		m_UpVector[0] = XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f);
		m_UpVector[1] = XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f);
		m_UpVector[2] = XMVectorSet(0.0f, 0.0f, -1.0f, 1.0f);
		m_UpVector[3] = XMVectorSet(0.0f, 0.0f, 1.0f, 1.0f);
		m_UpVector[4] = XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f);
		m_UpVector[5] = XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f);


		XMVECTOR vEye = XMVectorSet(0.0, 0.0f, 0.0f, 1.0f);

		{
			XMVECTOR vLookAtPos = XMVectorSet(1.0f, 0.0f, 0.0f, 1.0f);
			g_TempCubeMapCamera.SetViewParams(vEye, vLookAtPos, m_UpVector[0]);
			m_ViewMatrix[0] = g_TempCubeMapCamera.GetViewMatrix();
		}

		{
			XMVECTOR vLookAtPos = XMVectorSet(-1.0f, 0.0f, 0.0f, 1.0f);
			g_TempCubeMapCamera.SetViewParams(vEye, vLookAtPos, m_UpVector[1]);
			m_ViewMatrix[1] = g_TempCubeMapCamera.GetViewMatrix();
		}
		{
			XMVECTOR vLookAtPos = XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f);
			g_TempCubeMapCamera.SetViewParams(vEye, vLookAtPos, m_UpVector[2]);
			m_ViewMatrix[2] = g_TempCubeMapCamera.GetViewMatrix();
		}

		{
			XMVECTOR vLookAtPos = XMVectorSet(0.0f, -1.0f, 0.0f, 1.0f);
			g_TempCubeMapCamera.SetViewParams(vEye, vLookAtPos, m_UpVector[3]);
			m_ViewMatrix[3] = g_TempCubeMapCamera.GetViewMatrix();
		}
		{
			XMVECTOR vLookAtPos = XMVectorSet(0.0f, 0.0f, 1.0f, 1.0f);
			g_TempCubeMapCamera.SetViewParams(vEye, vLookAtPos, m_UpVector[4]);
			m_ViewMatrix[4] = g_TempCubeMapCamera.GetViewMatrix();
		}
		{
			XMVECTOR vLookAtPos = XMVectorSet(0.0f, 0.0f, -1.0f, 1.0f);
			g_TempCubeMapCamera.SetViewParams(vEye, vLookAtPos, m_UpVector[5]);
			m_ViewMatrix[5] = g_TempCubeMapCamera.GetViewMatrix();
		}


		m_MeshData = GeometryHelper::CreateCubePlane(2, 2, 2);
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


			this->AddShadersToCache2(pShaderCache, L"IrradianceConvolutionVS", L"IrradianceConvolutionPS", L"irradiance_convolution.hlsl", layout, size);

			this->AddShadersToCache3(pShaderCache, L"CubeMapPrefilterVS", L"CubeMapPrefilterPS", L"prefilter.hlsl", layout, size);


			m_bShaderInited = true;
		}



	}


	HRESULT CubeMapCaptureRender::OnD3DDeviceCreated(ID3D11Device * pD3dDevice, const DXGI_SURFACE_DESC * pBackBufferSurfaceDesc, void * pUserContext)
	{
		HRESULT hr;

		V_RETURN(this->CreateCommonBuffers(pD3dDevice, m_MeshData, m_uSizeConstantBufferPerObject, m_uSizeConstantBufferPerFrame));
		V_RETURN(this->CreateOtherRenderStateResources(pD3dDevice));

		{// HDR CubeMap

		 //V_RETURN(D3DX11CreateShaderResourceViewFromFile(pD3dDevice, L"../../newport_loft.dds", nullptr, nullptr, &g_pHdrTextureSRV, nullptr));
		 //V_RETURN(CreateWICTextureFromFile(pD3dDevice, L"../../newport_loft.dds", (ID3D11Resource**)&g_pHdrTexture, &g_pHdrTextureSRV));

			stbi_set_flip_vertically_on_load(true);
			int width, height, nrComponents;
			float *data = stbi_loadf("../../ForwardPlus11/media/hdr/newport_loft.hdr", &width, &height, &nrComponents,4);

			D3D11_TEXTURE2D_DESC texDesc;

			texDesc.Width = width;
			texDesc.Height = height;
			texDesc.MipLevels = 1;
			texDesc.ArraySize = 1;
			texDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			texDesc.SampleDesc.Count = 1;
			texDesc.SampleDesc.Quality = 0;
			texDesc.Usage = D3D11_USAGE_DEFAULT;
			texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			texDesc.CPUAccessFlags = 0;
			texDesc.MiscFlags = 0;


			D3D11_SUBRESOURCE_DATA InitData;
			ZeroMemory(&InitData, sizeof(D3D11_SUBRESOURCE_DATA));
			InitData.SysMemPitch = width * 16;
			InitData.pSysMem = data;
			InitData.SysMemSlicePitch = 0;

			V_RETURN(pD3dDevice->CreateTexture2D(&texDesc, &InitData, &m_pHdrTexture));
			D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
			ZeroMemory(&SRVDesc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
			SRVDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			SRVDesc.Texture2D.MipLevels = 1;
			SRVDesc.Texture2D.MostDetailedMip = 0;
			V_RETURN(pD3dDevice->CreateShaderResourceView(m_pHdrTexture, &SRVDesc, &m_pHdrTextureSRV));

			ID3D11DeviceContext* pD3dDeviceContext = DXUTGetD3D11DeviceContext();
			pD3dDeviceContext->GenerateMips(m_pHdrTextureSRV);

		}

		//CubeMap RTV
		{
			D3D11_TEXTURE2D_DESC texDesc;
			texDesc.Width = CUBEMAP_SIZE;
			texDesc.Height = CUBEMAP_SIZE;
			texDesc.MipLevels = 8;
			texDesc.ArraySize = 6;//arraysize
			texDesc.SampleDesc.Count = 1;
			texDesc.SampleDesc.Quality = 0;
			texDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			texDesc.Usage = D3D11_USAGE_DEFAULT;
			texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			texDesc.CPUAccessFlags = 0;
			texDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS | D3D11_RESOURCE_MISC_TEXTURECUBE;//Texture Cube 

			V_RETURN(pD3dDevice->CreateTexture2D(&texDesc, nullptr, &g_pCubeTexture));

			D3D11_TEXTURE2D_DESC IrradianceCubeTextureDesc;
			memcpy(&IrradianceCubeTextureDesc, &texDesc, sizeof(D3D11_TEXTURE2D_DESC));
			IrradianceCubeTextureDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			IrradianceCubeTextureDesc.Width = 32;
			IrradianceCubeTextureDesc.Height = 32;
			IrradianceCubeTextureDesc.MipLevels = 1;
			V_RETURN(pD3dDevice->CreateTexture2D(&IrradianceCubeTextureDesc, nullptr, &g_pIrradianceCubeTexture));

#ifdef EXPORT_CUBEMAP
			{
				D3D11_TEXTURE2D_DESC IrradianceCubeTextureDesc;
				ZeroMemory(&IrradianceCubeTextureDesc, sizeof(D3D11_TEXTURE2D_DESC));
				IrradianceCubeTextureDesc.Width = 32;
				IrradianceCubeTextureDesc.Height = 32;
				IrradianceCubeTextureDesc.MipLevels = 1;
				IrradianceCubeTextureDesc.ArraySize = 1;
				IrradianceCubeTextureDesc.SampleDesc.Count = 4;
				IrradianceCubeTextureDesc.SampleDesc.Quality = 0;
				IrradianceCubeTextureDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
				IrradianceCubeTextureDesc.Usage = D3D11_USAGE_DEFAULT;
				IrradianceCubeTextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
				IrradianceCubeTextureDesc.CPUAccessFlags = 0;
				IrradianceCubeTextureDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

				V_RETURN(pD3dDevice->CreateTexture2D(&IrradianceCubeTextureDesc, nullptr, &IrradianceTextureArray32x32[0]));
				V_RETURN(pD3dDevice->CreateTexture2D(&IrradianceCubeTextureDesc, nullptr, &IrradianceTextureArray32x32[1]));
				V_RETURN(pD3dDevice->CreateTexture2D(&IrradianceCubeTextureDesc, nullptr, &IrradianceTextureArray32x32[2]));
				V_RETURN(pD3dDevice->CreateTexture2D(&IrradianceCubeTextureDesc, nullptr, &IrradianceTextureArray32x32[3]));
				V_RETURN(pD3dDevice->CreateTexture2D(&IrradianceCubeTextureDesc, nullptr, &IrradianceTextureArray32x32[4]));
				V_RETURN(pD3dDevice->CreateTexture2D(&IrradianceCubeTextureDesc, nullptr, &IrradianceTextureArray32x32[5]));

				V_RETURN(pD3dDevice->CreateRenderTargetView(IrradianceTextureArray32x32[0], nullptr, &IrradianceRenderTarget32x32[0]));
				V_RETURN(pD3dDevice->CreateRenderTargetView(IrradianceTextureArray32x32[1], nullptr, &IrradianceRenderTarget32x32[1]));
				V_RETURN(pD3dDevice->CreateRenderTargetView(IrradianceTextureArray32x32[2], nullptr, &IrradianceRenderTarget32x32[2]));
				V_RETURN(pD3dDevice->CreateRenderTargetView(IrradianceTextureArray32x32[3], nullptr, &IrradianceRenderTarget32x32[3]));
				V_RETURN(pD3dDevice->CreateRenderTargetView(IrradianceTextureArray32x32[4], nullptr, &IrradianceRenderTarget32x32[4]));
				V_RETURN(pD3dDevice->CreateRenderTargetView(IrradianceTextureArray32x32[5], nullptr, &IrradianceRenderTarget32x32[5]));

			}
			

#endif
			D3D11_TEXTURE2D_DESC PrefilterCubeTextureDesc;
			memcpy(&PrefilterCubeTextureDesc, &texDesc, sizeof(D3D11_TEXTURE2D_DESC));
			PrefilterCubeTextureDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			PrefilterCubeTextureDesc.Width = 128;
			PrefilterCubeTextureDesc.Height = 128;
			PrefilterCubeTextureDesc.MipLevels = 8;
			PrefilterCubeTextureDesc.Usage = D3D11_USAGE_DEFAULT;
			PrefilterCubeTextureDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
			V_RETURN(pD3dDevice->CreateTexture2D(&PrefilterCubeTextureDesc, nullptr, &g_pPrefilterCubeTexture));

#ifdef EXPORT_CUBEMAP
			{
				D3D11_TEXTURE2D_DESC temp;
				temp.Width = 16;
				temp.Height = 16;
				temp.MipLevels = 1;
				temp.ArraySize = 1;
				temp.SampleDesc.Count = 1;
				temp.SampleDesc.Quality = 0;
				temp.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
				temp.Usage = D3D11_USAGE_DEFAULT;
				temp.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
				temp.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
				temp.MiscFlags = 0;

				
				temp.Width = 8;
				temp.Height = 8;
				for (int i = 0; i < 6; ++i)
				{
					V_RETURN(pD3dDevice->CreateTexture2D(&temp, nullptr, &TextureArray8x8[i]));
					V_RETURN(pD3dDevice->CreateRenderTargetView(TextureArray8x8[i], nullptr, &RenderTargetArray8x8[i]));
				}


				temp.Width = 16;
				temp.Height = 16;
				for (int i = 0; i < 6; ++i)
				{
					V_RETURN(pD3dDevice->CreateTexture2D(&temp, nullptr, &TextureArray16x16[i]));
					V_RETURN(pD3dDevice->CreateRenderTargetView(TextureArray16x16[i], nullptr, &RenderTargetArray16x16[i]));
				}


				temp.Width = 32;
				temp.Height = 32;
				for (int i = 0; i < 6; ++i)
				{
					V_RETURN(pD3dDevice->CreateTexture2D(&temp, nullptr, &TextureArray32x32[i]));
					V_RETURN(pD3dDevice->CreateRenderTargetView(TextureArray32x32[i], nullptr, &RenderTargetArray32x32[i]));
				}


				temp.Width = 64;
				temp.Height = 64;
				for (int i = 0; i < 6; ++i)
				{
					V_RETURN(pD3dDevice->CreateTexture2D(&temp, nullptr, &TextureArray64x64[i]));
					V_RETURN(pD3dDevice->CreateRenderTargetView(TextureArray64x64[i], nullptr, &RenderTargetArray64x64[i]));
				}


				temp.Width = 128;
				temp.Height = 128;
				for (int i = 0; i < 6; ++i)
				{
					V_RETURN(pD3dDevice->CreateTexture2D(&temp, nullptr, &TextureArray128x128[i]));
					V_RETURN(pD3dDevice->CreateRenderTargetView(TextureArray128x128[i], nullptr, &RenderTargetArray128x128[i]));
				}

			}		 
#endif
			//8x8
			V_RETURN(AMD::CreateDepthStencilSurface(&g_pDepthStencilTexture8x8, &g_pDepthStencilSRV8x8, &g_pDepthStencilView8x8,
				DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_R24_UNORM_X8_TYPELESS, 8, 8, pBackBufferSurfaceDesc->SampleDesc.Count));
			//16x16
			V_RETURN(AMD::CreateDepthStencilSurface(&g_pDepthStencilTexture16x16, &g_pDepthStencilSRV16x16, &g_pDepthStencilView16x16,
				DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_R24_UNORM_X8_TYPELESS, 16, 16, pBackBufferSurfaceDesc->SampleDesc.Count));
			//32x32
			V_RETURN(AMD::CreateDepthStencilSurface(&g_pDepthStencilTexture32x32, &g_pDepthStencilSRV32x32, &g_pDepthStencilView32x32,
				DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_R24_UNORM_X8_TYPELESS, 32, 32, pBackBufferSurfaceDesc->SampleDesc.Count));
			//64x64
			V_RETURN(AMD::CreateDepthStencilSurface(&g_pDepthStencilTexture64x64, &g_pDepthStencilSRV64x64, &g_pDepthStencilView64x64,
				DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_R24_UNORM_X8_TYPELESS, 64, 64, pBackBufferSurfaceDesc->SampleDesc.Count));
			//128x128
			V_RETURN(AMD::CreateDepthStencilSurface(&g_pDepthStencilTexture128x128, &g_pDepthStencilSRV128x128, &g_pDepthStencilView128x128,
				DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_R24_UNORM_X8_TYPELESS, 128, 128, pBackBufferSurfaceDesc->SampleDesc.Count));

			//create our own depth stencil surface that'bindable as a shader
			V_RETURN(AMD::CreateDepthStencilSurface(&g_pDepthStencilTexture, &g_pDepthStencilSRV, &g_pDepthStencilView,
				DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_R24_UNORM_X8_TYPELESS, PrefilterCubeTextureDesc.Width, PrefilterCubeTextureDesc.Height, pBackBufferSurfaceDesc->SampleDesc.Count));


			D3D11_RENDER_TARGET_VIEW_DESC EnvRTVDesc;
			EnvRTVDesc.Format = texDesc.Format;
			EnvRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
			EnvRTVDesc.Texture2DArray.ArraySize = 1;
			EnvRTVDesc.Texture2D.MipSlice = 0;// D3D11_RESOURCE_MISC_GENERATE_MIPS;

			D3D11_RENDER_TARGET_VIEW_DESC IrradianceRTVDesc;
			ZeroMemory(&IrradianceRTVDesc, sizeof(D3D11_RENDER_TARGET_VIEW_DESC));
			IrradianceRTVDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			IrradianceRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;// D3D11_RTV_DIMENSION_TEXTURE2D);
			IrradianceRTVDesc.Texture2DArray.ArraySize = 1;
			IrradianceRTVDesc.Texture2D.MipSlice = 0;

			D3D11_RENDER_TARGET_VIEW_DESC PrefilterRTVDesc;
			ZeroMemory(&PrefilterRTVDesc, sizeof(D3D11_RENDER_TARGET_VIEW_DESC));
			PrefilterRTVDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			PrefilterRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;// D3D11_RTV_DIMENSION_TEXTURE2D);
			PrefilterRTVDesc.Texture2DArray.ArraySize = 1;
			PrefilterRTVDesc.Texture2D.MipSlice = 0;// D3D11_RESOURCE_MISC_GENERATE_MIPS;;


			for (int i = 0; i < 6; ++i)
			{
				EnvRTVDesc.Texture2DArray.FirstArraySlice = i;
				V_RETURN(pD3dDevice->CreateRenderTargetView(g_pCubeTexture, &EnvRTVDesc, &g_pEnvCubeMapRTVs[i]));
				g_pEnvCubeMapRTVs[i]->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("CubeRT"), "CubeRT");

				IrradianceRTVDesc.Texture2DArray.FirstArraySlice = i;
				V_RETURN(pD3dDevice->CreateRenderTargetView(g_pIrradianceCubeTexture, &IrradianceRTVDesc, &g_pIrradianceCubeMapRTVs[i]));
				g_pIrradianceCubeMapRTVs[i]->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("IrranceRT"), "IrranceRT");

				//128*128
				PrefilterRTVDesc.Texture2D.MipSlice = 0;
				PrefilterRTVDesc.Texture2DArray.FirstArraySlice = i;
				V_RETURN(pD3dDevice->CreateRenderTargetView(g_pPrefilterCubeTexture, &PrefilterRTVDesc, &PrefilterRenderTarget128x128[i]));
				PrefilterRenderTarget128x128[i]->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("PrefilterRT128"), "PrefilterRT128");


				//64*64
				PrefilterRTVDesc.Texture2D.MipSlice = 1;
				PrefilterRTVDesc.Texture2DArray.FirstArraySlice = i;
				V_RETURN(pD3dDevice->CreateRenderTargetView(g_pPrefilterCubeTexture, &PrefilterRTVDesc, &PrefilterRenderTarget64x64[i]));
				PrefilterRenderTarget64x64[i]->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("PrefilterRT64"), "PrefilterRT64");

				//32*32
				PrefilterRTVDesc.Texture2D.MipSlice = 2;
				PrefilterRTVDesc.Texture2DArray.FirstArraySlice = i;
				V_RETURN(pD3dDevice->CreateRenderTargetView(g_pPrefilterCubeTexture, &PrefilterRTVDesc, &PrefilterRenderTarget32x32[i]));
				PrefilterRenderTarget32x32[i]->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("PrefilterRT32"), "PrefilterRT32");

				//16*16
				PrefilterRTVDesc.Texture2D.MipSlice = 3;
				PrefilterRTVDesc.Texture2DArray.FirstArraySlice = i;
				V_RETURN(pD3dDevice->CreateRenderTargetView(g_pPrefilterCubeTexture, &PrefilterRTVDesc, &PrefilterRenderTarget16x16[i]));
				PrefilterRenderTarget16x16[i]->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("PrefilterRT16"), "PrefilterRT16");

				//8*8
				PrefilterRTVDesc.Texture2D.MipSlice = 4;
				PrefilterRTVDesc.Texture2DArray.FirstArraySlice = i;
				V_RETURN(pD3dDevice->CreateRenderTargetView(g_pPrefilterCubeTexture, &PrefilterRTVDesc, &PrefilterRenderTarget8x8[i]));
				PrefilterRenderTarget8x8[i]->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("PrefilterRT8"), "PrefilterRT8");
			}


			D3D11_SHADER_RESOURCE_VIEW_DESC CubeDesc;
			CubeDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			CubeDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
			CubeDesc.TextureCube.MipLevels = texDesc.MipLevels;
			CubeDesc.TextureCube.MostDetailedMip = 0;

			V_RETURN(pD3dDevice->CreateShaderResourceView(g_pCubeTexture, &CubeDesc, &g_pEnvCubeMapSRV));

			{
				D3D11_SHADER_RESOURCE_VIEW_DESC CubeDesc;
				CubeDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
				CubeDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
				CubeDesc.TextureCube.MipLevels = 1;
				CubeDesc.TextureCube.MostDetailedMip = 0;

				V_RETURN(pD3dDevice->CreateShaderResourceView(g_pIrradianceCubeTexture, &CubeDesc, &g_pIrradianceSRV));
			}


			{
				D3D11_SHADER_RESOURCE_VIEW_DESC CubeDesc;
				CubeDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
				CubeDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
				CubeDesc.TextureCube.MipLevels = 8;
				CubeDesc.TextureCube.MostDetailedMip = 0;
				V_RETURN(pD3dDevice->CreateShaderResourceView(g_pPrefilterCubeTexture, &CubeDesc, &g_pPrefilterSRV));
			}



		}

		{

			CreateDDSTextureFromFileEx(pD3dDevice, L"../media/hdr/IceSkyCubeMap.dds",0,
				D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,0, D3D11_RESOURCE_MISC_TEXTURECUBE,false,
				&g_pIceCubemapTexture, &g_pIceEnvCubeMapSRV);

			//V_RETURN();
			
			D3D11_SHADER_RESOURCE_VIEW_DESC CubeDesc;
			g_pIceEnvCubeMapSRV->GetDesc(&CubeDesc);

			D3D11_RENDER_TARGET_VIEW_DESC EnvRTVDesc;
			EnvRTVDesc.Format = CubeDesc.Format;
			EnvRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
			EnvRTVDesc.Texture2DArray.ArraySize = 1;
			EnvRTVDesc.Texture2D.MipSlice = 0;

			for (int i = 0; i < 6; ++i)
			{
				EnvRTVDesc.Texture2DArray.FirstArraySlice = i;
				V_RETURN(pD3dDevice->CreateRenderTargetView(g_pIceCubemapTexture, &EnvRTVDesc, &g_pIceCubemapRTVs[i]));
				g_pIceCubemapRTVs[i]->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("IceCubeRT"), "IceCubeRT");
			}
		}
		


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
		pD3dImmediateContext->PSSetShaderResources(0, 1, &m_pHdrTextureSRV);
		pD3dImmediateContext->PSSetSamplers(0, 1, &m_pSamplerLinearClamp);




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
			pD3dImmediateContext->ClearRenderTargetView(g_pEnvCubeMapRTVs[i], reinterpret_cast<const float*>(&Colors::Black));
			//pD3dImmediateContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

			//Draw the scene with the exception of the center sphere to this cube map face.
			pD3dImmediateContext->DrawIndexed(m_MeshData.Indices32.size(), 0, 0);

		}
		pD3dImmediateContext->GenerateMips(g_pEnvCubeMapSRV);


		//还原状态
		pD3dImmediateContext->RSSetViewports(count, &PreViewport);
		pD3dImmediateContext->RSSetState(pPreRasterizerState);
		pD3dImmediateContext->OMSetBlendState(pPreBlendStateStored11, BlendFactorStored11, uSampleMaskStored11);
		pD3dImmediateContext->OMSetDepthStencilState(pPreDepthStencilStateStored11, uStencilRefStored11);
		pD3dImmediateContext->OMSetRenderTargets(1, &g_pTempTextureRenderTargetView, g_pTempDepthStencilView);

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
		pD3dImmediateContext->OMSetRenderTargets(0, nullptr, nullptr);
		pD3dImmediateContext->PSSetShaderResources(0, 1, &g_pEnvCubeMapSRV);
		pD3dImmediateContext->PSSetSamplers(0, 1, &m_pSamplerLinearClamp);

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
				pPerObject->vUp = m_UpVector[i];
				pD3dImmediateContext->Unmap(m_pConstantBufferPerObject, 0);
			}

			pD3dImmediateContext->VSSetConstantBuffers(0, 1, &m_pConstantBufferPerObject);
			pD3dImmediateContext->PSSetConstantBuffers(0, 1, &m_pConstantBufferPerObject);

			//Bind cube map face as render target.
			pD3dImmediateContext->OMSetRenderTargets(1, &g_pIrradianceCubeMapRTVs[i], nullptr);

			//Clear cube map face and depth buffer.
			pD3dImmediateContext->ClearRenderTargetView(g_pIrradianceCubeMapRTVs[i], reinterpret_cast<const float*>(&Colors::Black));
			pD3dImmediateContext->ClearDepthStencilView(g_pDepthStencilView32x32, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

			//Draw the scene with the exception of the center sphere to this cube map face.
			pD3dImmediateContext->DrawIndexed(m_MeshData.Indices32.size(), 0, 0);

#ifdef EXPORT_CUBEMAP
			//Bind cube map face as render target.
			pD3dImmediateContext->OMSetRenderTargets(1, &IrradianceRenderTarget32x32[i], nullptr);

			//Clear cube map face and depth buffer.
			pD3dImmediateContext->ClearRenderTargetView(IrradianceRenderTarget32x32[i], reinterpret_cast<const float*>(&Colors::Black));
			pD3dImmediateContext->ClearDepthStencilView(g_pDepthStencilView32x32, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

			//Draw the scene with the exception of the center sphere to this cube map face.
			pD3dImmediateContext->DrawIndexed(m_MeshData.Indices32.size(), 0, 0);
#endif

		}
		//Have harware generate lower mimap levels of cube map.
		pD3dImmediateContext->GenerateMips(g_pIrradianceSRV);


		//还原状态

		pD3dImmediateContext->RSSetViewports(count, &PreViewport);
		pD3dImmediateContext->RSSetState(pPreRasterizerState);
		pD3dImmediateContext->OMSetBlendState(pPreBlendStateStored11, BlendFactorStored11, uSampleMaskStored11);
		pD3dImmediateContext->OMSetDepthStencilState(pPreDepthStencilStateStored11, uStencilRefStored11);
		pD3dImmediateContext->OMSetRenderTargets(0, nullptr, nullptr);

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
		pD3dImmediateContext->OMSetRenderTargets(0, nullptr, nullptr);
		pD3dImmediateContext->PSSetShaderResources(0, 1, &g_pEnvCubeMapSRV);
		pD3dImmediateContext->PSSetSamplers(0, 1, &m_pSamplerLinearClamp);

		XMMATRIX mWorld = XMMatrixIdentity();

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


			//128x128
			{
				{
					D3D11_MAPPED_SUBRESOURCE MappedResource;
					V(pD3dImmediateContext->Map(m_pConstantBufferPerFrame, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
					CB_PER_FRAME* pPerFrame = (CB_PER_FRAME*)MappedResource.pData;
					XMFLOAT3 temp;
					temp.x = g_Viewport128.Width;
					temp.y = g_Viewport128.Height;
					temp.z = 0.0f / 4.0f;
					pPerFrame->vScreenSize = XMLoadFloat3(&temp);
					pD3dImmediateContext->Unmap(m_pConstantBufferPerFrame, 0);
				}

				pD3dImmediateContext->RSSetViewports(1, &g_Viewport128);
				//Bind cube map face as render target.
				pD3dImmediateContext->OMSetRenderTargets(1, &PrefilterRenderTarget128x128[i], nullptr);

				//Clear cube map face and depth buffer.
				pD3dImmediateContext->ClearRenderTargetView(PrefilterRenderTarget128x128[i], reinterpret_cast<const float*>(&Colors::Black));
				pD3dImmediateContext->ClearDepthStencilView(g_pDepthStencilView128x128, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

				//Draw the scene with the exception of the center sphere to this cube map face.
				pD3dImmediateContext->DrawIndexed(m_MeshData.Indices32.size(), 0, 0);

#ifdef EXPORT_CUBEMAP
				//Bind cube map face as render target.
				pD3dImmediateContext->OMSetRenderTargets(1, &RenderTargetArray128x128[i], nullptr);

				//Clear cube map face and depth buffer.
				pD3dImmediateContext->ClearRenderTargetView(RenderTargetArray128x128[i], reinterpret_cast<const float*>(&Colors::Black));
				pD3dImmediateContext->ClearDepthStencilView(g_pDepthStencilView128x128, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

				//Draw the scene with the exception of the center sphere to this cube map face.
				pD3dImmediateContext->DrawIndexed(m_MeshData.Indices32.size(), 0, 0);
#endif
			}

			//64x64
			{
				{
					D3D11_MAPPED_SUBRESOURCE MappedResource;
					V(pD3dImmediateContext->Map(m_pConstantBufferPerFrame, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
					CB_PER_FRAME* pPerFrame = (CB_PER_FRAME*)MappedResource.pData;
					XMFLOAT3 temp;
					temp.x = g_Viewport64.Width;
					temp.y = g_Viewport64.Height;
					temp.z = 1.0f / 4.0f;
					pPerFrame->vScreenSize = XMLoadFloat3(&temp);
					pD3dImmediateContext->Unmap(m_pConstantBufferPerFrame, 0);
				}

				pD3dImmediateContext->RSSetViewports(1, &g_Viewport64);
				//Bind cube map face as render target.
				pD3dImmediateContext->OMSetRenderTargets(1, &PrefilterRenderTarget64x64[i], nullptr);

				//Clear cube map face and depth buffer.
				pD3dImmediateContext->ClearRenderTargetView(PrefilterRenderTarget64x64[i], reinterpret_cast<const float*>(&Colors::Black));
				pD3dImmediateContext->ClearDepthStencilView(g_pDepthStencilView64x64, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

				//Draw the scene with the exception of the center sphere to this cube map face.
				pD3dImmediateContext->DrawIndexed(m_MeshData.Indices32.size(), 0, 0);

#ifdef EXPORT_CUBEMAP
				//Bind cube map face as render target.
				pD3dImmediateContext->OMSetRenderTargets(1, &RenderTargetArray64x64[i], nullptr);

				//Clear cube map face and depth buffer.
				pD3dImmediateContext->ClearRenderTargetView(RenderTargetArray64x64[i], reinterpret_cast<const float*>(&Colors::Black));
				pD3dImmediateContext->ClearDepthStencilView(g_pDepthStencilView64x64, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

				//Draw the scene with the exception of the center sphere to this cube map face.
				pD3dImmediateContext->DrawIndexed(m_MeshData.Indices32.size(), 0, 0);
#endif
			}

			//32x32
			{
				{
					D3D11_MAPPED_SUBRESOURCE MappedResource;
					V(pD3dImmediateContext->Map(m_pConstantBufferPerFrame, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
					CB_PER_FRAME* pPerFrame = (CB_PER_FRAME*)MappedResource.pData;
					XMFLOAT3 temp;
					temp.x = g_Viewport32.Width;
					temp.y = g_Viewport32.Height;
					temp.z = 2.0f / 4.0f;
					pPerFrame->vScreenSize = XMLoadFloat3(&temp);
					pD3dImmediateContext->Unmap(m_pConstantBufferPerFrame, 0);
				}

				pD3dImmediateContext->RSSetViewports(1, &g_Viewport32);
				//Bind cube map face as render target.
				pD3dImmediateContext->OMSetRenderTargets(1, &PrefilterRenderTarget32x32[i], nullptr);

				//Clear cube map face and depth buffer.
				pD3dImmediateContext->ClearRenderTargetView(PrefilterRenderTarget32x32[i], reinterpret_cast<const float*>(&Colors::Black));
				pD3dImmediateContext->ClearDepthStencilView(g_pDepthStencilView32x32, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

				//Draw the scene with the exception of the center sphere to this cube map face.
				pD3dImmediateContext->DrawIndexed(m_MeshData.Indices32.size(), 0, 0);


#ifdef EXPORT_CUBEMAP
				//Bind cube map face as render target.
				pD3dImmediateContext->OMSetRenderTargets(1, &RenderTargetArray32x32[i], nullptr);

				//Clear cube map face and depth buffer.
				pD3dImmediateContext->ClearRenderTargetView(RenderTargetArray32x32[i], reinterpret_cast<const float*>(&Colors::Black));
				pD3dImmediateContext->ClearDepthStencilView(g_pDepthStencilView32x32, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

				//Draw the scene with the exception of the center sphere to this cube map face.
				pD3dImmediateContext->DrawIndexed(m_MeshData.Indices32.size(), 0, 0);
#endif


			}

			//16x16
			{

				{
					D3D11_MAPPED_SUBRESOURCE MappedResource;
					V(pD3dImmediateContext->Map(m_pConstantBufferPerFrame, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
					CB_PER_FRAME* pPerFrame = (CB_PER_FRAME*)MappedResource.pData;
					XMFLOAT3 temp;
					temp.x = g_Viewport16.Width;
					temp.y = g_Viewport16.Height;
					temp.z = 3.0f / 4.0f;
					pPerFrame->vScreenSize = XMLoadFloat3(&temp);
					pD3dImmediateContext->Unmap(m_pConstantBufferPerFrame, 0);
				}


				pD3dImmediateContext->VSSetConstantBuffers(1, 1, &m_pConstantBufferPerFrame);
				pD3dImmediateContext->PSSetConstantBuffers(1, 1, &m_pConstantBufferPerFrame);


				pD3dImmediateContext->RSSetViewports(1, &g_Viewport16);
				//Bind cube map face as render target.
				pD3dImmediateContext->OMSetRenderTargets(1, &PrefilterRenderTarget16x16[i], nullptr);

				//Clear cube map face and depth buffer.
				pD3dImmediateContext->ClearRenderTargetView(PrefilterRenderTarget16x16[i], reinterpret_cast<const float*>(&Colors::Black));
				pD3dImmediateContext->ClearDepthStencilView(g_pDepthStencilView16x16, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

				//Draw the scene with the exception of the center sphere to this cube map face.
				pD3dImmediateContext->DrawIndexed(m_MeshData.Indices32.size(), 0, 0);

#ifdef EXPORT_CUBEMAP
				//Bind cube map face as render target.
				pD3dImmediateContext->OMSetRenderTargets(1, &RenderTargetArray16x16[i], nullptr);

				//Clear cube map face and depth buffer.
				pD3dImmediateContext->ClearRenderTargetView(RenderTargetArray16x16[i], reinterpret_cast<const float*>(&Colors::Black));
				pD3dImmediateContext->ClearDepthStencilView(g_pDepthStencilView16x16, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

				//Draw the scene with the exception of the center sphere to this cube map face.
				pD3dImmediateContext->DrawIndexed(m_MeshData.Indices32.size(), 0, 0);
#endif

			}

			//8x8
			{

				{
					D3D11_MAPPED_SUBRESOURCE MappedResource;
					V(pD3dImmediateContext->Map(m_pConstantBufferPerFrame, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
					CB_PER_FRAME* pPerFrame = (CB_PER_FRAME*)MappedResource.pData;
					XMFLOAT3 temp;
					temp.x = g_Viewport8.Width;
					temp.y = g_Viewport8.Height;
					temp.z = 4.0f / 4.0f;
					pPerFrame->vScreenSize = XMLoadFloat3(&temp);
					pD3dImmediateContext->Unmap(m_pConstantBufferPerFrame, 0);
				}


				pD3dImmediateContext->VSSetConstantBuffers(1, 1, &m_pConstantBufferPerFrame);
				pD3dImmediateContext->PSSetConstantBuffers(1, 1, &m_pConstantBufferPerFrame);


				pD3dImmediateContext->RSSetViewports(1, &g_Viewport8);
				//Bind cube map face as render target.
				pD3dImmediateContext->OMSetRenderTargets(1, &PrefilterRenderTarget8x8[i], nullptr);

				//Clear cube map face and depth buffer.
				pD3dImmediateContext->ClearRenderTargetView(PrefilterRenderTarget8x8[i], reinterpret_cast<const float*>(&Colors::Black));
				pD3dImmediateContext->ClearDepthStencilView(g_pDepthStencilView8x8, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

				//Draw the scene with the exception of the center sphere to this cube map face.
				pD3dImmediateContext->DrawIndexed(m_MeshData.Indices32.size(), 0, 0);

#ifdef EXPORT_CUBEMAP
				//Bind cube map face as render target.
				pD3dImmediateContext->OMSetRenderTargets(1, &RenderTargetArray8x8[i], nullptr);

				//Clear cube map face and depth buffer.
				pD3dImmediateContext->ClearRenderTargetView(RenderTargetArray8x8[i], reinterpret_cast<const float*>(&Colors::Black));
				pD3dImmediateContext->ClearDepthStencilView(g_pDepthStencilView8x8, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

				//Draw the scene with the exception of the center sphere to this cube map face.
				pD3dImmediateContext->DrawIndexed(m_MeshData.Indices32.size(), 0, 0);
#endif
			}


		}

		//pD3dImmediateContext->Flush();

		


		//还原状态

		pD3dImmediateContext->RSSetViewports(count, &PreViewport);
		pD3dImmediateContext->RSSetState(pPreRasterizerState);
		pD3dImmediateContext->OMSetBlendState(pPreBlendStateStored11, BlendFactorStored11, uSampleMaskStored11);
		pD3dImmediateContext->OMSetDepthStencilState(pPreDepthStencilStateStored11, uStencilRefStored11);
		pD3dImmediateContext->OMSetRenderTargets(0, nullptr, nullptr);

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
		V_RETURN(pD3dDevice->CreateSamplerState(&SamplerDesc, &m_pSamplerLinearClamp));
		m_pSamplerLinearClamp->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("LINEAR"), "LINEAR");



		SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
		V_RETURN(pD3dDevice->CreateSamplerState(&SamplerDesc, &m_pSamplerPoint));
		m_pSamplerPoint->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("Point"), "Point");

		{
			D3D11_SAMPLER_DESC SamplerDesc;
			ZeroMemory(&SamplerDesc, sizeof(SamplerDesc));
			SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
			SamplerDesc.AddressU = SamplerDesc.AddressV = SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
			SamplerDesc.MaxAnisotropy = 16;
			SamplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
			SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
			V_RETURN(pD3dDevice->CreateSamplerState(&SamplerDesc, &m_pSamplerLinearWrap));
			m_pSamplerLinearWrap->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("LINEAR WRAP"), "LINEAR WRAP");
		}

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
		SAFE_RELEASE(m_pSamplerLinearClamp);
		SAFE_RELEASE(m_pSamplerPoint);
		SAFE_RELEASE(m_pSamplerLinearWrap);
		

		SAFE_RELEASE(m_pConstantBufferPerObject);
		SAFE_RELEASE(m_pConstantBufferPerFrame);

		SAFE_RELEASE(m_pHdrTexture);
		SAFE_RELEASE(m_pHdrTextureSRV);
		

		SAFE_RELEASE(m_pSrcTextureSRV);




		SAFE_RELEASE(g_pCubeTexture);
		SAFE_RELEASE(g_pEnvCubeMapSRV);
		for (int i = 0; i < 6; ++i)
		{
			SAFE_RELEASE(g_pEnvCubeMapRTVs[i]);
		}

		SAFE_RELEASE(g_pDepthStencilView8x8);
		SAFE_RELEASE(g_pDepthStencilSRV8x8);
		SAFE_RELEASE(g_pDepthStencilTexture8x8);

		SAFE_RELEASE(g_pDepthStencilView16x16);
		SAFE_RELEASE(g_pDepthStencilSRV16x16);
		SAFE_RELEASE(g_pDepthStencilTexture16x16);

		SAFE_RELEASE(g_pDepthStencilView32x32);
		SAFE_RELEASE(g_pDepthStencilSRV32x32);
		SAFE_RELEASE(g_pDepthStencilTexture32x32);

		SAFE_RELEASE(g_pDepthStencilView64x64);
		SAFE_RELEASE(g_pDepthStencilSRV64x64);
		SAFE_RELEASE(g_pDepthStencilTexture64x64);

		SAFE_RELEASE(g_pDepthStencilView128x128);
		SAFE_RELEASE(g_pDepthStencilSRV128x128);
		SAFE_RELEASE(g_pDepthStencilTexture128x128);

		SAFE_RELEASE(g_pDepthStencilView);
		SAFE_RELEASE(g_pDepthStencilSRV);
		SAFE_RELEASE(g_pDepthStencilTexture);


		SAFE_RELEASE(g_pIceCubemapTexture);
		SAFE_RELEASE(g_pIceEnvCubeMapSRV);
		for (int i = 0; i < 6; ++i)
		{
			SAFE_RELEASE(g_pIceCubemapRTVs[i]);
		}
		
		
		SAFE_RELEASE(g_pIrradianceCubeTexture);
		SAFE_RELEASE(g_pIrradianceSRV);
		for (int i = 0; i < 6; ++i)
		{
			SAFE_RELEASE(g_pIrradianceCubeMapRTVs[i]);
		}

		SAFE_RELEASE(g_pPrefilterCubeTexture);
		for (int i = 0; i < 6; ++i)
		{
			SAFE_RELEASE(PrefilterRenderTarget8x8[i]);
			SAFE_RELEASE(PrefilterRenderTarget16x16[i]);
			SAFE_RELEASE(PrefilterRenderTarget32x32[i]);
			SAFE_RELEASE(PrefilterRenderTarget64x64[i]);
			SAFE_RELEASE(PrefilterRenderTarget128x128[i]);
		}
		SAFE_RELEASE(g_pPrefilterSRV);

#ifdef EXPORT_CUBEMAP
		for (int i = 0; i < 6; ++i)
		{
			SAFE_RELEASE(IrradianceTextureArray32x32[i]);
			SAFE_RELEASE(IrradianceRenderTarget32x32[i]);
			

			SAFE_RELEASE(TextureArray8x8[i]);
			SAFE_RELEASE(TextureArray16x16[i]);
			SAFE_RELEASE(TextureArray32x32[i]);
			SAFE_RELEASE(TextureArray64x64[i]);
			SAFE_RELEASE(TextureArray128x128[i]);

			SAFE_RELEASE(RenderTargetArray8x8[i]);
			SAFE_RELEASE(RenderTargetArray16x16[i]);
			SAFE_RELEASE(RenderTargetArray32x32[i]);
			SAFE_RELEASE(RenderTargetArray64x64[i]);
			SAFE_RELEASE(RenderTargetArray128x128[i]);
		}
#endif
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


