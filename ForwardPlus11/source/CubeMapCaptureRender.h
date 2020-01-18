#pragma once

#include "GeometryHelper.h"
#include <d3d11.h>
#include "../../DXUT/Core/DXUT.h"
#include "../../amd_sdk/inc/AMD_SDK.h"
#include "../../DXUT/Optional/DXUTCamera.h"

//#define EXPORT_CUBEMAP

namespace ForwardRender
{

	class CubeMapCaptureRender
	{
	public:
		CubeMapCaptureRender();
		~CubeMapCaptureRender();


		struct CB_PER_OBJECT
		{
			DirectX::XMMATRIX mWorldViewProjection;
			DirectX::XMMATRIX mWorld;
			DirectX::XMVECTOR vUp;
		};

		struct CB_PER_FRAME
		{
			DirectX::XMVECTOR vCameraPos3AndAlphaTest;
			DirectX::XMVECTOR vScreenSize;
		};


	public:

		GeometryHelper::MeshData m_MeshData;

		virtual void AddShadersToCache(AMD::ShaderCache* pShaderCache);

		HRESULT  OnD3DDeviceCreated(ID3D11Device* pD3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
			void* pUserContext);

		void OnD3D11DestroyDevice(void * pUserContext);

		//when some sln or window size changed
		void OnReleasingSwapChain(void);
		void OnResizedSwapChain(ID3D11Device* pD3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc);

		virtual void OnRender(ID3D11Device* pD3dDevice, ID3D11DeviceContext * pD3dImmediateContext, const DXGI_SURFACE_DESC* pBackBufferDesc,
			CBaseCamera* pCamera, ID3D11RenderTargetView* pRTV, ID3D11DepthStencilView* pDepthStencilView, ID3D11ShaderResourceView* pDepthStencilCopySRV);


		virtual void RenderHDRtoCubeMap(ID3D11Device* pD3dDevice, ID3D11DeviceContext * pD3dImmediateContext, const DXGI_SURFACE_DESC * pBackBufferDesc,
			CBaseCamera* pCamera,ID3D11RenderTargetView* g_pTempTextureRenderTargetView, ID3D11DepthStencilView* g_pTempDepthStencilView);

		virtual void RenderIrradiance(ID3D11Device* pD3dDevice, ID3D11DeviceContext * pD3dImmediateContext, const DXGI_SURFACE_DESC * pBackBufferDesc,
			CBaseCamera* pCamera);

		virtual void RenderPrefilter(ID3D11Device* pD3dDevice, ID3D11DeviceContext * pD3dImmediateContext, const DXGI_SURFACE_DESC * pBackBufferDesc,
			CBaseCamera* pCamera, ID3D11RenderTargetView* g_pTempTextureRenderTargetView, ID3D11DepthStencilView* g_pTempDepthStencilView);

		void SetSrcTextureSRV(ID3D11ShaderResourceView* pNewSRV)
		{
			if (m_pSrcTextureSRV!= pNewSRV)
			{
				SAFE_RELEASE(m_pSrcTextureSRV);
				m_pSrcTextureSRV = pNewSRV;
				m_pSrcTextureSRV->AddRef();
			}
			
		}

		ID3D11Texture2D* GetIrradianceTexture()
		{
			return g_pIrradianceCubeTexture;
		}


		ID3D11ShaderResourceView* GetIrradianceSRV()
		{
			return g_pIrradianceSRV;
		}
		
		ID3D11ShaderResourceView* GetPrefilterSRV()
		{
			return g_pPrefilterSRV;
		}

		ID3D11ShaderResourceView* GetEnvSRV()
		{
			return g_pEnvCubeMapSRV;
		}

		ID3D11RenderTargetView* GetIceCubeMapRTV(int i)
		{
			return g_pIceCubemapRTVs[i];
		}

#ifdef EXPORT_CUBEMAP

		ID3D11RenderTargetView* GetPrefilter8x8(int i)
		{
			return RenderTargetArray8x8[i];
		}

		ID3D11RenderTargetView* GetPrefilter16x16(int i)
		{
			return RenderTargetArray16x16[i];
		}

		ID3D11RenderTargetView* GetPrefilter32x32(int i)
		{
			return RenderTargetArray32x32[i];
		}

		ID3D11RenderTargetView* GetPrefilter64x64(int i)
		{
			return RenderTargetArray64x64[i];
		}

		ID3D11RenderTargetView* GetPrefilter128x128(int i)
		{
			return RenderTargetArray128x128[i];
		}
#endif


	protected:
		void AddShadersToCache(AMD::ShaderCache * pShaderCache, const wchar_t * pwsNameVS, const wchar_t * pwsNamePS, const wchar_t * pwsSourceFileName, const D3D11_INPUT_ELEMENT_DESC layout[], UINT size);
		void AddShadersToCache2(AMD::ShaderCache * pShaderCache, const wchar_t * pwsNameVS, const wchar_t * pwsNamePS, const wchar_t * pwsSourceFileName, const D3D11_INPUT_ELEMENT_DESC layout[], UINT size);
		void AddShadersToCache3(AMD::ShaderCache * pShaderCache, const wchar_t * pwsNameVS, const wchar_t * pwsNamePS, const wchar_t * pwsSourceFileName, const D3D11_INPUT_ELEMENT_DESC layout[], UINT size);

		virtual HRESULT CreateCommonBuffers(ID3D11Device * pD3dDevice, GeometryHelper::MeshData& meshData, UINT constBufferPerObjectSize, UINT constBufferPerFrameSize);

		virtual HRESULT CreateOtherRenderStateResources(ID3D11Device * pD3dDevice);

		void ReleaseAllD3D11COM(void);

		virtual void ReleaseSwapChainAssociatedCOM(void);
		virtual void ReleaseOneTimeInitedCOM(void);

		virtual HRESULT CreateSwapChainAssociatedResource(ID3D11Device * pD3dDevice, const DXGI_SURFACE_DESC * pBackBufferSurfaceDesc);
	private:

		ID3D11Buffer* m_pMeshIB;
		ID3D11Buffer* m_pMeshVB;

		ID3D11InputLayout* m_pShaderInputLayout;
		ID3D11VertexShader* m_pShaderVS;
		ID3D11PixelShader* m_pShaderPS;

		ID3D11InputLayout* m_pShaderInputLayout2;
		ID3D11VertexShader* m_pShaderVS2;
		ID3D11PixelShader* m_pShaderPS2;

		ID3D11InputLayout* m_pShaderInputLayout3;
		ID3D11VertexShader* m_pShaderVS3;
		ID3D11PixelShader* m_pShaderPS3;



		ID3D11DepthStencilState* m_pDepthStencilState;
		ID3D11DepthStencilState* m_pDisableDepthStencilState;
		ID3D11RasterizerState* m_pRasterizerState;
		ID3D11BlendState* m_pBlendState;
		ID3D11SamplerState* m_pSamplerLinearClamp;
		ID3D11SamplerState* m_pSamplerLinearWrap;
		ID3D11SamplerState* m_pSamplerPoint;

		ID3D11Buffer* m_pConstantBufferPerObject;
		ID3D11Buffer* m_pConstantBufferPerFrame;

		UINT m_uSizeConstantBufferPerObject;
		UINT m_uSizeConstantBufferPerFrame;

		ID3D11Texture2D* m_pHdrTexture;
		ID3D11ShaderResourceView* m_pHdrTextureSRV;

		ID3D11ShaderResourceView* m_pSrcTextureSRV;

		DirectX::XMVECTOR m_UpVector[6];
		DirectX::XMMATRIX m_ViewMatrix[6];
		CFirstPersonCamera g_TempCubeMapCamera;

		D3D11_VIEWPORT g_Viewport8;
		D3D11_VIEWPORT g_Viewport16;
		D3D11_VIEWPORT g_Viewport32;
		D3D11_VIEWPORT g_Viewport64;
		D3D11_VIEWPORT g_Viewport128;
		D3D11_VIEWPORT g_Viewport512;




		ID3D11Texture2D* g_pCubeTexture = nullptr;
		ID3D11RenderTargetView* g_pEnvCubeMapRTVs[6] = { nullptr,nullptr,nullptr,nullptr ,nullptr,nullptr };
		ID3D11ShaderResourceView* g_pEnvCubeMapSRV = nullptr;

		// Depth stencil data
		ID3D11Texture2D* g_pDepthStencilTexture8x8 = nullptr;
		ID3D11DepthStencilView* g_pDepthStencilView8x8 = nullptr;
		ID3D11ShaderResourceView* g_pDepthStencilSRV8x8 = nullptr;

		ID3D11Texture2D* g_pDepthStencilTexture16x16 = nullptr;
		ID3D11DepthStencilView* g_pDepthStencilView16x16 = nullptr;
		ID3D11ShaderResourceView* g_pDepthStencilSRV16x16 = nullptr;

		ID3D11Texture2D* g_pDepthStencilTexture32x32 = nullptr;
		ID3D11DepthStencilView* g_pDepthStencilView32x32 = nullptr;
		ID3D11ShaderResourceView* g_pDepthStencilSRV32x32 = nullptr;

		ID3D11Texture2D* g_pDepthStencilTexture64x64 = nullptr;
		ID3D11DepthStencilView* g_pDepthStencilView64x64 = nullptr;
		ID3D11ShaderResourceView* g_pDepthStencilSRV64x64 = nullptr;

		ID3D11Texture2D* g_pDepthStencilTexture128x128 = nullptr;
		ID3D11DepthStencilView* g_pDepthStencilView128x128 = nullptr;
		ID3D11ShaderResourceView* g_pDepthStencilSRV128x128 = nullptr;

		ID3D11Texture2D* g_pDepthStencilTexture = nullptr;
		ID3D11DepthStencilView* g_pDepthStencilView = nullptr;
		ID3D11ShaderResourceView* g_pDepthStencilSRV = nullptr;

		ID3D11Resource* g_pIceCubemapTexture = nullptr;
		ID3D11RenderTargetView* g_pIceCubemapRTVs[6] = { nullptr,nullptr,nullptr,nullptr ,nullptr,nullptr };
		ID3D11ShaderResourceView* g_pIceEnvCubeMapSRV = nullptr;


		ID3D11Texture2D* g_pIrradianceCubeTexture = nullptr;
		ID3D11RenderTargetView* g_pIrradianceCubeMapRTVs[6] = { nullptr,nullptr,nullptr,nullptr ,nullptr,nullptr };
		ID3D11ShaderResourceView* g_pIrradianceSRV = nullptr;

		ID3D11Texture2D* g_pPrefilterCubeTexture = nullptr;
		std::vector<ID3D11RenderTargetView*> PrefilterRenderTarget8x8;
		std::vector<ID3D11RenderTargetView*> PrefilterRenderTarget16x16;
		std::vector<ID3D11RenderTargetView*> PrefilterRenderTarget32x32;
		std::vector<ID3D11RenderTargetView*> PrefilterRenderTarget64x64;
		std::vector<ID3D11RenderTargetView*> PrefilterRenderTarget128x128;
		ID3D11ShaderResourceView* g_pPrefilterSRV = nullptr;


#ifdef EXPORT_CUBEMAP
		std::vector<ID3D11Texture2D*> TextureArray8x8;
		std::vector<ID3D11Texture2D*> TextureArray16x16;
		std::vector<ID3D11Texture2D*> TextureArray32x32;
		std::vector<ID3D11Texture2D*> TextureArray64x64;
		std::vector<ID3D11Texture2D*> TextureArray128x128;

		std::vector<ID3D11RenderTargetView*> RenderTargetArray8x8;
		std::vector<ID3D11RenderTargetView*> RenderTargetArray16x16;
		std::vector<ID3D11RenderTargetView*> RenderTargetArray32x32;
		std::vector<ID3D11RenderTargetView*> RenderTargetArray64x64;
		std::vector<ID3D11RenderTargetView*> RenderTargetArray128x128;
#endif


		bool m_bShaderInited;

	};




}