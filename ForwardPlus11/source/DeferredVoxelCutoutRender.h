#pragma once

#include "GeometryHelper.h"
#include <d3d11.h>
#include "../../DXUT/Core/DXUT.h"
#include "../../amd_sdk/inc/AMD_SDK.h"
#include "../../DXUT/Optional/DXUTCamera.h"
#include "BasePostProcessRender.h"


//
// 将体素内的2D像素绘制为标记色（vMaskedColor4默认为黑色），其余像素绘制为光颜色vCommonColor4
//

namespace PostProcess
{
	//--------------------------------
	// Constant buffers
	//--------------------------------


	class DeferredVoxelCutoutRender :public BasePostProcessRender
	{
#pragma pack(push,1)
		struct CB_PER_OBJECT
		{
			DirectX::XMMATRIX mWorldViewProjection;
			DirectX::XMMATRIX mWorld;
			DirectX::XMMATRIX mWorldViewInv;
			DirectX::XMFLOAT4 vBoundingParam;//if box:width/height/depth;if sphere:center.xyz/radius
		};

		struct CB_PER_FRAME
		{
			DirectX::XMVECTOR vCameraPos3AndAlphaTest;
			DirectX::XMMATRIX m_mProjection;
			DirectX::XMMATRIX m_mProjectionInv;
			DirectX::XMFLOAT4 ProjParams;
			DirectX::XMFLOAT4 RenderTargetHalfSizeAndFarZ;
			DirectX::XMFLOAT4 vCommonColor4;
			DirectX::XMFLOAT4 vMaskedColor4;
		};

#pragma pack(pop)

	public:
		DeferredVoxelCutoutRender();
		~DeferredVoxelCutoutRender();



		void AddShadersToCache(AMD::ShaderCache* pShaderCache)override;
		

		HRESULT OnResizedSwapChain(ID3D11Device* pD3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc);



		void OnRender(ID3D11Device* pD3dDevice, ID3D11DeviceContext * pD3dImmediateContext, const DXGI_SURFACE_DESC* pBackBufferDesc,
				CBaseCamera* pCamera,ID3D11RenderTargetView* pRTV,ID3D11DepthStencilView* pDepthStencilView,ID3D11ShaderResourceView* pDepthStencilCopySRV);


		void SetVoxelPosition(const DirectX::XMVECTOR& position)
		{
			XMStoreFloat4(&m_Position4, position);
		}

		void SetVoxelPosition(const DirectX::XMFLOAT4& position)
		{
			m_Position4 = position;
		}

		DirectX::XMFLOAT4 GetVoxelPosition(void)
		{
			return m_Position4;
		}

	protected:
		virtual void ReleaseOneTimeInitedCOM(void) override;

		HRESULT CreateTempShaderRenderTarget(ID3D11Device * pD3dDevice, const DXGI_SURFACE_DESC * pBackBufferSurfaceDesc);

		void GenerateSphereMeshData(float centerX = 1.0f, float centerY = 1.0f, float centerZ = 1.0f, float radius = 1.0f, unsigned int numSubdivision = 6);

		HRESULT CreateOtherRenderStateResources(ID3D11Device * pD3dDevice)override;

	private:

		DirectX::XMFLOAT4 m_VoxelCenterAndRadius;


		ID3D11DepthStencilState* m_pDepthLessAndStencilOnlyOneTime;
		ID3D11BlendState* m_pOpaqueBlendState;

		DirectX::XMFLOAT4 m_CommonColor;
		DirectX::XMFLOAT4 m_OverlapMaskedColor;
	};
}