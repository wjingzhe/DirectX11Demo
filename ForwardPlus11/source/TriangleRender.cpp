#include "TriangleRender.h"
using namespace DirectX;

namespace ForwardRender
{

	TriangleRender::TriangleRender()
		:BaseForwardRender()
	{
	}

	TriangleRender::~TriangleRender()
	{
	}

	void TriangleRender::AddShadersToCache(AMD::ShaderCache* pShaderCache)
	{

		const D3D11_INPUT_ELEMENT_DESC layout[] =
		{
			{ "POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0 },
			{ "NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0 },
			{ "TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0 },
			{ "TANGENT",0,DXGI_FORMAT_R32G32B32_FLOAT,0,32,D3D11_INPUT_PER_VERTEX_DATA,0 },
		};

		INT32 size = ARRAYSIZE(layout);

		BaseForwardRender::AddShadersToCache(pShaderCache, L"BaseGeometryVS", L"BaseGeometryPS", L"DrawBasicGeometry.hlsl", layout, size);

	}


	void TriangleRender::OnRender(ID3D11Device * pD3dDevice, ID3D11DeviceContext * pD3dImmediateContext, CBaseCamera* pCamera,
		ID3D11RenderTargetView* pRTV, ID3D11DepthStencilView* pDepthStencilView)
	{
		// save blend state(for later restore)
		ID3D11BlendState* pBlendStateStored11 = nullptr;
		FLOAT afBlendFactorStored11[4];
		UINT uSampleMaskStored11;
		pD3dImmediateContext->OMGetBlendState(&pBlendStateStored11, afBlendFactorStored11, &uSampleMaskStored11);

		// save depth state (for later restore)
		ID3D11DepthStencilState* pDepthStencilStateStored11 = nullptr;
		UINT uStencilRefStored11;
		pD3dImmediateContext->OMGetDepthStencilState(&pDepthStencilStateStored11, &uStencilRefStored11);


		XMMATRIX mWorld = XMMatrixIdentity();

		//Get the projection & view matrix from the camera class
		XMMATRIX mView = pCamera->GetViewMatrix();
		XMMATRIX mProj = pCamera->GetProjMatrix();
		XMMATRIX mWorldViewPrjection = mWorld*mView*mProj;

		XMFLOAT4 CameraPosAndAlphaTest;
		XMStoreFloat4(&CameraPosAndAlphaTest, pCamera->GetEyePt());
		//different alpha test for msaa vs diabled
		CameraPosAndAlphaTest.w = 0.003f;

		//Set the constant buffers
		HRESULT hr;
		D3D11_MAPPED_SUBRESOURCE MappedResource;


		V(pD3dImmediateContext->Map(m_pConstantBufferPerObject, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
		CB_PER_OBJECT* pPerObject = (CB_PER_OBJECT*)MappedResource.pData;
		pPerObject->mWorldViewProjection = XMMatrixTranspose(mWorldViewPrjection);
		pPerObject->mWorld = mWorld;
		pD3dImmediateContext->Unmap(m_pConstantBufferPerObject, 0);
		pD3dImmediateContext->VSSetConstantBuffers(0, 1, &m_pConstantBufferPerObject);
		pD3dImmediateContext->PSSetConstantBuffers(0, 1, &m_pConstantBufferPerObject);
		//	pD3dImmediateContext->CSSetConstantBuffers(0, 1, &g_pConstantBufferPerObject);


		V(pD3dImmediateContext->Map(m_pConstantBufferPerFrame, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
		CB_PER_FRAME* pPerFrame = (CB_PER_FRAME*)MappedResource.pData;
		/////////////////////////////////////////////////////////
		pD3dImmediateContext->Unmap(m_pConstantBufferPerFrame, 0);
		pD3dImmediateContext->VSSetConstantBuffers(1, 1, &m_pConstantBufferPerFrame);
		pD3dImmediateContext->PSSetConstantBuffers(1, 1, &m_pConstantBufferPerFrame);
		//	pD3dImmediateContext->CSSetConstantBuffers(1, 1, &g_pConstantBufferPerFrame);

		pD3dImmediateContext->OMSetRenderTargets(1, &pRTV, pDepthStencilView);
		pD3dImmediateContext->RSSetState(m_pRasterizerState);
		pD3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pD3dImmediateContext->IASetIndexBuffer(m_pMeshIB, DXGI_FORMAT_R32_UINT, 0);
		pD3dImmediateContext->IASetInputLayout(m_pDefaultShaderInputLayout);
		UINT uStride = sizeof(GeometryHelper::Vertex);
		UINT uOffset = 0;
		pD3dImmediateContext->IASetVertexBuffers(0, 1, &m_pMeshVB, &uStride, &uOffset);

		pD3dImmediateContext->VSSetShader(m_pDefaultShaderVS, nullptr, 0);
		//pD3dImmediateContext->VSSetConstantBuffers(0, 1, &g_pConstantBufferPerObject);
		//pD3dImmediateContext->VSSetConstantBuffers(1, 1, &g_pConstantBufferPerFrame);

		pD3dImmediateContext->PSSetShader(m_pDefaultShaderPS, nullptr, 0);
		//pD3dImmediateContext->PSSetConstantBuffers(0, 1, &g_pConstantBufferPerObject);
		//pD3dImmediateContext->PSSetConstantBuffers(1, 1, &g_pConstantBufferPerFrame);


		pD3dImmediateContext->DrawIndexed(m_MeshData.Indices32.size(), 0, 0);


		pD3dImmediateContext->OMSetBlendState(pBlendStateStored11, afBlendFactorStored11, uSampleMaskStored11);
		pD3dImmediateContext->OMSetDepthStencilState(pDepthStencilStateStored11, uStencilRefStored11);
		//RS 

		SAFE_RELEASE(pBlendStateStored11);
		SAFE_RELEASE(pDepthStencilStateStored11);

	}

	HRESULT TriangleRender::CreateOtherRenderStateResources(ID3D11Device * pD3dDevice)
	{
		HRESULT hr;

		hr = BaseForwardRender::CreateOtherRenderStateResources(pD3dDevice);


		D3D11_BLEND_DESC BlendStateDesc;
		BlendStateDesc.AlphaToCoverageEnable = FALSE;
		BlendStateDesc.IndependentBlendEnable = FALSE;
		BlendStateDesc.RenderTarget[0].BlendEnable = FALSE;
		BlendStateDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		BlendStateDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_COLOR;
		BlendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		BlendStateDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		BlendStateDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		BlendStateDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
		BlendStateDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;


		BlendStateDesc.RenderTarget[0].RenderTargetWriteMask = 0;
		V_RETURN(pD3dDevice->CreateBlendState(&BlendStateDesc, &m_pDepthOnlyState));

		BlendStateDesc.AlphaToCoverageEnable = TRUE;
		BlendStateDesc.RenderTarget[0].RenderTargetWriteMask = 0;
		BlendStateDesc.RenderTarget[0].BlendEnable = TRUE;
		V_RETURN(pD3dDevice->CreateBlendState(&BlendStateDesc, &m_pDepthOnlyAndAlphaToCoverageState));

		return hr;
	}

	void TriangleRender::ReleaseOneTimeInitedCOM(void)
	{
		BaseForwardRender::ReleaseOneTimeInitedCOM();

		SAFE_RELEASE(m_pDepthOnlyState);
		SAFE_RELEASE(m_pDepthOnlyAndAlphaToCoverageState);
	}

}
