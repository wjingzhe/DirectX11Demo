#pragma once

#include "BasePostProcessRender.h"

namespace PostProcess
{
	class SphereRender:public BasePostProcessRender
	{
		//--------------------------------
		// Constant buffers
		//--------------------------------
	public:
		SphereRender();
		~SphereRender();

		void AddShadersToCache(AMD::ShaderCache* pShaderCache)override;

		virtual void OnRender(ID3D11Device* pD3dDevice, ID3D11DeviceContext * pD3dImmediateContext, const DXGI_SURFACE_DESC* pBackBufferDesc,
			CBaseCamera* pCamera, ID3D11RenderTargetView* pRTV, ID3D11DepthStencilView* pDepthStencilView, ID3D11ShaderResourceView* pDepthStencilCopySRV)override;

	protected:
		virtual HRESULT CreateOtherRenderStateResources(ID3D11Device * pD3dDevice)override;


	private:

	};
}