#pragma once

#include "GeometryHelper.h"
#include <d3d11.h>
#include "../../DXUT/Core/DXUT.h"
#include "../../amd_sdk/inc/AMD_SDK.h"
#include "../../DXUT/Core/DXUT.h"
#include "../../DXUT/Optional/DXUTcamera.h"
#include "BaseForwardRender.h"

namespace ForwardRender
{


	class TriangleRender:public BaseForwardRender
	{

	public:

		// Direct3D 11 resources

		// Blend states 
		ID3D11BlendState* m_pDepthOnlyState = nullptr;//No color pass can be writen
		ID3D11BlendState* m_pDepthOnlyAndAlphaToCoverageState = nullptr;

		TriangleRender();
		virtual~TriangleRender();


		void AddShadersToCache(AMD::ShaderCache* pShaderCache)override;

		void OnRender(ID3D11Device * pD3dDevice, ID3D11DeviceContext * pD3dImmediateContext, CBaseCamera* pCamera, ID3D11RenderTargetView* pRTV = nullptr, ID3D11DepthStencilView* pDepthStencilView = nullptr);


protected:
		virtual HRESULT CreateOtherRenderStateResources(ID3D11Device * pD3dDevice) override;

		virtual void ReleaseOneTimeInitedCOM(void)override;

	private:
	};
}

