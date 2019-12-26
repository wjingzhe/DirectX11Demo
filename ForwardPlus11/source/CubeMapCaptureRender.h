#pragma once

#include "GeometryHelper.h"
#include <d3d11.h>
#include "../../DXUT/Core/DXUT.h"
#include "../../amd_sdk/inc/AMD_SDK.h"
#include "../../DXUT/Optional/DXUTCamera.h"



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
			CBaseCamera* pCamera, ID3D11RenderTargetView* g_pTempTextureRenderTargetView, ID3D11DepthStencilView* g_pTempDepthStencilView);

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

		ID3D11ShaderResourceView* GetIrradianceSRV()
		{
			return g_pIrradianceSRV;
		}
		
		ID3D11ShaderResourceView* GetPrefilterSRV()
		{
			return g_pPrefilterSRV;
		}


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
		ID3D11RasterizerState* m_pRasterizerState;
		ID3D11BlendState* m_pBlendState;
		ID3D11SamplerState* m_pSamplerLinear;
		ID3D11SamplerState* m_pSamplerPoint;

		ID3D11Buffer* m_pConstantBufferPerObject;
		ID3D11Buffer* m_pConstantBufferPerFrame;

		UINT m_uSizeConstantBufferPerObject;
		UINT m_uSizeConstantBufferPerFrame;

		//ID3D11Texture2D* m_pHdrTexture;
		ID3D11ShaderResourceView* m_pSrcTextureSRV;

		DirectX::XMMATRIX m_ViewMatrix[6];
		CFirstPersonCamera g_TempCubeMapCamera;
		D3D11_VIEWPORT g_Viewport;

		ID3D11Texture2D* g_pCubeTexture = nullptr;
		ID3D11RenderTargetView* g_pEnvCubeMapRTVs[6] = { nullptr,nullptr,nullptr,nullptr ,nullptr,nullptr };
		

		// Depth stencil data
		ID3D11Texture2D* g_pDepthStencilTexture = nullptr;
		ID3D11DepthStencilView* g_pDepthStencilView = nullptr;
		ID3D11ShaderResourceView* g_pDepthStencilSRV = nullptr;

		ID3D11Resource* g_pIceCubemapTexture = nullptr;
		ID3D11ShaderResourceView* g_pEnvCubeMapSRV = nullptr;


		ID3D11Texture2D* g_pIrradianceCubeTexture = nullptr;
		ID3D11RenderTargetView* g_pIrradianceCubeMapRTVs[6] = { nullptr,nullptr,nullptr,nullptr ,nullptr,nullptr };
		ID3D11ShaderResourceView* g_pIrradianceSRV = nullptr;

		ID3D11Texture2D* g_pPrefilterCubeTexture = nullptr;
		ID3D11RenderTargetView* g_pPrefilterCubeMapRTVs[6] = { nullptr,nullptr,nullptr,nullptr ,nullptr,nullptr };
		ID3D11ShaderResourceView* g_pPrefilterSRV = nullptr;


		bool m_bShaderInited;

	};




}