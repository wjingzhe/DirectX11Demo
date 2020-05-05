#pragma once

#include "GeometryHelper.h"
#include <d3d11.h>
#include "../../DXUT/Core/DXUT.h"
#include "../../amd_sdk/inc/AMD_SDK.h"
#include "../../DXUT/Optional/DXUTCamera.h"


namespace PostProcess
{
	class BasePostProcessRender
	{
	protected:
		//--------------------------------
		// Constant buffers
		//--------------------------------
#pragma pack(push,1)
		struct CB_PER_OBJECT
		{
			DirectX::XMMATRIX mWorldViewProjection;
			DirectX::XMMATRIX mWorld;
			DirectX::XMMATRIX mWorldViewInv;
			DirectX::XMFLOAT4 vBoxCenterAndRadius;
		};

		struct CB_PER_FRAME
		{
			DirectX::XMVECTOR vCameraPos3AndAlphaTest;
			DirectX::XMMATRIX m_mProjection;
			DirectX::XMMATRIX m_mProjectionInv;
			DirectX::XMFLOAT4 ProjParams;
			DirectX::XMFLOAT4 RenderTargetHalfSizeAndFarZ;
		};

#pragma pack(pop)

	public:
		BasePostProcessRender();
		virtual ~BasePostProcessRender();

		GeometryHelper::MeshData m_MeshData;

		static void CalculateSceneMinMax(GeometryHelper::MeshData& meshData, DirectX::XMVECTOR* pBBoxMinOut, DirectX::XMVECTOR* pBBoxMaxOut);

		virtual void AddShadersToCache(AMD::ShaderCache* pShaderCache);

		HRESULT  OnD3DDeviceCreated(ID3D11Device* pD3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
			void* pUserContext);

		void OnD3D11DestroyDevice(void * pUserContext);

		//when some sln or window size changed
		void OnReleasingSwapChain(void);
		void OnResizedSwapChain(ID3D11Device* pD3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc);

		virtual void OnRender(ID3D11Device* pD3dDevice, ID3D11DeviceContext * pD3dImmediateContext, const DXGI_SURFACE_DESC* pBackBufferDesc,
			CBaseCamera* pCamera, ID3D11RenderTargetView* pRTV, ID3D11DepthStencilView* pDepthStencilView, ID3D11ShaderResourceView* pDepthStencilCopySRV) = 0;


		void SetMeshCenterOffset(const DirectX::XMVECTOR& position)
		{
			XMStoreFloat4(&m_Position4, position);
		}

		void SetMeshCenterOffset(const DirectX::XMFLOAT4& position)
		{
			m_Position4 = position;
		}

		void SetSrcTextureSRV(ID3D11ShaderResourceView* pNewSRV)
		{
			if (m_pSrcTextureSRV!= pNewSRV)
			{
				SAFE_RELEASE(m_pSrcTextureSRV);
				m_pSrcTextureSRV = pNewSRV;
				m_pSrcTextureSRV->AddRef();
			}
			
		}

		void SetSize_ConstantBufferPerObject(UINT size)
		{
			m_uSizeConstantBufferPerObject = size;
		}

		void SetSize_ConstantBufferPerFrame(UINT size)
		{
			m_uSizeConstantBufferPerFrame = size;
		}

	protected:
		void AddShadersToCache(AMD::ShaderCache * pShaderCache, const wchar_t * pwsNameVS, const wchar_t * pwsNamePS, const wchar_t * pwsSourceFileName, const D3D11_INPUT_ELEMENT_DESC layout[], UINT size);

		virtual HRESULT CreateCommonBuffers(ID3D11Device * pD3dDevice, GeometryHelper::MeshData& meshData, UINT constBufferPerObjectSize, UINT constBufferPerFrameSize);

		virtual HRESULT CreateOtherRenderStateResources(ID3D11Device * pD3dDevice);

		void ReleaseAllD3D11COM(void);

		virtual void ReleaseSwapChainAssociatedCOM(void);
		virtual void ReleaseOneTimeInitedCOM(void);

		virtual HRESULT CreateSwapChainAssociatedResource(ID3D11Device * pD3dDevice, const DXGI_SURFACE_DESC * pBackBufferSurfaceDesc);

	protected:
		DirectX::XMFLOAT4 m_Position4;

		ID3D11Buffer* m_pMeshIB;
		ID3D11Buffer* m_pMeshVB;

		ID3D11InputLayout* m_pShaderInputLayout;
		ID3D11VertexShader* m_pShaderVS_Pos_Normal_UV;
		ID3D11PixelShader* m_pShaderPS_Pos_Normal_UV;

		ID3D11DepthStencilState* m_pDepthStencilState;
		ID3D11RasterizerState* m_pRasterizerState;
		ID3D11BlendState* m_pBlendState;
		ID3D11SamplerState* m_pSamplerState;

		ID3D11Buffer* m_pConstantBufferPerObject;
		ID3D11Buffer* m_pConstantBufferPerFrame;

		UINT m_uSizeConstantBufferPerObject;
		UINT m_uSizeConstantBufferPerFrame;

		ID3D11ShaderResourceView* m_pSrcTextureSRV;

		bool m_bShaderInited;

		DirectX::XMFLOAT4 m_BoxExtend;
	};
}