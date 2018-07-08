#include "stdafx.h"
#include "TriangleRender.h"
using namespace DirectX;

namespace Triangle
{
	TriangleRender::TriangleRender()
		:m_pMeshIB(nullptr), m_pMeshVB(nullptr), m_pPosAndNormalAndTextureInputLayout(nullptr),
		m_pScenePosAndNormalAndTextureVS(nullptr), m_pScenePosAndNomralAndTexturePS(nullptr),
		m_pConstantBufferPerObject(nullptr), m_pConstantBufferPerFrame(nullptr)
	{
	}

	TriangleRender::~TriangleRender()
	{
		ReleaseAllD3D11COM();
	}

	void TriangleRender::GenerateMeshData()
	{
		m_MeshData = GeometryHelper::CreateBox(1, 1, 1, 6);
	}

	void TriangleRender::CalculateSceneMinMax(GeometryHelper::MeshData &meshData, DirectX::XMVECTOR * pBBoxMinOut, DirectX::XMVECTOR * pBBoxMaxOut)
	{

		*pBBoxMinOut = DirectX::XMVectorSet(-0.5, -0.5, -0.5, 1.0f);
		*pBBoxMaxOut = DirectX::XMVectorSet(+0.5, +0.5, +0.5, 1.0f);
	}

	void TriangleRender::AddShadersToCache(AMD::ShaderCache& shaderCache)
	{
		SAFE_RELEASE(m_pPosAndNormalAndTextureInputLayout);
		SAFE_RELEASE(m_pScenePosAndNormalAndTextureVS);
		SAFE_RELEASE(m_pScenePosAndNomralAndTexturePS);


		const D3D11_INPUT_ELEMENT_DESC layout[] =
		{
			{ "POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0 },
			{ "NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0 },
			{ "TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0 },
			{ "TANGENT",0,DXGI_FORMAT_R32G32B32_FLOAT,0,32,D3D11_INPUT_PER_VERTEX_DATA,0 },
		};

		//Add vertex shader
		shaderCache.AddShader((ID3D11DeviceChild**)&m_pScenePosAndNormalAndTextureVS, AMD::ShaderCache::SHADER_TYPE::SHADER_TYPE_VERTEX,
			L"vs_5_0", L"BaseGeometryVS", L"DrawBasicGeometry.hlsl", 0, nullptr, &m_pPosAndNormalAndTextureInputLayout, (D3D11_INPUT_ELEMENT_DESC*)layout, ARRAYSIZE(layout));

		//Add pixel shader

		shaderCache.AddShader((ID3D11DeviceChild**)&m_pScenePosAndNomralAndTexturePS, AMD::ShaderCache::SHADER_TYPE::SHADER_TYPE_PIXEL,
			L"ps_5_0", L"BaseGeometryPS", L"DrawBasicGeometry.hlsl", 0, nullptr, nullptr, nullptr, 0);
	}

	HRESULT  TriangleRender::OnD3DDeviceCreated(ID3D11Device * pD3dDevice, const DXGI_SURFACE_DESC * pBackBufferSurfaceDesc, void * pUserContext)
	{
		this->GenerateMeshData();

		HRESULT hr;
		D3D11_SUBRESOURCE_DATA InitData;

		//Create index buffer
		D3D11_BUFFER_DESC indexBufferDesc;
		ZeroMemory(&indexBufferDesc, sizeof(indexBufferDesc));
		indexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
		indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		indexBufferDesc.ByteWidth = sizeof(GeometryHelper::uint32)*m_MeshData.Indices32.size();
		InitData.pSysMem = &m_MeshData.Indices32[0];
		V_RETURN(pD3dDevice->CreateBuffer(&indexBufferDesc, &InitData, &m_pMeshIB));
		m_pMeshIB->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("TriangleIB"), "TriangleIB");


		//Create vertex buffer
		D3D11_BUFFER_DESC vertexBufferDesc;
		ZeroMemory(&vertexBufferDesc, sizeof(vertexBufferDesc));
		vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
		vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		vertexBufferDesc.ByteWidth = sizeof(GeometryHelper::Vertex)*m_MeshData.Vertices.size();
		InitData.pSysMem = &m_MeshData.Vertices[0];
		V_RETURN(pD3dDevice->CreateBuffer(&vertexBufferDesc, &InitData, &m_pMeshVB));
		m_pMeshVB->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("TriangleVB"), "TriangleVB");


		//  Create constant buffers
		D3D11_BUFFER_DESC ConstantBufferDesc;
		ZeroMemory(&ConstantBufferDesc, sizeof(ConstantBufferDesc));
		ConstantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		ConstantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		ConstantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		ConstantBufferDesc.ByteWidth = sizeof(CB_PER_OBJECT);
		V_RETURN(pD3dDevice->CreateBuffer(&ConstantBufferDesc, nullptr, &m_pConstantBufferPerObject));
		m_pConstantBufferPerObject->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("CB_PER_OBJECT"), "CB_PER_OBJECT");

		ConstantBufferDesc.ByteWidth = sizeof(CB_PER_FRAME);
		V_RETURN(pD3dDevice->CreateBuffer(&ConstantBufferDesc, nullptr, &m_pConstantBufferPerFrame));
		m_pConstantBufferPerFrame->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("CB_PER_FRAME"), "CB_PER_FRAME");

		return hr;
	}

	void TriangleRender::OnD3D11DestroyDevice(void * pUserContext)
	{
		ReleaseAllD3D11COM();
	}

	void TriangleRender::OnReleasingSwapChain()
	{
		return;
	}

	HRESULT TriangleRender::OnResizedSwapChain(ID3D11Device * pD3dDevice, const DXGI_SURFACE_DESC * pBackBufferSurfaceDesc)
	{
		return S_OK;
	}

	void TriangleRender::OnRender(ID3D11Device * pD3dDevice, ID3D11DeviceContext * pD3dImmediateContext, CBaseCamera * pCamera, ID3D11RenderTargetView * pRTV, ID3D11DepthStencilView * pDepthStencilView)
	{
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
		pPerObject->mWolrd = mWorld;
		pD3dImmediateContext->Unmap(m_pConstantBufferPerObject, 0);
		pD3dImmediateContext->VSSetConstantBuffers(0, 1, &m_pConstantBufferPerObject);
		pD3dImmediateContext->PSSetConstantBuffers(0, 1, &m_pConstantBufferPerObject);
		//	pD3dImmediateContext->CSSetConstantBuffers(0, 1, &m_pConstantBufferPerObject);


		V(pD3dImmediateContext->Map(m_pConstantBufferPerFrame, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
		CB_PER_FRAME* pPerFrame = (CB_PER_FRAME*)MappedResource.pData;
		/////////////////////////////////////////////////////////
		pD3dImmediateContext->Unmap(m_pConstantBufferPerFrame, 0);
		pD3dImmediateContext->VSSetConstantBuffers(1, 1, &m_pConstantBufferPerFrame);
		pD3dImmediateContext->PSSetConstantBuffers(1, 1, &m_pConstantBufferPerFrame);
		//	pD3dImmediateContext->CSSetConstantBuffers(1, 1, &g_pConstantBufferPerFrame);


		pD3dImmediateContext->OMSetRenderTargets(1, &pRTV, pDepthStencilView);

		pD3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pD3dImmediateContext->IASetIndexBuffer(m_pMeshIB, DXGI_FORMAT_R32_UINT, 0);
		pD3dImmediateContext->IASetInputLayout(m_pPosAndNormalAndTextureInputLayout);
		UINT uStride = sizeof(GeometryHelper::Vertex);
		UINT uOffset = 0;
		pD3dImmediateContext->IASetVertexBuffers(0, 1, &m_pMeshVB, &uStride, &uOffset);

		pD3dImmediateContext->VSSetShader(m_pScenePosAndNormalAndTextureVS, nullptr, 0);
		//pD3dImmediateContext->VSSetConstantBuffers(0, 1, &g_pConstantBufferPerObject);
		//pD3dImmediateContext->VSSetConstantBuffers(1, 1, &g_pConstantBufferPerFrame);

		pD3dImmediateContext->PSSetShader(m_pScenePosAndNomralAndTexturePS, nullptr, 0);
		//pD3dImmediateContext->PSSetConstantBuffers(0, 1, &g_pConstantBufferPerObject);
		//pD3dImmediateContext->PSSetConstantBuffers(1, 1, &g_pConstantBufferPerFrame);

		pD3dImmediateContext->DrawIndexed(m_MeshData.Indices32.size(), 0, 0);

	}

	void Triangle::TriangleRender::ReleaseAllD3D11COM(void)
	{
		SAFE_RELEASE(m_pPosAndNormalAndTextureInputLayout);

		SAFE_RELEASE(m_pMeshIB);
		SAFE_RELEASE(m_pMeshVB);

		SAFE_RELEASE(m_pScenePosAndNormalAndTextureVS);
		SAFE_RELEASE(m_pScenePosAndNomralAndTexturePS);

		SAFE_RELEASE(m_pConstantBufferPerObject);
		SAFE_RELEASE(m_pConstantBufferPerFrame);
	}
}

