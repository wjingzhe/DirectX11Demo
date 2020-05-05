#pragma once

#include "GeometryHelper.h"
#include <d3d11.h>
#include "../../DXUT/Core/DXUT.h"
#include "../../amd_sdk/inc/AMD_SDK.h"
#include "../../DXUT/Optional/DXUTcamera.h"
#include "BasePostProcessRender.h"

namespace PostProcess
{

	class DeferredDecalRender :public BasePostProcessRender
	{
	public:
		DeferredDecalRender();
		~DeferredDecalRender();

		void GenerateMeshData(float width = 1.0f, float height = 1.0f, float depth = 1.0f);

		void AddShadersToCache(AMD::ShaderCache* pShaderCache);

		HRESULT  CreateOtherRenderStateResources(ID3D11Device* pD3dDevice);

		virtual void ReleaseOneTimeInitedCOM(void);

		void OnRender(ID3D11Device * pD3dDevice, ID3D11DeviceContext * pD3dImmediateContext, const DXGI_SURFACE_DESC* pBackBufferDesc,
			CBaseCamera* pCamera, ID3D11RenderTargetView* pRTV, ID3D11DepthStencilView* pDepthStencilView, ID3D11ShaderResourceView* pDepthStencilCopySRV);

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

	private:
		ID3D11ShaderResourceView* m_pDecalTextureSRV;

	



		ID3D11DepthStencilState* m_pDepthAlwaysAndStencilOnlyOneTime;
		//SamplerState
		ID3D11SamplerState* m_pSamAnisotropic;
	};
}