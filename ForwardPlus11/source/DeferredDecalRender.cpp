#include "DeferredDecalRender.h"
using namespace DirectX;

namespace PostProcess
{
	DeferredDecalRender::DeferredDecalRender()
		:BasePostProcessRender(),
		m_pDecalTextureSRV(nullptr), m_pDepthAlwaysAndStencilOnlyOneTime(nullptr),
		//SamplerState
		m_pSamAnisotropic(nullptr)
	{
	}

	DeferredDecalRender::~DeferredDecalRender()
	{
		ReleaseAllD3D11COM();
	}

	void DeferredDecalRender::GenerateMeshData(float width, float height, float depth, unsigned int numSubdivision)
	{
		m_BoxExtend.x = width;
		m_BoxExtend.y = height;
		m_BoxExtend.z = depth;

		m_MeshData = GeometryHelper::CreateBox(width, height, depth, numSubdivision);
	}

	void DeferredDecalRender::AddShadersToCache(AMD::ShaderCache* pShaderCache)
	{
		if (!m_bShaderInited)
		{
			SAFE_RELEASE(m_pShaderInputLayout);
			SAFE_RELEASE(m_pShaderVS);
			SAFE_RELEASE(m_pShaderPS);


			const D3D11_INPUT_ELEMENT_DESC layout[] =
			{
				{ "POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0 },
				{ "NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0 },
				{ "TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0 },
				{ "TANGENT",0,DXGI_FORMAT_R32G32B32_FLOAT,0,32,D3D11_INPUT_PER_VERTEX_DATA,0 },
			};

			//Add vertex shader
			pShaderCache->AddShader((ID3D11DeviceChild**)&m_pShaderVS, AMD::ShaderCache::SHADER_TYPE::SHADER_TYPE_VERTEX,
				L"vs_5_0", L"DeferredDecalVS", L"DeferredDecal.hlsl", 0, nullptr, &m_pShaderInputLayout, (D3D11_INPUT_ELEMENT_DESC*)layout, ARRAYSIZE(layout));

			//Add pixel shader

			pShaderCache->AddShader((ID3D11DeviceChild**)&m_pShaderPS, AMD::ShaderCache::SHADER_TYPE::SHADER_TYPE_PIXEL,
				L"ps_5_0", L"DeferredDecalPS", L"DeferredDecal.hlsl", 0, nullptr, nullptr, nullptr, 0);

			m_bShaderInited = true;
		}


	}

	HRESULT  DeferredDecalRender::CreateOtherRenderStateResources(ID3D11Device * pD3dDevice)
	{

		HRESULT hr;
		
		hr = BasePostProcessRender::CreateOtherRenderStateResources(pD3dDevice);


		//Create DepthStencilState
		D3D11_DEPTH_STENCIL_DESC DepthStencilDesc;
		DepthStencilDesc.DepthEnable = TRUE;
		DepthStencilDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
		DepthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		DepthStencilDesc.StencilEnable = TRUE;
		DepthStencilDesc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
		DepthStencilDesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
		DepthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_GREATER;
		DepthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		DepthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		DepthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_INCR;
		DepthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_NEVER;
		DepthStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		DepthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		DepthStencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		V_RETURN(pD3dDevice->CreateDepthStencilState(&DepthStencilDesc, &m_pDepthAlwaysAndStencilOnlyOneTime));

		
		//Create state objects
		D3D11_SAMPLER_DESC SamplerDesc;
		ZeroMemory(&SamplerDesc, sizeof(SamplerDesc));
		SamplerDesc.Filter = D3D11_FILTER_ANISOTROPIC;
		SamplerDesc.AddressU = SamplerDesc.AddressV = SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		SamplerDesc.MaxAnisotropy = 16;
		SamplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
		SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
		V_RETURN(pD3dDevice->CreateSamplerState(&SamplerDesc, &m_pSamAnisotropic));
		m_pSamAnisotropic->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("Anisotropic"), "Anisotropic");


		return hr;
	}


	void DeferredDecalRender::OnRender(ID3D11Device * pD3dDevice, ID3D11DeviceContext * pD3dImmediateContext, const DXGI_SURFACE_DESC* pBackBufferDesc,
		CBaseCamera * pCamera, ID3D11RenderTargetView * pRTV, ID3D11DepthStencilView * pDepthStencilView,ID3D11ShaderResourceView* pDepthStencilCopySRV)
	{
		// save Rasterizer State (for later restore)
		ID3D11RasterizerState* pPreRasterizerState = nullptr;
		pD3dImmediateContext->RSGetState(&pPreRasterizerState);

		// save blend state(for later restore)
		ID3D11BlendState* pPreBlendStateStored11 = nullptr;
		FLOAT afBlendFactorStored11[4];
		UINT uSampleMaskStored11;
		pD3dImmediateContext->OMGetBlendState(&pPreBlendStateStored11, afBlendFactorStored11, &uSampleMaskStored11);

		// save depth state (for later restore)
		ID3D11DepthStencilState* pPreDepthStencilStateStored11 = nullptr;
		UINT uStencilRefStored11;
		pD3dImmediateContext->OMGetDepthStencilState(&pPreDepthStencilStateStored11, &uStencilRefStored11);



		XMMATRIX mWorld = XMMatrixIdentity();
		mWorld.r[3].m128_f32[0] = m_Position4.x;
		mWorld.r[3].m128_f32[1] = m_Position4.y;
		mWorld.r[3].m128_f32[2] = m_Position4.z;


		//Get the projection & view matrix from the camera class
		XMMATRIX mView = pCamera->GetViewMatrix();
		XMMATRIX mProj = pCamera->GetProjMatrix();
		XMMATRIX mWorldViewPrjection = mWorld*mView*mProj;

		XMFLOAT4 CameraPosAndAlphaTest;
		XMStoreFloat4(&CameraPosAndAlphaTest, pCamera->GetEyePt());
		//different alpha test for msaa vs diabled
		CameraPosAndAlphaTest.w = 0.05f;

		//Set the constant buffers
		HRESULT hr;
		D3D11_MAPPED_SUBRESOURCE MappedResource;


		XMVECTOR det;
		XMMATRIX mWorldViewInv = XMMatrixInverse(&det, mWorld*mView);

		V(pD3dImmediateContext->Map(m_pConstantBufferPerObject, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
		CB_PER_OBJECT* pPerObject = (CB_PER_OBJECT*)MappedResource.pData;
		pPerObject->mWorldViewProjection = XMMatrixTranspose(mWorldViewPrjection);
		pPerObject->mWorld = XMMatrixTranspose(mWorld);
		pPerObject->mWorldViewInv = XMMatrixTranspose(mWorldViewInv);
		pPerObject->vBoxCenterAndRadius = m_BoxExtend;
		pD3dImmediateContext->Unmap(m_pConstantBufferPerObject, 0);
		pD3dImmediateContext->VSSetConstantBuffers(0, 1, &m_pConstantBufferPerObject);
		pD3dImmediateContext->PSSetConstantBuffers(0, 1, &m_pConstantBufferPerObject);
		//	pD3dImmediateContext->CSSetConstantBuffers(0, 1, &m_pConstantBufferPerObject);

		{
			// we need the inverse proj matrix in the per-tile light culling compute shader
			XMVECTOR det;
			XMMATRIX mInvProj = XMMatrixInverse(&det, mProj);

			float nearZ = pCamera->GetNearClip();
			float farZ = pCamera->GetFarClip();
			float fRange = farZ / (farZ - nearZ);

			float FovRadY = pCamera->GetFOV();
			float tanHalfFovY = std::tanf(FovRadY*0.5f);


			V(pD3dImmediateContext->Map(m_pConstantBufferPerFrame, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
			CB_PER_FRAME* pPerFrame = (CB_PER_FRAME*)MappedResource.pData;
			pPerFrame->vCameraPos3AndAlphaTest = XMLoadFloat4(&CameraPosAndAlphaTest);
			pPerFrame->m_mProjection = XMMatrixTranspose(mProj);
			pPerFrame->m_mProjectionInv = XMMatrixTranspose(mInvProj);

			pPerFrame->ProjParams.x = tanHalfFovY;
			pPerFrame->ProjParams.y = pCamera->GetAspect();
			pPerFrame->ProjParams.z = nearZ;
			pPerFrame->ProjParams.w = fRange;
			pPerFrame->RenderTargetHalfSizeAndFarZ.x = pBackBufferDesc->Width/2.0f;
			pPerFrame->RenderTargetHalfSizeAndFarZ.y = pBackBufferDesc->Height/2.0f;
			pPerFrame->RenderTargetHalfSizeAndFarZ.z = farZ;

			pD3dImmediateContext->Unmap(m_pConstantBufferPerFrame, 0);
			pD3dImmediateContext->VSSetConstantBuffers(1, 1, &m_pConstantBufferPerFrame);
			pD3dImmediateContext->PSSetConstantBuffers(1, 1, &m_pConstantBufferPerFrame);
			//	pD3dImmediateContext->CSSetConstantBuffers(1, 1, &g_pConstantBufferPerFrame);
		}



		pD3dImmediateContext->OMSetRenderTargets(1, &pRTV, pDepthStencilView);
		pD3dImmediateContext->ClearDepthStencilView(pDepthStencilView, D3D11_CLEAR_STENCIL, 1.0f, 0);
		pD3dImmediateContext->OMSetDepthStencilState(m_pDepthAlwaysAndStencilOnlyOneTime, 1);
		float BlendFactor[4] = { 0.0f,0.0f,0.0f,0.0f };
		pD3dImmediateContext->OMSetBlendState(nullptr, BlendFactor, 0xFFFFFFFF);
		pD3dImmediateContext->RSSetState(m_pRasterizerState);

		pD3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pD3dImmediateContext->IASetIndexBuffer(m_pMeshIB, DXGI_FORMAT_R32_UINT, 0);
		pD3dImmediateContext->IASetInputLayout(m_pShaderInputLayout);
		UINT uStride = sizeof(GeometryHelper::Vertex);
		UINT uOffset = 0;
		pD3dImmediateContext->IASetVertexBuffers(0, 1, &m_pMeshVB, &uStride, &uOffset);

		pD3dImmediateContext->VSSetShader(m_pShaderVS, nullptr, 0);
		pD3dImmediateContext->VSSetConstantBuffers(0, 1, &m_pConstantBufferPerObject);
		pD3dImmediateContext->VSSetConstantBuffers(1, 1, &m_pConstantBufferPerFrame);

		pD3dImmediateContext->PSSetShader(m_pShaderPS, nullptr, 0);
		pD3dImmediateContext->PSSetConstantBuffers(0, 1, &m_pConstantBufferPerObject);
		pD3dImmediateContext->PSSetConstantBuffers(1, 1, &m_pConstantBufferPerFrame);
		pD3dImmediateContext->PSSetShaderResources(0, 1, &pDepthStencilCopySRV);
		pD3dImmediateContext->PSSetShaderResources(1, 1, &m_pDecalTextureSRV);
		pD3dImmediateContext->PSSetSamplers(0, 1, &m_pSamAnisotropic);


		pD3dImmediateContext->DrawIndexed(m_MeshData.Indices32.size(), 0, 0);


		pD3dImmediateContext->RSSetState(pPreRasterizerState);
		pD3dImmediateContext->OMSetBlendState(pPreBlendStateStored11, afBlendFactorStored11, uSampleMaskStored11);
		pD3dImmediateContext->OMSetDepthStencilState(pPreDepthStencilStateStored11, uStencilRefStored11);
	
		SAFE_RELEASE(pPreRasterizerState);
		SAFE_RELEASE(pPreBlendStateStored11);
		SAFE_RELEASE(pPreDepthStencilStateStored11);

	}


	void DeferredDecalRender::ReleaseOneTimeInitedCOM(void)
	{

		BasePostProcessRender::ReleaseOneTimeInitedCOM();

		SAFE_RELEASE(m_pDepthAlwaysAndStencilOnlyOneTime);

		SAFE_RELEASE(m_pDecalTextureSRV);
		//SamplerState
		SAFE_RELEASE(m_pSamAnisotropic);
	}
}


