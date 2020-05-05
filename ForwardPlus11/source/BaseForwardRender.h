#pragma once

#include "GeometryHelper.h"
#include <d3d11.h>
#include "../../DXUT/Core/DXUT.h"
#include "../../amd_sdk/inc/AMD_SDK.h"
#include "../../DXUT/Core/DXUT.h"
#include "../../DXUT/Optional/DXUTcamera.h"

namespace ForwardRender
{

	class BaseForwardRender
	{
	public:
#pragma pack(push,1)
		struct CB_PER_OBJECT
		{
			DirectX::XMMATRIX mWorldViewProjection;
			DirectX::XMMATRIX mWorld;
		};

		struct CB_PER_FRAME
		{
			DirectX::XMVECTOR vCameraPos3AndAlphaTest;
		};

#pragma pack(pop)

	public:
		BaseForwardRender();
		virtual~BaseForwardRender();


		

	public:
		GeometryHelper::MeshData m_MeshData;

		virtual void GenerateMeshData(float width = 1.0f, float height = 1.0f, float depth = 1.0f, float centerX = 0.0f, float centerY = 0.0f, float centerZ = 0.0f);

		static void CalculateBoundingBoxMinMax(const GeometryHelper::MeshData& meshData, DirectX::XMVECTOR* pBoxMinOut, DirectX::XMVECTOR* pBoxMaxOut);

		virtual void AddShadersToCache(AMD::ShaderCache* pShaderCache);

		HRESULT  OnD3DDeviceCreated(ID3D11Device* pD3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
			void* pUserContext);

		void OnD3D11DestroyDevice(void * pUserContext);

		void OnReleasingSwapChain(void);
		HRESULT OnResizedSwapChain(ID3D11Device* pD3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc);

		virtual void OnRender(ID3D11Device * pD3dDevice, ID3D11DeviceContext * pD3dImmediateContext, CBaseCamera* pCamera, ID3D11RenderTargetView* pRTV = nullptr, ID3D11DepthStencilView* pDepthStencilView = nullptr);

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

		// Depth stencil states
		ID3D11DepthStencilState* m_pDepthStencilState = nullptr;
		ID3D11RasterizerState* m_pRasterizerState = nullptr;
		ID3D11BlendState* m_pBlendState = nullptr;
		ID3D11SamplerState* m_pSamplerState;


		ID3D11Buffer* m_pMeshIB;
		ID3D11Buffer* m_pMeshVB;

		ID3D11InputLayout* m_pDefaultShaderInputLayout;
		ID3D11VertexShader* m_pDefaultShaderVS;
		ID3D11PixelShader* m_pDefaultShaderPS;

		ID3D11Buffer* m_pConstantBufferPerObject;
		ID3D11Buffer* m_pConstantBufferPerFrame;

		UINT m_uSizeConstantBufferPerObject;
		UINT m_uSizeConstantBufferPerFrame;

		bool m_bShaderInited;
	};



}