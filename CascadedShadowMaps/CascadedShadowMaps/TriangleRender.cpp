#include "stdafx.h"
#include "TriangleRender.h"

TriangleRender::TriangleRender()
	:m_pMeshIB(nullptr), m_pMeshVB(nullptr), m_pPosAndNormalAndTextureInputLayout(nullptr), m_pScenePosAndNormalAndTextureVS(nullptr), m_pScenePosAndNomralAndTexturePS(nullptr)
{
}

TriangleRender::~TriangleRender()
{
	SAFE_RELEASE(m_pPosAndNormalAndTextureInputLayout);

	SAFE_RELEASE(m_pMeshIB);
	SAFE_RELEASE(m_pMeshVB);

	SAFE_RELEASE(m_pScenePosAndNormalAndTextureVS);
	SAFE_RELEASE(m_pScenePosAndNomralAndTexturePS);
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
	//Ensure all shaders(and input layouts) are released
	SAFE_RELEASE(m_pScenePosAndNormalAndTextureVS);
	SAFE_RELEASE(m_pScenePosAndNomralAndTexturePS);
	SAFE_RELEASE(m_pPosAndNormalAndTextureInputLayout);

	//AMD::ShaderCache::Macro ShaderMacros[2];
	//wcscpy_s(ShaderMacros[0].m_wsName, AMD::ShaderCache::m_uMACRO_MAX_LENGTH, L"");
	//wcscpy_s(ShaderMacros[1].m_wsName, AMD::ShaderCache::m_uMACRO_MAX_LENGTH, L"");


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
	DXUT_SetDebugName(m_pMeshIB, "TriangleIB");


	//Create vertex buffer
	D3D11_BUFFER_DESC vertexBufferDesc;
	ZeroMemory(&vertexBufferDesc, sizeof(vertexBufferDesc));
	vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.ByteWidth = sizeof(GeometryHelper::Vertex)*m_MeshData.Vertices.size();
	InitData.pSysMem = &m_MeshData.Vertices[0];
	V_RETURN(pD3dDevice->CreateBuffer(&vertexBufferDesc, &InitData, &m_pMeshVB));
	DXUT_SetDebugName(m_pMeshVB, "TriangleVB");

	return hr;
}

void TriangleRender::OnD3D11DestroyDevice(void * pUserContext)
{
	SAFE_RELEASE(m_pPosAndNormalAndTextureInputLayout);

	SAFE_RELEASE(m_pMeshIB);
	SAFE_RELEASE(m_pMeshVB);

	SAFE_RELEASE(m_pScenePosAndNormalAndTextureVS);
	SAFE_RELEASE(m_pScenePosAndNomralAndTexturePS);
}

void TriangleRender::OnReleasingSwapChain()
{
}

HRESULT TriangleRender::OnResizedSwapChain(ID3D11Device * pD3dDevice, const DXGI_SURFACE_DESC * pBackBufferSurfaceDesc)
{
	return S_OK;
}



void TriangleRender::OnRender(ID3D11Device * pD3dDevice, ID3D11DeviceContext * pD3dImmediateContext, double fTime, float fElapsedTime, void* pUserContext)
{
	//ID3D11RenderTargetView* pRTV = DXUTGetD3D11RenderTargetView();

	// save blend state(for later restore)
	ID3D11BlendState* pBlendStateStored11 = nullptr;
	FLOAT afBlendFactorStored11[4];
	UINT uSampleMaskStored11;
	pD3dImmediateContext->OMGetBlendState(&pBlendStateStored11, afBlendFactorStored11, &uSampleMaskStored11);

	// save depth state (for later restore)
	ID3D11DepthStencilState* pDepthStencilStateStored11 = nullptr;
	UINT uStencilRefStored11;
	pD3dImmediateContext->OMGetDepthStencilState(&pDepthStencilStateStored11, &uStencilRefStored11);


	//Save RS

	pD3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	pD3dImmediateContext->IASetIndexBuffer(m_pMeshIB,DXGI_FORMAT_R32_UINT,0);
	pD3dImmediateContext->IASetInputLayout(m_pPosAndNormalAndTextureInputLayout);
	UINT uStride = sizeof(GeometryHelper::Vertex);
	UINT uOffset = 0;
	pD3dImmediateContext->IASetVertexBuffers(0,1,&m_pMeshVB,&uStride, &uOffset);

	pD3dImmediateContext->VSSetShader(m_pScenePosAndNormalAndTextureVS,nullptr,0);
	//pD3dImmediateContext->VSSetConstantBuffers(0, 1, &g_pConstantBufferPerObject);
	//pD3dImmediateContext->VSSetConstantBuffers(1, 1, &g_pConstantBufferPerFrame);

	pD3dImmediateContext->PSSetShader(m_pScenePosAndNomralAndTexturePS,nullptr,0);
	//pD3dImmediateContext->PSSetConstantBuffers(0, 1, &g_pConstantBufferPerObject);
	//pD3dImmediateContext->PSSetConstantBuffers(1, 1, &g_pConstantBufferPerFrame);





	pD3dImmediateContext->DrawIndexed(m_MeshData.Indices32.size(),0,0);


	pD3dImmediateContext->OMSetBlendState(pBlendStateStored11, afBlendFactorStored11, uSampleMaskStored11);
	pD3dImmediateContext->OMSetDepthStencilState(pDepthStencilStateStored11, uStencilRefStored11);
	//RS 

	SAFE_RELEASE(pBlendStateStored11);
	SAFE_RELEASE(pDepthStencilStateStored11);

	pD3dImmediateContext->Flush();
}
