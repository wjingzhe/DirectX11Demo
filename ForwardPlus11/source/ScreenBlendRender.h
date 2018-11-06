#pragma once

#include "BasePostProcessRender.h"

namespace PostProcess
{
	class ScreenBlendRender:public BasePostProcessRender
	{
	public:
		ScreenBlendRender();
		~ScreenBlendRender();

		void AddShadersToCache(AMD::ShaderCache* pShaderCache)override;

		virtual void OnRender(ID3D11Device* pD3dDevice, ID3D11DeviceContext * pD3dImmediateContext, const DXGI_SURFACE_DESC* pBackBufferDesc,
			CBaseCamera* pCamera, ID3D11RenderTargetView* pRTV, ID3D11DepthStencilView* pDepthStencilView, ID3D11ShaderResourceView* pDepthStencilCopySRV)override;

		void SetDecalTextureSRV(ID3D11ShaderResourceView* pNewSRV)
		{
			SAFE_RELEASE(m_pBlendTextureSRV);
			m_pBlendTextureSRV = pNewSRV;
			m_pBlendTextureSRV->AddRef();
		}

	protected:
		HRESULT CreateOtherRenderStateResources(ID3D11Device * pD3dDevice)override;
		virtual void ReleaseSwapChainAssociatedCOM(void)override;

	private:
		ID3D11ShaderResourceView* m_pBlendTextureSRV;
	};
}