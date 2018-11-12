#pragma once

#include "BasePostProcessRender.h"

namespace PostProcess
{
	class SphereRender:public BasePostProcessRender
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