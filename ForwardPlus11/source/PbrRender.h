#pragma once

#include "GeometryHelper.h"
#include <d3d11.h>
#include "../../DXUT/Core/DXUT.h"
#include "../../amd_sdk/inc/AMD_SDK.h"
#include "../../DXUT/Optional/DXUTCamera.h"


namespace ForwardRender
{

	class PbrRender
	{
	public:
		PbrRender();
		~PbrRender();


		struct CB_PER_OBJECT
		{
			DirectX::XMMATRIX mWorldViewProjection;
			DirectX::XMMATRIX mWorld;
		};

		struct CB_PER_FRAME
		{
			DirectX::XMVECTOR vCameraPos3AndAlphaTest;
			DirectX::XMVECTOR vScreenSize;

			DirectX::XMVECTOR vLightPostion[4];
			DirectX::XMVECTOR vLightColors[4];

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
			CBaseCamera* pCamera, ID3D11RenderTargetView* pRTV, ID3D11DepthStencilView* pDepthStencilView, 
			ID3D11ShaderResourceView* pIrradianceSRV, ID3D11ShaderResourceView* pPrefilterSRV);


		void SetSrcTextureSRV(ID3D11ShaderResourceView* pNewSRV)
		{
			if (m_pSrcTextureSRV != pNewSRV)
			{
				SAFE_RELEASE(m_pSrcTextureSRV);
				m_pSrcTextureSRV = pNewSRV;
				m_pSrcTextureSRV->AddRef();
			}

		}

	protected:
		void AddShadersToCache(AMD::ShaderCache * pShaderCache, const wchar_t * pwsNameVS, const wchar_t * pwsNamePS, const wchar_t * pwsSourceFileName, const D3D11_INPUT_ELEMENT_DESC layout[], UINT size);

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

		ID3D11DepthStencilState* m_pDepthStencilState;
		ID3D11RasterizerState* m_pRasterizerState;
		ID3D11BlendState* m_pBlendState;
		ID3D11SamplerState* m_pSamplerLinear;
		ID3D11SamplerState* m_pSamplerPoint;

		ID3D11Buffer* m_pConstantBufferPerObject;
		ID3D11Buffer* m_pConstantBufferPerFrame;

		UINT m_uSizeConstantBufferPerObject;
		UINT m_uSizeConstantBufferPerFrame;

		ID3D11ShaderResourceView* m_pSrcTextureSRV;


		D3D11_VIEWPORT g_Viewport;

		// Depth stencil data
		ID3D11Texture2D* g_pDepthStencilTexture = nullptr;
		ID3D11DepthStencilView* g_pDepthStencilView = nullptr;
		ID3D11ShaderResourceView* g_pDepthStencilSRV = nullptr;

		ID3D11Texture2D* m_pBrdfLUT = nullptr;
		ID3D11ShaderResourceView* m_pBrdfSRV = nullptr;

		ID3D11Texture2D* m_pAlbedo = nullptr;
		ID3D11ShaderResourceView* m_pAlbedoSRV = nullptr;

		ID3D11Texture2D* m_pNormal = nullptr;
		ID3D11ShaderResourceView* m_pNormalSRV = nullptr;

		ID3D11Texture2D* m_pMetallic = nullptr;
		ID3D11ShaderResourceView* m_pMetallicSRV = nullptr;

		ID3D11Texture2D* m_pRoughness = nullptr;
		ID3D11ShaderResourceView* m_pRoughnessSRV = nullptr;

		ID3D11Texture2D* m_pAmbientOverlap = nullptr;
		ID3D11ShaderResourceView* m_pAmbientOverlapSRV = nullptr;

		bool m_bShaderInited;

	};

}