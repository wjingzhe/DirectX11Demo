#include "BaseForwardRender.h"
using namespace DirectX;

namespace ForwardRender
{
	BaseForwardRender::BaseForwardRender()
		: m_Position4(0.0f, 0.0f, 0.0f, 0.0f), m_pMeshIB(nullptr), m_pMeshVB(nullptr),
		m_pDefaultShaderInputLayout(nullptr), m_pDefaultShaderVS(nullptr), m_pDefaultShaderPS(nullptr),
		m_pConstantBufferPerObject(nullptr), m_pConstantBufferPerFrame(nullptr), m_uSizeConstantBufferPerObject(sizeof(CB_PER_OBJECT)),
		m_uSizeConstantBufferPerFrame(sizeof(CB_PER_FRAME)), m_bShaderInited(false)
	{
	}

	BaseForwardRender::~BaseForwardRender()
	{
		ReleaseAllD3D11COM();
	}

	void BaseForwardRender::GenerateMeshData(float width, float height, float depth, float centerX, float centerY, float centerZ)
	{
		m_MeshData = GeometryHelper::CreateBox(width, height, depth, centerX, centerY, centerZ);
	}
	void BaseForwardRender::CalculateBoundingBoxMinMax(const GeometryHelper::MeshData & meshData, DirectX::XMVECTOR * pBoxMinOut, DirectX::XMVECTOR * pBoxMaxOut)
	{
		auto temp = meshData.Vertices[0].Position;
		*pBoxMinOut = DirectX::XMVectorSet(temp.x, temp.y, temp.z, 1.0f);
		*pBoxMaxOut = DirectX::XMVectorSet(temp.x, temp.y, temp.z, 1.0f);

		for (int i = 1; i < meshData.Vertices.size(); ++i)
		{
			auto temp = meshData.Vertices[i].Position;
			DirectX::XMVECTOR vNew = DirectX::XMVectorSet(temp.x, temp.y, temp.z, 1.0f);
			*pBoxMaxOut = XMVectorMax(*pBoxMaxOut, vNew);
			*pBoxMinOut = XMVectorMin(*pBoxMinOut, vNew);
		}
	}

	void BaseForwardRender::AddShadersToCache(AMD::ShaderCache * pShaderCache)
	{
		const D3D11_INPUT_ELEMENT_DESC layout[] =
		{
			{ "POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0 },
			{ "NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0 },
			{ "TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0 },
			{ "TANGENT",0,DXGI_FORMAT_R32G32B32_FLOAT,0,32,D3D11_INPUT_PER_VERTEX_DATA,0 },
		};

		INT32 size = ARRAYSIZE(layout);

		this->AddShadersToCache(pShaderCache, L"BaseGeometryVS", L"BaseGeometryPS", L"DrawBasicGeometry.hlsl", layout, size);
	}

	HRESULT BaseForwardRender::OnD3DDeviceCreated(ID3D11Device * pD3dDevice, const DXGI_SURFACE_DESC * pBackBufferSurfaceDesc, void * pUserContext)
	{
		HRESULT hr;
		hr = this->CreateCommonBuffers(pD3dDevice, m_MeshData, m_uSizeConstantBufferPerObject, m_uSizeConstantBufferPerFrame);
		hr = this->CreateOtherRenderStateResources(pD3dDevice);

		return hr;
	}

	void BaseForwardRender::OnD3D11DestroyDevice(void * pUserContext)
	{
		ReleaseAllD3D11COM();
	}

	void BaseForwardRender::OnReleasingSwapChain(void)
	{
	}

	HRESULT BaseForwardRender::OnResizedSwapChain(ID3D11Device * pD3dDevice, const DXGI_SURFACE_DESC * pBackBufferSurfaceDesc)
	{
		return S_OK;
	}

	void BaseForwardRender::OnRender(ID3D11Device * pD3dDevice, ID3D11DeviceContext * pD3dImmediateContext, CBaseCamera * pCamera, ID3D11RenderTargetView * pRTV, ID3D11DepthStencilView * pDepthStencilView)
	{
	}

	void BaseForwardRender::AddShadersToCache(AMD::ShaderCache * pShaderCache, const wchar_t * pwsNameVS, const wchar_t * pwsNamePS, const wchar_t * pwsSourceFileName, const D3D11_INPUT_ELEMENT_DESC layout[], UINT size)
	{
		if (!m_bShaderInited)
		{
			SAFE_RELEASE(m_pDefaultShaderInputLayout);
			SAFE_RELEASE(m_pDefaultShaderVS);
			SAFE_RELEASE(m_pDefaultShaderPS);

			//Add vertex shader
			pShaderCache->AddShader((ID3D11DeviceChild**)&m_pDefaultShaderVS, AMD::ShaderCache::SHADER_TYPE::SHADER_TYPE_VERTEX,
				L"vs_5_0", pwsNameVS, pwsSourceFileName, 0, nullptr, &m_pDefaultShaderInputLayout, (D3D11_INPUT_ELEMENT_DESC*)layout, size);

			//Add pixel shader

			pShaderCache->AddShader((ID3D11DeviceChild**)&m_pDefaultShaderPS, AMD::ShaderCache::SHADER_TYPE::SHADER_TYPE_PIXEL,
				L"ps_5_0", pwsNamePS, pwsSourceFileName, 0, nullptr, nullptr, nullptr, 0);

			m_bShaderInited = true;

		}
	}

	HRESULT BaseForwardRender::CreateCommonBuffers(ID3D11Device * pD3dDevice, GeometryHelper::MeshData & meshData, UINT constBufferPerObjectSize, UINT constBufferPerFrameSize)
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
		//DXUT_SetDebugName(m_pMeshIB, "TriangleIB");


		//Create vertex buffer
		D3D11_BUFFER_DESC vertexBufferDesc;
		ZeroMemory(&vertexBufferDesc, sizeof(vertexBufferDesc));
		vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
		vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		vertexBufferDesc.ByteWidth = sizeof(GeometryHelper::Vertex)*m_MeshData.Vertices.size();
		InitData.pSysMem = &m_MeshData.Vertices[0];
		V_RETURN(pD3dDevice->CreateBuffer(&vertexBufferDesc, &InitData, &m_pMeshVB));
		//DXUT_SetDebugName(m_pMeshVB, "TriangleVB");

		//  Create constant buffers
		D3D11_BUFFER_DESC ConstantBufferDesc;
		ZeroMemory(&ConstantBufferDesc, sizeof(ConstantBufferDesc));
		ConstantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		ConstantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		ConstantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		ConstantBufferDesc.ByteWidth = constBufferPerObjectSize > 4 ? constBufferPerObjectSize : 16;
		V_RETURN(pD3dDevice->CreateBuffer(&ConstantBufferDesc, nullptr, &m_pConstantBufferPerObject));

		ConstantBufferDesc.ByteWidth = constBufferPerFrameSize > 4 ? constBufferPerFrameSize : 16;
		V_RETURN(pD3dDevice->CreateBuffer(&ConstantBufferDesc, nullptr, &m_pConstantBufferPerFrame));

		return hr;
	}

	HRESULT BaseForwardRender::CreateOtherRenderStateResources(ID3D11Device * pD3dDevice)
	{
		HRESULT hr;


		// Create depth stencil states
		D3D11_DEPTH_STENCIL_DESC DepthStencilDesc;
		DepthStencilDesc.DepthEnable = TRUE;
		DepthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		DepthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;//jingz leave it as default in Direct3D
		DepthStencilDesc.StencilEnable = TRUE;
		DepthStencilDesc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
		DepthStencilDesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
		DepthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;//jingz todo
		DepthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;//jingz todo stencil test passed but failed in depth tests
		DepthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_INCR;//jingz todo
		DepthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;//jingz todo
																		 //jingz backface usually was culled
		DepthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		DepthStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		DepthStencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		DepthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
		V_RETURN(pD3dDevice->CreateDepthStencilState(&DepthStencilDesc, &m_pDepthStencilState));


		//Create rasterizer states
		D3D11_RASTERIZER_DESC RasterizerDesc;
		RasterizerDesc.FillMode = D3D11_FILL_SOLID;// D3D11_FILL_SOLID;D3D11_FILL_WIREFRAME
		RasterizerDesc.CullMode = D3D11_CULL_BACK;// disable culling
		RasterizerDesc.FrontCounterClockwise = FALSE;
		RasterizerDesc.DepthBias = 0;
		RasterizerDesc.DepthBiasClamp = 0.0f;
		RasterizerDesc.SlopeScaledDepthBias = 0.0f;
		RasterizerDesc.DepthClipEnable = TRUE;
		RasterizerDesc.ScissorEnable = FALSE;
		RasterizerDesc.MultisampleEnable = FALSE;
		RasterizerDesc.AntialiasedLineEnable = FALSE;
		V_RETURN(pD3dDevice->CreateRasterizerState(&RasterizerDesc, &m_pRasterizerState));


		// Create blend states
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
		V_RETURN(pD3dDevice->CreateBlendState(&BlendStateDesc, &m_pBlendState));

		//Create state objects
		D3D11_SAMPLER_DESC SamplerDesc;
		ZeroMemory(&SamplerDesc, sizeof(SamplerDesc));
		SamplerDesc.Filter = D3D11_FILTER_ANISOTROPIC;
		SamplerDesc.AddressU = SamplerDesc.AddressV = SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		SamplerDesc.MaxAnisotropy = 16;
		SamplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
		SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
		V_RETURN(pD3dDevice->CreateSamplerState(&SamplerDesc, &m_pSamplerState));

		return hr;
	}

	void BaseForwardRender::ReleaseAllD3D11COM(void)
	{
		this->ReleaseSwapChainAssociatedCOM();
		this->ReleaseOneTimeInitedCOM();
	}

	void BaseForwardRender::ReleaseSwapChainAssociatedCOM(void)
	{
	}

	void BaseForwardRender::ReleaseOneTimeInitedCOM(void)
	{
		SAFE_RELEASE(m_pMeshIB);
		SAFE_RELEASE(m_pMeshVB);

		SAFE_RELEASE(m_pDefaultShaderInputLayout);
		SAFE_RELEASE(m_pDefaultShaderVS);
		SAFE_RELEASE(m_pDefaultShaderPS);

		SAFE_RELEASE(m_pDepthStencilState);
		SAFE_RELEASE(m_pRasterizerState);
		SAFE_RELEASE(m_pBlendState);
		SAFE_RELEASE(m_pSamplerState);

		SAFE_RELEASE(m_pConstantBufferPerObject);
		SAFE_RELEASE(m_pConstantBufferPerFrame);
	}

	HRESULT BaseForwardRender::CreateSwapChainAssociatedResource(ID3D11Device * pD3dDevice, const DXGI_SURFACE_DESC * pBackBufferSurfaceDesc)
	{
		return S_OK;
	}
}

