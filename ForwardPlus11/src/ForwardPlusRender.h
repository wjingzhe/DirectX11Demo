#pragma once

#include "../../DXUT/Core/DXUT.h"

namespace AMD
{
	class ShaderCache;
}



class CDXUTSDKMesh;

namespace ForwardPlus11
{
	static const int MAX_NUM_LIGHTS = 2 * 1024;



#pragma pack(push,1)
	struct CB_PER_OBJECT
	{
		DirectX::XMMATRIX m_mWorldViewProjection;
		DirectX::XMMATRIX m_mWorldView;
		DirectX::XMMATRIX m_mWorld;
		DirectX::XMVECTOR m_MaterialAmbientColorUp;
		DirectX::XMVECTOR m_MaterialAmbientColorDown;
	};

	struct CB_PER_FRAME
	{
		DirectX::XMMATRIX m_mProjection;
		DirectX::XMMATRIX m_mProjectionInv;
		DirectX::XMVECTOR m_vCameraPosAndAlphaTest;
		unsigned m_uNumLights;
		unsigned m_uWindowWidth;
		unsigned m_uWindowHeight;
		unsigned m_uMaxNumLightsPerTile;
	};
#pragma pack(pop)





	class ForwardPlusRender
	{
	public:
		ForwardPlusRender();
		~ForwardPlusRender();

		static void CalculateSceneMinMax(CDXUTSDKMesh& Mesh, DirectX::XMVECTOR * pBBoxMinOut, DirectX::XMVECTOR* pBBoxMaxOut);
		static void InitRandomLights(const DirectX::XMVECTOR& BBoxMin, const DirectX::XMVECTOR& BBoxMax);

		void AddShaderToCache(AMD::ShaderCache* pShaderCache);

		//various hook functions
		HRESULT OnCreateDevice(ID3D11Device* pD3DDeive, AMD::ShaderCache* pShaderCache, DirectX::XMVECTOR SceneMin, DirectX::XMVECTOR SceneMax);
		void OnDestroyDevice();
		HRESULT OnResizedSwapChain(ID3D11Device* pD3DDevie, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, int nLineHeight);
		void OnReleasingSwapChain();
		void OnRender(float fElaspedTime, unsigned uNumPointLights, unsigned uNumSporLights);

		void ResizeSolution(UINT width,UINT height);

		unsigned GetNumTileX();//X-axis have tile counts
		unsigned GetNumTileY();//Y-axis have tile counts
		unsigned GetMaxNumLightsPerTile();

		ID3D11ShaderResourceView* const * GetPointLightBufferCenterAndRadiusSRV() { return &m_pPointLightBufferCenterAndRadiusSRV; }
		ID3D11ShaderResourceView* const * GetPointLightBufferColorSRV() { return &m_pPointLightBufferColorSRV; }
		ID3D11ShaderResourceView* const * GetSpotLightBufferCenterAndRadiusSRV() { return &m_pSpotLightBufferCenterAndRadiusSRV; }
		ID3D11ShaderResourceView* const * GetSpotLightBufferColorSRV() {return &m_pSpotLightBufferColorSRV;}

		ID3D11ShaderResourceView* const * GetLightIndexBufferSRV() { return &m_pLightIndexBufferSRV; }
		ID3D11UnorderedAccessView* const * GetLightIndexBufferUAV() { return &m_pLightIndexBufferUAV; }


	protected:
		void ReleaseAllD3D11COM(void);


	private:

		//Light culling constants.
		// These must match their counterParts in ForwardPlus11 in ForwardPlus11Common.hlsl
		static const unsigned TILE_RES = 16;
		static const unsigned MAX_NUM_LIGHTS_PER_TILE = 544;

		//Forward rendering render target with and height
		unsigned int m_uWidth;
		unsigned int m_uHeight;

		//Point Lights
		ID3D11Buffer* m_pPointLightBufferCenterAndRadius;
		ID3D11ShaderResourceView* m_pPointLightBufferCenterAndRadiusSRV;
		ID3D11Buffer* m_pPointLightBufferColor;
		ID3D11ShaderResourceView* m_pPointLightBufferColorSRV;

		//Spot lights
		ID3D11Buffer* m_pSpotLightBufferCenterAndRadius;
		ID3D11ShaderResourceView* m_pSpotLightBufferCenterAndRadiusSRV;
		ID3D11Buffer* m_pSpotLightBufferColor;
		ID3D11ShaderResourceView* m_pSpotLightBufferColorSRV;
		ID3D11Buffer* m_pSpotLightBufferParams;
		ID3D11ShaderResourceView* m_pSpotLightBufferParamsSRV;


		//Buffers for light culling
		ID3D11Buffer* m_pLightIndexBuffer;
		ID3D11ShaderResourceView* m_pLightIndexBufferSRV;
		ID3D11UnorderedAccessView* m_pLightIndexBufferUAV;


		//Vertex Shader
		ID3D11VertexShader* m_pScenePositionOnlyVS;
		ID3D11VertexShader* m_pScenePositionAndTextureVS;
		ID3D11VertexShader* m_pSceneMeshVS;
		
		//Pixel Shader
		ID3D11PixelShader* m_pSceneMeshPS;//m_pSceneNoAlphaTestAndLightCullPS;
		ID3D11PixelShader* m_pSceneAlphaTestPS;//m_pSceneAlphaTestAndLightCullPS;
		ID3D11PixelShader* m_pSceneNoCullPS;//m_pSceneNoAlphaTestAndNoLightCullPS;
		ID3D11PixelShader* m_pSceneNoCullAlphaTestPS;//m_pSceneAlphaTestAndNoLightCullPS
		ID3D11PixelShader* m_pSceneAlphaTestOnlyPS;

		// Compute Shader
		ID3D11ComputeShader* m_pLightCullCS;
		ID3D11ComputeShader* m_pLightCullMSAA_CS;
		ID3D11ComputeShader* m_pLightCullNoDepthCS;

		//InputLayout
		ID3D11InputLayout* m_pPositionOnlyInputLayout;
		ID3D11InputLayout* m_pPositionAndTexInputLayout;
		ID3D11InputLayout* m_pMeshInputLayout;

		//SamplerState
		ID3D11SamplerState* m_pSamAnisotropic;

		// Depth buffer data
		ID3D11Texture2D* m_pDepthStencilTexture;
		ID3D11DepthStencilView* m_pDepthStencilView;
		ID3D11ShaderResourceView* m_pDepthStencilSRV;

		//DepthStencilState
		ID3D11DepthStencilState* m_pDepthLess;
		ID3D11DepthStencilState* m_pDepthEqualAndDisableDepthWrite;
		ID3D11DepthStencilState* m_pDisableDepthWrite;
		ID3D11DepthStencilState* m_pDisableDepthTest;

		// Blend States
		ID3D11BlendState* m_pAdditiveBS;
		ID3D11BlendState* m_pOpaqueBS;
		ID3D11BlendState* m_pDepthOnlyBS;
		ID3D11BlendState* m_pDepthOnlyAlphaToCoverageBS;

		//Rasterize states
		ID3D11RasterizerState* m_pDisableCullingRS;

		//Constant Buffer
		ID3D11Buffer* m_pCbPerObject;
		ID3D11Buffer* m_pCbPerFrame;


		// Number of currently active lights
		static const int g_iNumActivePointLights = 2048;
		static const int g_iNumActiveSpotLights = 0;

		bool bFirstPass;
		bool bSolutionSized;
	};

}