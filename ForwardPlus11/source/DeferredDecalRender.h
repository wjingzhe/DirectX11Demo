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
		DirectX::XMMATRIX mWolrd;
	};

	struct CB_PER_FRAME
	{
		DirectX::XMVECTOR vCameraPos3AndAlphaTest;
	};

#pragma pack(pop)

	class DeferredDecalRender
	{
	public:
		DeferredDecalRender();
		~DeferredDecalRender();

		GeometryHelper::MeshData m_MeshData;

		void GenerateMeshData();

		static void CalculateSceneMinMax(GeometryHelper::MeshData& meshData, DirectX::XMVECTOR* pBBoxMinOut, DirectX::XMVECTOR* pBBoxMaxOut);

		void AddShadersToCache(AMD::ShaderCache* pShaderCache);

		HRESULT  OnD3DDeviceCreated(ID3D11Device* pD3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
			void* pUserContext);

		void OnD3D11DestroyDevice(void * pUserContext);
		void OnReleasingSwapChain();
		HRESULT OnResizedSwapChain(ID3D11Device* pD3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc);
		void OnRender(ID3D11Device * pD3dDevice, ID3D11DeviceContext * pD3dImmediateContext, CBaseCamera* pCamera, ID3D11RenderTargetView* pRTV = nullptr, ID3D11DepthStencilView* pDepthStencilView = nullptr);

		ID3D11Buffer* m_pMeshIB;
		ID3D11Buffer* m_pMeshVB;
		ID3D11InputLayout* m_pPosAndNormalAndTextureInputLayout;
		ID3D11VertexShader* m_pScenePosAndNormalAndTextureVS;
		ID3D11PixelShader* m_pScenePosAndNomralAndTexturePS;

	protected:
		void ReleaseAllD3D11COM(void);

	private:


		ID3D11Buffer* m_pConstantBufferPerObject;
		ID3D11Buffer* m_pConstantBufferPerFrame;
		bool m_bShaderInited;
	};
}