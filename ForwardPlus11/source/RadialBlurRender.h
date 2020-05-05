#pragma once

#include "BasePostProcessRender.h"

namespace PostProcess
{
	class RadialBlurRender:public BasePostProcessRender
	{
#pragma pack(push,1)
		struct CB_PER_OBJECT
		{
			DirectX::XMFLOAT2 RadialBlurCenterUV;
			float fRadialBlurLength;
			INT32 iSampleCount;
		};

		struct CB_PER_FRAME
		{

		};
#pragma pack(pop)

	public:
		RadialBlurRender();
		~RadialBlurRender();

		void AddShadersToCache(AMD::ShaderCache* pShaderCache)override;

		virtual void OnRender(ID3D11Device* pD3dDevice, ID3D11DeviceContext * pD3dImmediateContext, const DXGI_SURFACE_DESC* pBackBufferDesc,
			CBaseCamera* pCamera, ID3D11RenderTargetView* pRTV, ID3D11DepthStencilView* pDepthStencilView, ID3D11ShaderResourceView* pDepthStencilCopySRV)override;

		void SetRadialBlurCenter(float BlurCenterU, float BlurCenterV)
		{
			SetRadialBlurCenter(DirectX::XMFLOAT2(BlurCenterU, BlurCenterV));
		}

		void SetRadialBlurCenter(DirectX::XMFLOAT2 BlurCenter)
		{
			m_RadialBlurCenterUV = BlurCenter;
		}

		void SetRadialBlurTextureSRV(ID3D11ShaderResourceView* pNewSRV)
		{
			SetSrcTextureSRV(pNewSRV);
		}

	protected:
		HRESULT CreateOtherRenderStateResources(ID3D11Device * pD3dDevice)override;
		virtual void ReleaseOneTimeInitedCOM(void) override;

	private:
		ID3D11BlendState* m_pOpaqueBlendState;

		DirectX::XMFLOAT2 m_RadialBlurCenterUV;
		float m_fRadialBlurLength;
		INT32 m_iSampleCount;
	};
}