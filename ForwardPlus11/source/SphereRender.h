#pragma once

#include "BaseForwardRender.h"

namespace ForwardRender
{
	class SphereRender:public BaseForwardRender
	{
		//--------------------------------
		// Constant buffers
		//--------------------------------
	public:
		SphereRender();
		~SphereRender();

		void AddShadersToCache(AMD::ShaderCache* pShaderCache)override;

		void OnRender(ID3D11Device * pD3dDevice, ID3D11DeviceContext * pD3dImmediateContext, CBaseCamera* pCamera, ID3D11RenderTargetView* pRTV = nullptr, ID3D11DepthStencilView* pDepthStencilView = nullptr)override;

	protected:
		virtual HRESULT CreateOtherRenderStateResources(ID3D11Device * pD3dDevice)override;


	private:

	};
}