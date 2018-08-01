#pragma once

#include "GeometryHelper.h"
#include <d3d11.h>
#include "../../DXUT/Core/DXUT.h"
#include "../../amd_sdk/inc/AMD_SDK.h"
#include "../../DXUT/Core/DXUT.h"
#include "../../DXUT/Optional/DXUTcamera.h"

namespace Triangle
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

	class TriangleRender
	{

	public:

		// Direct3D 11 resources

		//Rasterize states
		ID3D11RasterizerState* m_pCullingBackRS = nullptr;

		// Depth stencil states
		ID3D11DepthStencilState* m_pDepthStencilDefaultDS = nullptr;

		// Blend states 
		ID3D11BlendState* m_pOpaqueState = nullptr;
		ID3D11BlendState* m_pDepthOnlyState = nullptr;//No color pass can be writen
		ID3D11BlendState* m_pDepthOnlyAndAlphaToCoverageState = nullptr;

		TriangleRender();
		~TriangleRender();

		GeometryHelper::MeshData m_MeshData;

		void GenerateMeshData(float width = 1.0f, float height = 1.0f, float depth = 1.0f, unsigned int numSubdivision = 6, float centerX = 0.0f, float centerY = 0.0f, float centerZ = 0.0f);

		static void CalculateSceneMinMax(GeometryHelper::MeshData& meshData, DirectX::XMVECTOR* pBBoxMinOut, DirectX::XMVECTOR* pBBoxMaxOut);

		void AddShadersToCache(AMD::ShaderCache* pShaderCache);

		HRESULT  OnD3DDeviceCreated(ID3D11Device* pD3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
			void* pUserContext);

		void OnD3D11DestroyDevice(void * pUserContext);
		void OnReleasingSwapChain();
		HRESULT OnResizedSwapChain(ID3D11Device* pD3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc);
		void OnRender(ID3D11Device * pD3dDevice, ID3D11DeviceContext * pD3dImmediateContext, CBaseCamera* pCamera, ID3D11RenderTargetView* pRTV = nullptr, ID3D11DepthStencilView* pDepthStencilView = nullptr);

		void SetPosition(const DirectX::XMVECTOR& position)
		{
			XMStoreFloat4(&m_Position4, position);
		}

		ID3D11Buffer* m_pMeshIB;
		ID3D11Buffer* m_pMeshVB;
		ID3D11InputLayout* m_pPosAndNormalAndTextureInputLayout;
		ID3D11VertexShader* m_pScenePosAndNormalAndTextureVS;
		ID3D11PixelShader* m_pScenePosAndNomralAndTexturePS;
		ID3D11Buffer* m_pConstantBufferPerObject = nullptr;
		ID3D11Buffer* m_pConstantBufferPerFrame = nullptr;
		ID3D11SamplerState* m_pSamplerLinear = nullptr;

	protected:

		void ReleaseAllD3D11COM(void);

	private:

		DirectX::XMFLOAT4 m_Position4;
	};
}

