#pragma once

#include "GeometryHelper.h"
#include <d3d11.h>
#include "../../DXUT/Core/DXUT.h"
#include "../../amd_sdk/inc/AMD_SDK.h"

class TriangleRender
{
public:
	TriangleRender();
	~TriangleRender();

	GeometryHelper::MeshData m_MeshData;

	void GenerateMeshData();

	static void CalculateSceneMinMax(GeometryHelper::MeshData& meshData, DirectX::XMVECTOR* pBBoxMinOut, DirectX::XMVECTOR* pBBoxMaxOut);

	void AddShadersToCache(AMD::ShaderCache& shaderCache);

	HRESULT  OnD3DDeviceCreated(ID3D11Device* pD3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
		void* pUserContext);

	void OnD3D11DestroyDevice(void * pUserContext);
	void OnReleasingSwapChain();
	HRESULT OnResizedSwapChain(ID3D11Device* pD3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc);
	void OnRender(ID3D11Device * pD3dDevice, ID3D11DeviceContext * pD3dImmediateContext, double fTime, float fElapsedTime, void* pUserContext);

	ID3D11Buffer* m_pMeshIB;
	ID3D11Buffer* m_pMeshVB;
	ID3D11InputLayout* m_pPosAndNormalAndTextureInputLayout;
	ID3D11VertexShader* m_pScenePosAndNormalAndTextureVS;
	ID3D11PixelShader* m_pScenePosAndNomralAndTexturePS;

protected:
private:
};