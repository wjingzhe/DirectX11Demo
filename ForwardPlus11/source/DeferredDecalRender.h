#pragma once

#include "GeometryHelper.h"
#include <d3d11.h>
#include "../../DXUT/Core/DXUT.h"
#include "../../amd_sdk/inc/AMD_SDK.h"
#include "../../DXUT/Optional/DXUTcamera.h"


namespace PostProcess
{
	//--------------------------------
	// Constant buffers
	//--------------------------------
#pragma pack(push,1)
	struct CB_PER_OBJECT
	{
		DirectX::XMMATRIX mWorldViewProjection;
		DirectX::XMMATRIX mWorld;
		DirectX::XMMATRIX mWorldViewInv;
		DirectX::XMFLOAT4 vBoxExtend;
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

	class DeferredDecalRender
	{
	public:
		DeferredDecalRender();
		~DeferredDecalRender();

		GeometryHelper::MeshData m_MeshData;

		void GenerateMeshData(float width = 1.0f, float height = 1.0f, float depth = 1.0f,unsigned int numSubdivision = 6);

		static void CalculateSceneMinMax(GeometryHelper::MeshData& meshData, DirectX::XMVECTOR* pBBoxMinOut, DirectX::XMVECTOR* pBBoxMaxOut);

		void AddShadersToCache(AMD::ShaderCache* pShaderCache);

		HRESULT  OnD3DDeviceCreated(ID3D11Device* pD3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
			void* pUserContext);

		void OnD3D11DestroyDevice(void * pUserContext);
		void OnReleasingSwapChain();
		HRESULT OnResizedSwapChain(ID3D11Device* pD3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc);
		void OnRender(ID3D11Device * pD3dDevice, ID3D11DeviceContext * pD3dImmediateContext, const DXGI_SURFACE_DESC* pBackBufferDesc,
			CBaseCamera* pCamera, ID3D11RenderTargetView* pRTV, ID3D11DepthStencilView* pDepthStencilView, ID3D11ShaderResourceView* pDepthStencilCopySRV);

		ID3D11Buffer* m_pMeshIB;
		ID3D11Buffer* m_pMeshVB;
		ID3D11InputLayout* m_pPosAndNormalAndTextureInputLayout;
		ID3D11VertexShader* m_pScenePosAndNormalAndTextureVS;
		ID3D11PixelShader* m_pScenePosAndNormalAndTexturePS;

		void SetDecalPosition(const DirectX::XMVECTOR& position)
		{
			XMStoreFloat4(&m_Position4, position);
		}

		void SetDecalPosition(const DirectX::XMFLOAT4& position)
		{
			m_Position4 = position;
		}

		void SetDecalTextureSRV(ID3D11ShaderResourceView* pDecalTextureSRV)
		{
			SAFE_RELEASE(m_pDecalTextureSRV);
			m_pDecalTextureSRV = pDecalTextureSRV;
			m_pDecalTextureSRV->AddRef();
		}

	//	ID3D11ShaderResourceView* const * GetDecalTextureSRV() { return &m_pDecalTextureSRV; }

	protected:
		void ReleaseAllD3D11COM(void);

	private:
		ID3D11ShaderResourceView* m_pDecalTextureSRV;

		DirectX::XMFLOAT4 m_Position4;

		DirectX::XMFLOAT4 m_BoxExtend;

		ID3D11DepthStencilState* m_pDepthAlwaysAndStencilOnlyOneTime;
		ID3D11RasterizerState* m_pRasterizerState;
		//SamplerState
		ID3D11SamplerState* m_pSamAnisotropic;

		ID3D11Buffer* m_pConstantBufferPerObject;
		ID3D11Buffer* m_pConstantBufferPerFrame;
		bool m_bShaderInited;
	};
}