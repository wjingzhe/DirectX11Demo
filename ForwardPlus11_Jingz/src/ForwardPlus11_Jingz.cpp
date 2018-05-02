//
// Copyright (c) 2016 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

//--------------------------------------------------------------------------------------
// File: ForwardPlus11.cpp
//
// Implements the Forward+ algorithm.
//--------------------------------------------------------------------------------------

// DXUT now sits one directory up
#include "../../DXUT/Core/DXUT.h"
#include "../../DXUT/Core/DXUTmisc.h"
#include "../../DXUT/Optional/DXUTgui.h"
#include "../../DXUT/Optional/DXUTcamera.h"
#include "../../DXUT/Optional/DXUTsettingsdlg.h"
#include "../../DXUT/Optional/SDKmisc.h"
#include "../../DXUT/Optional/SDKmesh.h"

// AMD SDK also sits one directory up
#include "../../amd_sdk/inc/AMD_SDK.h"

//Project includes
#include "resource.h"

#include "ForwardPlusUtil.h"

//-------------------------------------------------------------------------
// ForwardPlus11_Jingz.cpp : Defines the entry point for the application.
//
// Implements the Forward+ algorithm.
//


#pragma warning (disable:4100) // disable unreference formal parammeter warning for /W4 builds

using namespace DirectX;
using namespace ForwardPlus11;



//-------------------------------------------------------------------
// Constants
//-------------------------------------------------------------------
static const int TEXT_LINE_HEIGHT = 15;

//------------------------------------------------------------------
// Global variables
//------------------------------------------------------------------
CFirstPersonCamera g_Camera; // A first-person camera
CDXUTDialogResourceManager g_DialogResourceManager; //Manager for shared resources of dialogs
CD3DSettingsDlg g_SettingsDlg; // Device settings dialog
CDXUTTextHelper* g_pTextHelper = nullptr;


// Direct3D 11 resources
CDXUTSDKMesh g_SceneMesh;
CDXUTSDKMesh g_AlphaMesh;
ID3D11VertexShader* g_pScenePositionOnlyVS = nullptr;
ID3D11VertexShader* g_pScenePositionAndTextureVS = nullptr;
ID3D11VertexShader* g_pSceneVS = nullptr;
ID3D11PixelShader* g_pScenePS = nullptr;
ID3D11PixelShader* g_pScenePSAlphaTest = nullptr;
ID3D11PixelShader* g_pScenePSNoCull = nullptr;
ID3D11PixelShader* g_pScenePSNoCullAlphaTest = nullptr;
ID3D11PixelShader* g_pScenePSAlphaTestOnly = nullptr;

ID3D11PixelShader* g_pDebugDrawNumLightsPerTileRadarColorsPS = nullptr;
ID3D11PixelShader* g_pDebugDrawNumLightsPerTileGrayscalePS = nullptr;

// CS
ID3D11ComputeShader* g_pLightCullCS = nullptr;
ID3D11ComputeShader* g_pLightCullCSMSAA = nullptr;
ID3D11ComputeShader* g_pLightCullCSNoDepth = nullptr;

ID3D11InputLayout* g_pLayoutPositionOnly11 = nullptr;
ID3D11InputLayout* g_pLayoutPositionAndTex11 = nullptr;
ID3D11InputLayout* g_pLayout11 = nullptr;
ID3D11SamplerState* g_pSamLinear = nullptr;

// depth buffer data
ID3D11Texture2D* g_pDepthStencilTexture = nullptr;
ID3D11DepthStencilView* g_pDepthStencilView = nullptr;
ID3D11ShaderResourceView* g_pDepthStencilSRV = nullptr;
ID3D11DepthStencilState* g_pDepthGreater = nullptr;
ID3D11DepthStencilState* g_pDepthEqualAndDisableDepthWrite = nullptr;

// Blend states
ID3D11BlendState* g_pOpaqueState = nullptr;
ID3D11BlendState* g_pDepthOnlyAlphaTestState = nullptr;
ID3D11BlendState* g_pDepthOnlyAlphaToCoverageState = nullptr;

// Rasterize states
ID3D11RasterizerState* g_pDisableCullingRS = nullptr;

// Number of currently active lights
static int g_iNumActivePointLights = 2048;
static int g_iNumActiveSpotLights = 0;

// The max distance the camera can travel
static float g_fMaxDistance = 500.0f;

//---------------------------------------------------------------
// Constance buffers
//---------------------------------------------------------------
#pragma pack(push,1)
struct CB_PER_OBJECT
{
	XMMATRIX m_mWorldViewProjection;
	XMMATRIX m_mWorldView;
	XMMATRIX m_mWorld;
	XMVECTOR m_MaterialAmbientColorUp;
	XMVECTOR m_MaterialAmbientColorDown;
};

struct CB_PER_FRAME
{
	XMMATRIX m_mProjection;
	XMMATRIX m_mProjectionInv;
	XMVECTOR m_vCameraPosAndAlphaTest;
	unsigned m_uNumLights;
	unsigned m_uWindowWidth;
	unsigned m_uWindowHeight;
	unsigned m_uMaxNumLightsPerTile;
};
#pragma pack(pop)


ID3D11Buffer* g_pcbPerObject11 = nullptr;
ID3D11Buffer* g_pcbPerFrame11 = nullptr;


//---------------------------------------------------------------------
// AMD helper classes defined here
//---------------------------------------------------------------------

static AMD::ShaderCache g_ShaderCache;
static AMD::MagnifyTool g_MagnifyTool;
static AMD::HUD g_HUD;

// Global boolean for HUD rendering
bool g_bRenderHUD = true;

static ForwardPlusUtil g_Util;

//----------------------------------------------------------------------
// UI control IDs
//---------------------------------------------------------------------
enum 
{
	IDC_BUTTON_TOGGLE_FULLSCREEN = 1,
	IDC_BUTTON_TOGGLE_REF,
	IDC_BUTTON_CHANGEDEVICE,
	IDC_CHECKBOX_ENABLE_LIGHT_DRAWING,
	IDC_STATIC_NUM_POINT_LIGHTS,
	IDC_SLIDER_NUM_POINT_LIGHTS,
	IDC_STATIC_NUM_SPOT_LIGHTS,
	IDC_SLIDER_NUM_SPOT_LIGHTS,
	IDC_CHECKBOX_ENABLE_LIGHT_CULLING,
	IDC_CHECKBOX_ENABLE_DEPTH_BOUNDS,
	IDC_CHECKBOX_ENABLE_DEBUG_DRAWING,
	IDC_RADIOBUTTON_DEBUG_DRAWING_ONE,
	IDC_RADIOBUTTON_DEBUG_DRAWING_TWO,
	IDC_TILE_DRAWING_GROUP,
	IDC_NUM_CONTROL_IDS
};



//---------------------------------------------------------------------------------------------
// Forward declarations
//---------------------------------------------------------------------------------------------

LRESULT CALLBACK MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
	void* pUserContext);

void CALLBACK OnKeyboard(UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext);
void CALLBACK OnFrameMove(double fTime, float fElapsedTime,void* pUserContext);
bool CALLBACK ModifyDeviceSettings(DXUTDeviceSettings* pDeviceSettings, void* pUserContext);

bool CALLBACK IsD3D11DeviceAcceptable(const CD3D11EnumAdapterInfo* AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo* DeviceInfo,
	DXGI_FORMAT BackBufferFormat, bool bWindowed, void*pUserContext);
HRESULT CALLBACK OnD3D11CreateDevice(ID3D11Device* pD3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
	void* pUserContext);

HRESULT CALLBACK OnD3D11ResizedSwapChain(ID3D11Device* pD3dDevice, IDXGISwapChain* pSwapChain, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext);
void CALLBACK OnD3D11ReleasingSwapChain(void* pUserContext);
void CALLBACK OnD3D11DestroyDevice(void* pUserContext);
void CALLBACK OnD3D11FrameRender(ID3D11Device* pD3dDevice, ID3D11DeviceContext* pD3dImmediateContext, double fTime, float fElapsedTime, void* pUserContext);

void CALLBACK OnGUIEvent(UINT nEvent,int nControlID,CDXUTControl*pControl, void* pUserContext);

HRESULT AddShadersToCache();

void RenderText();

void InitApp();


int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);


	// Enable run-time memory check for debug 12
#if defined(DEBUG)|| defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif //  defined(DEBUG)|| defined(_DEBUG)


	//Set DXUT callbacks
	DXUTSetCallbackMsgProc(MsgProc);
	DXUTSetCallbackKeyboard(OnKeyboard);
	DXUTSetCallbackFrameMove(OnFrameMove);
	DXUTSetCallbackDeviceChanging(ModifyDeviceSettings);

	DXUTSetCallbackD3D11DeviceAcceptable(IsD3D11DeviceAcceptable);
	DXUTSetCallbackD3D11DeviceCreated(OnD3D11CreateDevice);
	DXUTSetCallbackD3D11SwapChainResized(OnD3D11ResizedSwapChain);
	DXUTSetCallbackD3D11SwapChainReleasing(OnD3D11ReleasingSwapChain);
	DXUTSetCallbackD3D11DeviceDestroyed(OnD3D11DestroyDevice);
	DXUTSetCallbackD3D11FrameRender(OnD3D11FrameRender);


	InitApp();
	DXUTInit(true, true, NULL); //Parse the command line,show msgBoxes on error,no extra command line params
	DXUTSetCursorSettings(true, true);
	DXUTCreateWindow(L"ForwardPlus11  v1.2");

	// Require D3D_FEATURE_LEVEL_11_0
	DXUTCreateDevice(D3D_FEATURE_LEVEL_11_0, true, 1920, 1080);

	DXUTMainLoop();//Enter into the DXUT render loop;


	// Emsure the shaderCache aborts if in a lengthy generation process
	g_ShaderCache.Abort();

	return DXUTGetExitCode();

}



//-----------------------------------------------------------------------------------------
// Handle message to the application
//-----------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
	void* pUserContext)
{
	switch (uMsg)
	{
	case WM_GETMINMAXINFO:
		// override DXUT_MIN_WINDOW_SIZE_X and DXUT_MIN_WINDOW_SIZE_Y
		// to prevent windows that are to small
		((MINMAXINFO*)lParam)->ptMinTrackSize.x = 512;
		((MINMAXINFO*)lParam)->ptMinTrackSize.y = 512;
		*pbNoFurtherProcessing = true;
		break;
	}

	if (*pbNoFurtherProcessing)
	{
		return 0;
	}

	// Pase message to dialog resource manager calls so GUI state is updated correctly
	*pbNoFurtherProcessing = g_DialogResourceManager.MsgProc(hWnd, uMsg, wParam, lParam);
	if (*pbNoFurtherProcessing)
	{
		return 0;
	}

	// Pass message to setting dialog if its active
	if (g_SettingsDlg.IsActive())
	{
		g_SettingsDlg.MsgProc(hWnd, uMsg, wParam, lParam);
		return 0;
	}

	// Give the diablog a chance to handle the message first
	*pbNoFurtherProcessing = g_HUD.m_GUI.MsgProc(hWnd, uMsg, wParam, lParam);
	if (*pbNoFurtherProcessing)
	{
		return 0;
	}

	//Pass all remaining windows message to camera so it can respond to user input
	g_Camera.HandleMessages(hWnd, uMsg, wParam, lParam);

	return 0;
}

void CALLBACK OnKeyboard(UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext)
{
	if (bKeyDown)
	{
		switch (nChar)
		{
		case VK_F1:
			g_bRenderHUD = g_bRenderHUD;
			break;
		}
	}
}

void CALLBACK OnFrameMove(double fTime, float fElapsedTime, void* pUserContext)
{
	g_Camera.FrameMove(fElapsedTime);
}

//-----------------------------------------------------------------------------------------
// Called right before creating a D3D11 device,allowing the app to modify the device settings as needed
//-------------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings(DXUTDeviceSettings* pDeviceSettings, void* pUserContext)
{
	static bool s_bFirstTime = true;
	if (s_bFirstTime)
	{
		s_bFirstTime = false;

		// For the first device created if its a REF device,optionally display a warning dialog box
		if (pDeviceSettings->d3d11.DriverType == D3D_DRIVER_TYPE_REFERENCE)
		{
			DXUTDisplaySwitchingToREFWarning();
		}
		// Since a major point of this sample is that MSAA is easy for Forward+
		// and that Forward+ uses the hardware MSAA as intended,then default to 4x MSAA
		else
		{
			pDeviceSettings->d3d11.sd.SampleDesc.Count = 4;
		}

		//Start with sync disabled
		pDeviceSettings->d3d11.SyncInterval = 0;
	}

	// Muti-sample quality is always zero
	pDeviceSettings->d3d11.sd.SampleDesc.Quality = 0;

	//Don't auto create a depth buffer,as this sample requires a depth buffer be created
	// such that it's bindable as a shader resource
	pDeviceSettings->d3d11.AutoCreateDepthStencil = false;

	return true;
}

//-----------------------------------------------------------------------------------------
// Reject any D3D11 devices that aren;t acceptable by returning false
//-----------------------------------------------------------------------------------------
bool CALLBACK IsD3D11DeviceAcceptable(const CD3D11EnumAdapterInfo* AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo* DeviceInfo,
	DXGI_FORMAT BackBufferFormat, bool bWindowed, void*pUserContext)
{
	return true;
}

//----------------------------------------------------------------------------------------
// Create any D3D11 resource that aren't dependent on the back buffer
//----------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11CreateDevice(ID3D11Device* pD3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
	void* pUserContext)
{
	HRESULT hr;

	XMVECTOR SceneMin, SceneMax;

	ID3D11DeviceContext* pD3dImmediateContext = DXUTGetD3D11DeviceContext();
	V_RETURN(g_DialogResourceManager.OnD3D11CreateDevice(pD3dDevice, pD3dImmediateContext));
	V_RETURN(g_SettingsDlg.OnD3D11CreateDevice(pD3dDevice));
	g_pTextHelper = new CDXUTTextHelper(pD3dDevice, pD3dImmediateContext, &g_DialogResourceManager, TEXT_LINE_HEIGHT);

	// Load the scene mesh
	g_SceneMesh.Create(pD3dDevice, L"sponza\\sponza.sdkmesh", false);
	g_Util.CalculateSceneMinMax(g_SceneMesh, &SceneMin, &SceneMax);

	// Load the alpha-test mesh
	g_AlphaMesh.Create(pD3dDevice, L"sponza\\sponza_alpha.sdkmesh", false);

	// Create state objects
	D3D11_SAMPLER_DESC SamplerDesc;
	ZeroMemory(&SamplerDesc, sizeof(SamplerDesc));
	SamplerDesc.Filter = D3D11_FILTER_ANISOTROPIC;
	SamplerDesc.AddressU = SamplerDesc.AddressV = SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	SamplerDesc.MaxAnisotropy = 16;
	SamplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
	V_RETURN(pD3dDevice->CreateSamplerState(&SamplerDesc, &g_pSamLinear));
	DXUT_SetDebugName(g_pSamLinear, "Linear");

	// Create constant buffers
	D3D11_BUFFER_DESC CbDesc;
	ZeroMemory(&CbDesc, sizeof(CbDesc));
	CbDesc.Usage = D3D11_USAGE_DYNAMIC;
	CbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	CbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	
	CbDesc.ByteWidth = sizeof(CB_PER_OBJECT);
	V_RETURN(pD3dDevice->CreateBuffer(&CbDesc, nullptr, &g_pcbPerObject11));
	DXUT_SetDebugName(g_pcbPerObject11, "CB_PER_OBJECT");

	CbDesc.ByteWidth = sizeof(CB_PER_FRAME);
	V_RETURN(pD3dDevice->CreateBuffer(&CbDesc, nullptr, &g_pcbPerFrame11));
	DXUT_SetDebugName(g_pcbPerFrame11, "CB_PER_FRAME");

	// Create blend states
	D3D11_BLEND_DESC BlendStateDesc;
	BlendStateDesc.AlphaToCoverageEnable = FALSE;
	BlendStateDesc.IndependentBlendEnable = FALSE;
	BlendStateDesc.RenderTarget[0].BlendEnable = FALSE;
	BlendStateDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	BlendStateDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	BlendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
	BlendStateDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	BlendStateDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	BlendStateDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	BlendStateDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	V_RETURN(pD3dDevice->CreateBlendState(&BlendStateDesc, &g_pOpaqueState));

	BlendStateDesc.RenderTarget[0].RenderTargetWriteMask = 0;
	V_RETURN(pD3dDevice->CreateBlendState(&BlendStateDesc, &g_pDepthOnlyAlphaTestState));

	BlendStateDesc.RenderTarget[0].RenderTargetWriteMask = 0;
	BlendStateDesc.AlphaToCoverageEnable = TRUE;
	V_RETURN(pD3dDevice->CreateBlendState(&BlendStateDesc, &g_pDepthOnlyAlphaToCoverageState));

	// Create rasterizer states
	D3D11_RASTERIZER_DESC RasterizerDesc;
	RasterizerDesc.FillMode = D3D11_FILL_SOLID;
	RasterizerDesc.CullMode = D3D11_CULL_NONE;// disable culling
	RasterizerDesc.FrontCounterClockwise = FALSE;
	RasterizerDesc.DepthBias = 0;
	RasterizerDesc.DepthBiasClamp = 0.0f;
	RasterizerDesc.SlopeScaledDepthBias = 0.0f;
	RasterizerDesc.DepthClipEnable = TRUE;
	RasterizerDesc.ScissorEnable = FALSE;
	RasterizerDesc.MultisampleEnable = FALSE;
	RasterizerDesc.AntialiasedLineEnable = FALSE;
	V_RETURN(pD3dDevice->CreateRasterizerState(&RasterizerDesc, &g_pDisableCullingRS));

	// Create depth states
	D3D11_DEPTH_STENCIL_DESC DepthStencilDesc;
	DepthStencilDesc.DepthEnable = TRUE;
	DepthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	DepthStencilDesc.DepthFunc = D3D11_COMPARISON_GREATER; // we are using inverted 32-bit float depth better precision
	DepthStencilDesc.StencilEnable = FALSE;
	DepthStencilDesc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
	DepthStencilDesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
	V_RETURN(pD3dDevice->CreateDepthStencilState(&DepthStencilDesc, &g_pDepthGreater));

	DepthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	DepthStencilDesc.DepthFunc = D3D11_COMPARISON_EQUAL;
	V_RETURN(pD3dDevice->CreateDepthStencilState(&DepthStencilDesc, &g_pDepthEqualAndDisableDepthWrite));

	// Create AMD_SDK resource here
	g_HUD.OnCreateDevice(pD3dDevice);
	g_MagnifyTool.OnCreateDevice(pD3dDevice);
	TIMER_Init(pD3dDevice);

	static bool bFirstPass = true;

	// One-time setup
	if (bFirstPass)
	{
		// Setup the camera's view parameters
		XMVECTOR SceneCenter = 0.5f*(SceneMax + SceneMin);
		XMVECTOR SceneExtents = 0.5f*(SceneMax - SceneMin);
		XMVECTOR BoundaryMin = SceneCenter - 2.0f*SceneExtents;
		XMVECTOR BoundaryMax = SceneCenter + 2.0f*SceneExtents;
		XMVECTOR BoundaryDiff = 4.0f*SceneExtents; //BoundaryMax - BoundaryMin;

		g_fMaxDistance = XMVectorGetX(XMVector3Length(BoundaryDiff));
		XMVECTOR vEye = SceneCenter - XMVectorSet(0.67f*XMVectorGetX(SceneExtents), 0.5f*XMVectorGetY(SceneExtents), 0.0f, 0.0f);
		XMVECTOR vAt = SceneCenter - XMVectorSet(0.0f, 0.5f*XMVectorGetY(SceneExtents), 0.0f, 0.0f);
		g_Camera.SetRotateButtons(true, false, false);
		g_Camera.SetEnablePositionMovement(true);
		g_Camera.SetViewParams(vEye, vAt);
		g_Camera.SetScalers(0.005f, 0.1f*g_fMaxDistance);

		XMFLOAT3 vBoundaryMin, vBoundaryMax;
		XMStoreFloat3(&vBoundaryMin, BoundaryMin);
		XMStoreFloat3(&vBoundaryMax, BoundaryMax);
		g_Camera.SetClipToBoundary(true, &vBoundaryMin, &vBoundaryMax);

		// Init light buffer data;
		ForwardPlusUtil::InitLights(SceneMin, SceneMax);

	}

	// Create helper resource here
	g_Util.OnCreateDevice(pD3dDevice);

	// Generate shaders (this is an async operation - call AMD::ShaderCache::ShadersReady() to find out if they are complete)
	if (bFirstPass)
	{
		// Add the applications shaders to the cache
		AddShadersToCache();
		g_ShaderCache.GenerateShaders(AMD::ShaderCache::CREATE_TYPE_COMPILE_CHANGES);// Only compile shaders that have changed(development mode)
		bFirstPass = false;
	}

	return S_OK;
}

HRESULT AddShadersToCache()
{
	HRESULT hr = E_FAIL;

	//Ensure all shaders (and input layouts) are released
	SAFE_RELEASE(g_pScenePositionOnlyVS);
	SAFE_RELEASE(g_pScenePositionAndTextureVS);
	SAFE_RELEASE(g_pSceneVS);
	SAFE_RELEASE(g_pScenePS);
	SAFE_RELEASE(g_pScenePSAlphaTest);
	SAFE_RELEASE(g_pScenePSNoCull);
	SAFE_RELEASE(g_pScenePSNoCullAlphaTest);
	SAFE_RELEASE(g_pScenePSAlphaTestOnly);
	SAFE_RELEASE(g_pDebugDrawNumLightsPerTileRadarColorsPS);
	SAFE_RELEASE(g_pDebugDrawNumLightsPerTileGrayscalePS);
	SAFE_RELEASE(g_pLightCullCS);
	SAFE_RELEASE(g_pLightCullCSMSAA);
	SAFE_RELEASE(g_pLightCullCSNoDepth);
	
	SAFE_RELEASE(g_pLayoutPositionOnly11);
	SAFE_RELEASE(g_pLayoutPositionAndTex11);
	SAFE_RELEASE(g_pLayout11);

	AMD::ShaderCache::Macro ShaderMacros[2];
	wcscpy_s(ShaderMacros[0].m_wsName, AMD::ShaderCache::m_uMACRO_MAX_LENGTH, L"USE_ALPHA_TEST");
	wcscpy_s(ShaderMacros[1].m_wsName, AMD::ShaderCache::m_uMACRO_MAX_LENGTH, L"USE_LIGHT_CULLING");

	AMD::ShaderCache::Macro ShaderMacroUseDepthBounds;
	wcscpy_s(ShaderMacroUseDepthBounds.m_wsName, AMD::ShaderCache::m_uMACRO_MAX_LENGTH, L"USE_DEPTH_BOUNDS");

	const D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
		{"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0},
		{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0},
		{"TANGENT",0,DXGI_FORMAT_R32G32B32_FLOAT,0,32,D3D11_INPUT_PER_VERTEX_DATA,0}
	};

	g_ShaderCache.AddShader((ID3D11DeviceChild**)&g_pScenePositionOnlyVS, AMD::ShaderCache::SHADER_TYPE_VERTEX, L"vs_5_0", L"RenderScenePositionOnlyVS",
		L"ForwardPlus11.hlsl", 0, nullptr, &g_pLayoutPositionOnly11, (D3D11_INPUT_ELEMENT_DESC*)layout, ARRAYSIZE(layout));

	g_ShaderCache.AddShader((ID3D11DeviceChild**)&g_pScenePositionAndTextureVS, AMD::ShaderCache::SHADER_TYPE_VERTEX, L"vs_5_0", L"RenderScenePositionAndTexVS",
		L"ForwardPlus11.hlsl", 0, nullptr, &g_pLayoutPositionAndTex11, (D3D11_INPUT_ELEMENT_DESC*)layout, ARRAYSIZE(layout));

	g_ShaderCache.AddShader((ID3D11DeviceChild**)&g_pSceneVS, AMD::ShaderCache::SHADER_TYPE_VERTEX, L"vs_5_0", L"RenderSceneVS",
		L"ForwardPlus11.hlsl", 0, nullptr, &g_pLayout11, (D3D11_INPUT_ELEMENT_DESC*)layout, ARRAYSIZE(layout));

	ShaderMacros[0].m_iValue = 0;
	ShaderMacros[1].m_iValue = 1;
	g_ShaderCache.AddShader((ID3D11DeviceChild**)&g_pScenePS, AMD::ShaderCache::SHADER_TYPE_PIXEL, L"ps_5_0", L"RenderScenePS", L"ForwardPlus11.hlsl", 2, ShaderMacros, nullptr, nullptr, 0);

	ShaderMacros[0].m_iValue = 1;
	ShaderMacros[1].m_iValue = 1;
	g_ShaderCache.AddShader((ID3D11DeviceChild**)&g_pScenePSAlphaTest, AMD::ShaderCache::SHADER_TYPE_PIXEL, L"ps_5_0", L"RenderScenePS", L"ForwardPlus11.hlsl", 2, ShaderMacros, nullptr, nullptr, 0);

	ShaderMacros[0].m_iValue = 0;
	ShaderMacros[1].m_iValue = 0;
	g_ShaderCache.AddShader((ID3D11DeviceChild**)&g_pScenePSNoCull, AMD::ShaderCache::SHADER_TYPE_PIXEL, L"ps_5_0", L"RenderScenePS", L"ForwardPlus11.hlsl", 2, ShaderMacros, nullptr, nullptr, 0);

	ShaderMacros[0].m_iValue = 1;
	ShaderMacros[1].m_iValue = 0;
	g_ShaderCache.AddShader((ID3D11DeviceChild**)&g_pScenePSNoCullAlphaTest, AMD::ShaderCache::SHADER_TYPE_PIXEL, L"ps_5_0",L"RenderScenePS", L"ForwardPlus11.hlsl", 2, ShaderMacros, nullptr, nullptr, 0);

	g_ShaderCache.AddShader((ID3D11DeviceChild**)&g_pScenePSAlphaTestOnly, AMD::ShaderCache::SHADER_TYPE_PIXEL, L"ps_5_0", L"RenderSceneAlphaTestOnlyPS",
		L"ForwardPlus11.hlsl", 0, nullptr, nullptr, nullptr, 0);

	g_ShaderCache.AddShader((ID3D11DeviceChild**)&g_pDebugDrawNumLightsPerTileRadarColorsPS, AMD::ShaderCache::SHADER_TYPE_PIXEL, L"ps_5_0", L"DebugDrawNumLightsPerTileRadarColorsPS",
		L"ForwardPlus11DebugDraw.hlsl", 0, nullptr, nullptr, nullptr, 0);

	g_ShaderCache.AddShader((ID3D11DeviceChild**)&g_pDebugDrawNumLightsPerTileGrayscalePS, AMD::ShaderCache::SHADER_TYPE_PIXEL, L"ps_5_0", L"DebugDrawNumLightsPerTileGrayscalePS", 
		L"ForwardPlus11DebugDraw.hlsl", 0, nullptr, nullptr, nullptr, 0);

	ShaderMacroUseDepthBounds.m_iValue = 1;
	g_ShaderCache.AddShader((ID3D11DeviceChild**)&g_pLightCullCS, AMD::ShaderCache::SHADER_TYPE_COMPUTE, L"cs_5_0", L"CullLightsCS",L"ForwardPlus11Tiling.hlsl", 1, &ShaderMacroUseDepthBounds, nullptr, nullptr, 0);

	ShaderMacroUseDepthBounds.m_iValue = 2;
	g_ShaderCache.AddShader((ID3D11DeviceChild**)&g_pLightCullCSMSAA, AMD::ShaderCache::SHADER_TYPE_COMPUTE, L"cs_5_0", L"CullLightsCS",L"ForwardPlus11Tiling.hlsl", 1, &ShaderMacroUseDepthBounds, nullptr, nullptr, 0);

	ShaderMacroUseDepthBounds.m_iValue = 0;
	g_ShaderCache.AddShader((ID3D11DeviceChild**)&g_pLightCullCSNoDepth, AMD::ShaderCache::SHADER_TYPE_COMPUTE, L"cs_5_0", L"CullLightsCS",L"ForwardPlus11Tiling.hlsl", 1, &ShaderMacroUseDepthBounds, nullptr, nullptr, 0);

	g_Util.AddShadersToCache(&g_ShaderCache);

	return hr;
}

//-------------------------------------------------------------------------------------
// Create any D3D11 resources that depend on the back buffer
//-------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11ResizedSwapChain(ID3D11Device* pD3dDevice, IDXGISwapChain* pSwapChain, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext)
{
	HRESULT hr;

	V_RETURN(g_DialogResourceManager.OnD3D11ResizedSwapChain(pD3dDevice, pBackBufferSurfaceDesc));
	V_RETURN(g_SettingsDlg.OnD3D11ResizedSwapChain(pD3dDevice, pBackBufferSurfaceDesc));

	// Setup the camera's projection parameters
	// Note,we are using inverted 32-bit float depth for better precision,
	// so reverse near and far below
	// 数据进行了倒数变化
	float fAspectRatio = pBackBufferSurfaceDesc->Width / (FLOAT)pBackBufferSurfaceDesc->Height;
	g_Camera.SetProjParams(XM_PI / 4, fAspectRatio, g_fMaxDistance, 0.1f);

	// Set the location and size of the AMD standard HUD
	g_HUD.m_GUI.SetLocation(pBackBufferSurfaceDesc->Width - AMD::HUD::iDialogWidth, 0);
	g_HUD.m_GUI.SetSize(AMD::HUD::iDialogWidth, pBackBufferSurfaceDesc->Height);
	g_HUD.OnResizedSwapChain(pBackBufferSurfaceDesc);

	//Create our own depth stencil surface that's bindable as a shader
	V_RETURN(AMD::CreateDepthStencilSurface(&g_pDepthStencilTexture, &g_pDepthStencilSRV, &g_pDepthStencilView, 
		DXGI_FORMAT_D32_FLOAT, DXGI_FORMAT_R32_FLOAT, pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height, pBackBufferSurfaceDesc->SampleDesc.Count));

	// Magnify tool will capture from the color buffer
	g_MagnifyTool.OnResizedSwapChain(pD3dDevice, pSwapChain, pBackBufferSurfaceDesc, pUserContext,
		pBackBufferSurfaceDesc->Width - AMD::HUD::iDialogWidth, 0);
	D3D11_RENDER_TARGET_VIEW_DESC RtDesc;
	ID3D11Resource* pTempRtResource;
	DXUTGetD3D11RenderTargetView()->GetResource(&pTempRtResource);
	DXUTGetD3D11RenderTargetView()->GetDesc(&RtDesc);
	g_MagnifyTool.SetSourceResources(pTempRtResource, RtDesc.Format,
		DXUTGetDXGIBackBufferSurfaceDesc()->Width, DXUTGetDXGIBackBufferSurfaceDesc()->Height,
		DXUTGetDXGIBackBufferSurfaceDesc()->SampleDesc.Count);
	g_MagnifyTool.SetPixelRegion(128);
	g_MagnifyTool.SetScale(5);
	SAFE_RELEASE(pTempRtResource);

	g_Util.OnResizedSwapChain(pD3dDevice, pBackBufferSurfaceDesc, TEXT_LINE_HEIGHT);

	return S_OK;
}

//-----------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11ResizedSwapChain
//-----------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain(void* pUserContext)
{
	g_Util.OnReleasingSwapChain();

	g_DialogResourceManager.OnD3D11ReleasingSwapChain();

	SAFE_RELEASE(g_pDepthStencilTexture);
	SAFE_RELEASE(g_pDepthStencilView);
	SAFE_RELEASE(g_pDepthStencilSRV);
}

//----------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11CreateDevice
//----------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice(void* pUserContext)
{
	g_DialogResourceManager.OnD3D11DestroyDevice();
	g_SettingsDlg.OnD3D11DestroyDevice();
	DXUTGetGlobalResourceCache().OnDestroyDevice();
	SAFE_DELETE(g_pTextHelper);

	SAFE_RELEASE(g_pDepthStencilTexture);
	SAFE_RELEASE(g_pDepthStencilView);
	SAFE_RELEASE(g_pDepthStencilSRV);
	SAFE_RELEASE(g_pDepthGreater);
	SAFE_RELEASE(g_pDepthEqualAndDisableDepthWrite);

	SAFE_RELEASE(g_pScenePositionOnlyVS);
	SAFE_RELEASE(g_pScenePositionAndTextureVS);
	SAFE_RELEASE(g_pSceneVS);
	SAFE_RELEASE(g_pScenePS);
	SAFE_RELEASE(g_pScenePSAlphaTest);
	SAFE_RELEASE(g_pScenePSNoCull);
	SAFE_RELEASE(g_pScenePSNoCullAlphaTest);
	SAFE_RELEASE(g_pScenePSAlphaTestOnly);
	SAFE_RELEASE(g_pDebugDrawNumLightsPerTileRadarColorsPS);
	SAFE_RELEASE(g_pDebugDrawNumLightsPerTileGrayscalePS);
	SAFE_RELEASE(g_pLightCullCS);
	SAFE_RELEASE(g_pLightCullCSMSAA);
	SAFE_RELEASE(g_pLightCullCSNoDepth);
	SAFE_RELEASE(g_pLayoutPositionOnly11);
	SAFE_RELEASE(g_pLayoutPositionAndTex11);
	SAFE_RELEASE(g_pLayout11);
	SAFE_RELEASE(g_pSamLinear);

	SAFE_RELEASE(g_pOpaqueState);
	SAFE_RELEASE(g_pDepthOnlyAlphaTestState);
	SAFE_RELEASE(g_pDepthOnlyAlphaToCoverageState);

	SAFE_RELEASE(g_pDisableCullingRS);

	//Delete additional render resources here...
	g_SceneMesh.Destroy();
	g_AlphaMesh.Destroy();

	SAFE_RELEASE(g_pcbPerObject11);
	SAFE_RELEASE(g_pcbPerFrame11);

	g_Util.OnDestroyDevice();

	// Destroy AMD_SDK resources here
	g_ShaderCache.OnDestroyDevice();
	g_HUD.OnDestroyDevice();
	g_MagnifyTool.OnDestroyDevice();
	TIMER_Destroy();

}

//--------------------------------------------------------------------------------------
// Stripped down version of DXUT ClearD3D11DeviceCOntext.
// For this sample,the HS,DS,and GS are not used.And it is assumped that drawing code will always 
// call VSSetShader,PSSetShader,IASetVertexBuffers,IASetIndexBuffer(if applicable), and IASetInputLayout.
void ClearD3D11DeviceContext()
{
	ID3D11DeviceContext* pD3dDeviceContext = DXUTGetD3D11DeviceContext();

	ID3D11ShaderResourceView* pSRVs[16] = 
	{	nullptr,nullptr,nullptr,nullptr,
		nullptr,nullptr,nullptr,nullptr,
		nullptr,nullptr,nullptr,nullptr,
		nullptr,nullptr,nullptr,nullptr 
	};

	ID3D11RenderTargetView* pRTVs[16] = 
	{ nullptr,nullptr,nullptr,nullptr,
		nullptr,nullptr,nullptr,nullptr,
		nullptr,nullptr,nullptr,nullptr,
		nullptr,nullptr,nullptr,nullptr 
	};

	ID3D11DepthStencilView* pDSV = nullptr;

	ID3D11Buffer* pBuffers[16] = 
	{	nullptr,nullptr,nullptr,nullptr,
		nullptr,nullptr,nullptr,nullptr,
		nullptr,nullptr,nullptr,nullptr,
		nullptr,nullptr,nullptr,nullptr 
	};

	ID3D11SamplerState* pSamplers[16] =
	{
		nullptr,nullptr,nullptr,nullptr,
		nullptr,nullptr,nullptr,nullptr,
		nullptr,nullptr,nullptr,nullptr,
		nullptr,nullptr,nullptr,nullptr
	};


	// Constant buffers
	pD3dDeviceContext->VSSetConstantBuffers(0, 14, pBuffers);
	pD3dDeviceContext->PSSetConstantBuffers(0, 14, pBuffers);
	pD3dDeviceContext->CSSetConstantBuffers(0, 14, pBuffers);

	//Resources
	pD3dDeviceContext->VSSetShaderResources(0, 16, pSRVs);
	pD3dDeviceContext->PSSetShaderResources(0, 16, pSRVs);
	pD3dDeviceContext->CSSetShaderResources(0, 16, pSRVs);

	// Samplers
	pD3dDeviceContext->VSSetSamplers(0, 16, pSamplers);
	pD3dDeviceContext->PSSetSamplers(0, 16, pSamplers);
	pD3dDeviceContext->CSSetSamplers(0, 16, pSamplers);

	// Render targets
	pD3dDeviceContext->OMSetRenderTargets(8, pRTVs, pDSV);

	// States
	FLOAT BlendFactor[4] = { 0,0,0,0 };
	pD3dDeviceContext->OMSetBlendState(nullptr, BlendFactor, 0xFFFFFFFF);
	pD3dDeviceContext->OMSetDepthStencilState(g_pDepthGreater, 0x00); //we are using inverted 32-bit float depth for better precision
	pD3dDeviceContext->RSSetState(nullptr);
}


//---------------------------------------------------------------------------------------
// Render the scene using ghe D3D11 device
//---------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender(ID3D11Device* pD3dDevice, ID3D11DeviceContext* pD3dImmediateContext, double fTime, float fElapsedTime, void* pUserContext)
{
	// Reset the timer at start of frame
	TIMER_Reset();

	// If the setting dialog is being shown,then render it instead of rendering the app's scene
	if (g_SettingsDlg.IsActive())
	{
		g_SettingsDlg.OnRender(fElapsedTime);
		return;
	}

	ClearD3D11DeviceContext();

	const DXGI_SURFACE_DESC* pBackBufferDesc = DXUTGetDXGIBackBufferSurfaceDesc();

	// Default pixel shader
	ID3D11PixelShader* pScenePS = g_pScenePS;
	ID3D11PixelShader* pScenePSAlphaTest = g_pScenePSAlphaTest;

	// See if we need to use one of the debug drawing shaders instead
	bool bDebugDrawingEnabled = g_HUD.m_GUI.GetCheckBox(IDC_CHECKBOX_ENABLE_DEBUG_DRAWING)->GetEnabled() &&
		g_HUD.m_GUI.GetCheckBox(IDC_CHECKBOX_ENABLE_DEBUG_DRAWING)->GetChecked();
	bool bDebugDrawMethodOne = g_HUD.m_GUI.GetRadioButton(IDC_RADIOBUTTON_DEBUG_DRAWING_ONE)->GetEnabled() &&
		g_HUD.m_GUI.GetRadioButton(IDC_RADIOBUTTON_DEBUG_DRAWING_ONE)->GetChecked();

	if (bDebugDrawingEnabled)
	{
		pScenePS = bDebugDrawMethodOne ? g_pDebugDrawNumLightsPerTileRadarColorsPS : g_pDebugDrawNumLightsPerTileGrayscalePS;
		pScenePSAlphaTest = bDebugDrawMethodOne ? g_pDebugDrawNumLightsPerTileRadarColorsPS : g_pDebugDrawNumLightsPerTileGrayscalePS;
	}

	// And see if we need to use the no-cull pixel shader instead
	bool bLightCullingEnabled = g_HUD.m_GUI.GetCheckBox(IDC_CHECKBOX_ENABLE_LIGHT_CULLING)->GetEnabled() &&
		g_HUD.m_GUI.GetCheckBox(IDC_CHECKBOX_ENABLE_LIGHT_CULLING)->GetChecked();
	pScenePS = bLightCullingEnabled ? pScenePS : g_pScenePSNoCull;
	pScenePSAlphaTest = bLightCullingEnabled ? pScenePSAlphaTest : g_pScenePSNoCullAlphaTest;

	// Default compute shader
	bool bMsaaEnabled = (pBackBufferDesc->SampleDesc.Count > 1);
	ID3D11ComputeShader* pLightCullCS = bMsaaEnabled ? g_pLightCullCSMSAA : g_pLightCullCS;
	ID3D11ShaderResourceView* pDepthSRV = g_pDepthStencilSRV;

	// Determine which compute shader we should use
	bool bDepthBoundsEnabled = g_HUD.m_GUI.GetCheckBox(IDC_CHECKBOX_ENABLE_DEPTH_BOUNDS)->GetEnabled() &&
		g_HUD.m_GUI.GetCheckBox(IDC_CHECKBOX_ENABLE_DEPTH_BOUNDS)->GetChecked();
	pLightCullCS = bDepthBoundsEnabled ? pLightCullCS : g_pLightCullCSNoDepth;
	pDepthSRV = bDepthBoundsEnabled ? pDepthSRV : nullptr;

	// Clear the backBuffer and depth stencil
	float ClearColor[4] = { 0.0013f,0.0015f,0.0050f,0.0f };
	ID3D11RenderTargetView* pRTV = DXUTGetD3D11RenderTargetView();
	pD3dImmediateContext->ClearRenderTargetView((ID3D11RenderTargetView*)DXUTGetD3D11RenderTargetView(), ClearColor);
	pD3dImmediateContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH, 0.0, 0); // we are using inverted depth,so cleat to zero

	XMMATRIX mWorld = XMMatrixIdentity();

	// Get the projection & view matrix from the camera class
	XMMATRIX mView = g_Camera.GetViewMatrix();
	XMMATRIX mProj = g_Camera.GetProjMatrix();
	XMMATRIX mWorldView = mWorld*mView;
	XMMATRIX mWorldViewProjection = mWorld*mView*mProj;

	// we need the inverse proj matrix in the per-tile light culling compute shader
	XMFLOAT4X4 f4x4Proj, f4x4InvProj;
	XMStoreFloat4x4(&f4x4Proj, mProj);
	XMStoreFloat4x4(&f4x4InvProj, XMMatrixIdentity());
	f4x4InvProj._11 = 1.0f / f4x4Proj._11;
	f4x4InvProj._22 = 1.0f / f4x4Proj._22;
	f4x4InvProj._33 = 0.0f;
	f4x4InvProj._34 = 1.0f / f4x4Proj._43;
	f4x4InvProj._43 = 1.0f;
	f4x4InvProj._44 = -f4x4Proj._33 / f4x4Proj._43;
	XMMATRIX mInvProj = XMLoadFloat4x4(&f4x4InvProj);


	XMFLOAT4 CameraPosAndAlphaTest;
	XMStoreFloat4(&CameraPosAndAlphaTest, g_Camera.GetEyePt());
	// different alpha test for MSAA enabled vs disabled
	CameraPosAndAlphaTest.w = bMsaaEnabled ? 0.003f : 0.5f;


	// Set the constant buffers
	HRESULT hr;
	D3D11_MAPPED_SUBRESOURCE MappedResource;

	V(pD3dImmediateContext->Map(g_pcbPerFrame11, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
	CB_PER_FRAME* pPerFrame = (CB_PER_FRAME*)MappedResource.pData;
	pPerFrame->m_mProjection = XMMatrixTranspose(mProj);
	pPerFrame->m_mProjectionInv = XMMatrixTranspose(mInvProj);
	pPerFrame->m_vCameraPosAndAlphaTest = XMLoadFloat4(&CameraPosAndAlphaTest);
	pPerFrame->m_uNumLights = (((unsigned)g_iNumActiveSpotLights & 0xFFFFu) << 16) | ((unsigned)g_iNumActivePointLights & 0xFFFFu);
	pPerFrame->m_uWindowWidth = pBackBufferDesc->Width;
	pPerFrame->m_uWindowHeight = pBackBufferDesc->Height;
	pPerFrame->m_uMaxNumLightsPerTile = g_Util.GetMaxNumLightsPerTile();
	pD3dImmediateContext->Unmap(g_pcbPerFrame11, 0);
	pD3dImmediateContext->VSSetConstantBuffers(1, 1, &g_pcbPerFrame11);
	pD3dImmediateContext->PSSetConstantBuffers(1, 1, &g_pcbPerFrame11);
	pD3dImmediateContext->CSSetConstantBuffers(1, 1, &g_pcbPerFrame11);


	V(pD3dImmediateContext->Map(g_pcbPerObject11, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource));
	CB_PER_OBJECT* pPerObject = (CB_PER_OBJECT*)MappedResource.pData;
	pPerObject->m_mWorldViewProjection = XMMatrixTranspose(mWorldViewProjection);
	pPerObject->m_mWorldView = XMMatrixTranspose(mWorldView);
	pPerObject->m_mWorld = XMMatrixTranspose(mWorld);
	pPerObject->m_MaterialAmbientColorUp = XMVectorSet(0.013f, 0.015f, 0.050f, 1.0f);
	pPerObject->m_MaterialAmbientColorDown = XMVectorSet(0.0013f, 0.0015f, 0.0050f, 1.0f);
	pD3dImmediateContext->Unmap(g_pcbPerObject11, 0);
	pD3dImmediateContext->VSSetConstantBuffers(0, 1, &g_pcbPerObject11);
	pD3dImmediateContext->PSSetConstantBuffers(0, 1, &g_pcbPerObject11);
	pD3dImmediateContext->CSSetConstantBuffers(0, 1, &g_pcbPerObject11);



	// Switch off alpha blending
	float BlendFactor[1] = { 0.0f };
	pD3dImmediateContext->OMSetBlendState(g_pOpaqueState, BlendFactor, 0xFFFFFFFF);

	//Render objects here...
	if (g_ShaderCache.ShadersReady())
	{
		ID3D11RenderTargetView* pNullRTV = nullptr;
		ID3D11DepthStencilView* pNullDSV = nullptr;
		ID3D11ShaderResourceView* pNullSRV = nullptr;
		ID3D11UnorderedAccessView* pNullUAV = nullptr;
		ID3D11SamplerState* pNullSampler = nullptr;

		TIMER_Begin(0, L"Render");
		{
			TIMER_Begin(0, L"Depth pre-pass");
			{
				// Depth pre-pass (to eliminate pixel overdraw during forward rendering)
				pD3dImmediateContext->OMSetRenderTargets(1, &pNullRTV, g_pDepthStencilView); //null color buffer
				pD3dImmediateContext->OMSetDepthStencilState(g_pDepthGreater, 0x00); //we are using inverted 32-bit float depth for better precision
				pD3dImmediateContext->IASetInputLayout(g_pLayoutPositionOnly11);
				pD3dImmediateContext->VSSetShader(g_pScenePositionOnlyVS, nullptr, 0);
				pD3dImmediateContext->PSSetShader(nullptr, nullptr, 0); // null pixel shader
				pD3dImmediateContext->PSSetShaderResources(0, 1, &pNullSRV);
				pD3dImmediateContext->PSSetShaderResources(1, 1, &pNullSRV);
				pD3dImmediateContext->PSSetSamplers(0, 1, &pNullSampler);
				g_SceneMesh.Render(pD3dImmediateContext);


				// More depth pre-pass,for test geometry
				pD3dImmediateContext->OMSetRenderTargets(1, (ID3D11RenderTargetView*const*)&pRTV, g_pDepthStencilView);
				if (bMsaaEnabled)
				{
					pD3dImmediateContext->OMSetBlendState(g_pDepthOnlyAlphaToCoverageState, BlendFactor, 0xFFFFFFFF);
				}
				else
				{
					pD3dImmediateContext->OMSetBlendState(g_pDepthOnlyAlphaTestState, BlendFactor, 0xFFFFFFFF);
				}

				pD3dImmediateContext->RSSetState(g_pDisableCullingRS);
				pD3dImmediateContext->IASetInputLayout(g_pLayoutPositionAndTex11);
				pD3dImmediateContext->VSSetShader(g_pScenePositionAndTextureVS, nullptr, 0);
				pD3dImmediateContext->PSSetShader(g_pScenePSAlphaTestOnly, nullptr, 0);
				pD3dImmediateContext->PSSetSamplers(0, 1, &g_pSamLinear);
				g_AlphaMesh.Render(pD3dImmediateContext, 0);

				//还原
				pD3dImmediateContext->RSSetState(nullptr);
				pD3dImmediateContext->OMSetBlendState(g_pOpaqueState, BlendFactor, 0xFFFFFFFF);
			}
			TIMER_End(); //Depth pre-pass



			TIMER_Begin(0, L"Light culling");
			{
				// Cull lights on the GPU,using a Compute Shader
				if (g_HUD.m_GUI.GetCheckBox(IDC_CHECKBOX_ENABLE_LIGHT_CULLING)->GetEnabled() && g_HUD.m_GUI.GetCheckBox(IDC_CHECKBOX_ENABLE_LIGHT_CULLING)->GetChecked())
				{
					pD3dImmediateContext->OMSetRenderTargets(1, &pNullRTV, pNullDSV); // null color buffer and depth-stencil
					pD3dImmediateContext->VSSetShader(nullptr, nullptr, 0);// null vertex shader
					pD3dImmediateContext->PSSetShader(nullptr, nullptr, 0); // null pixel shader
					pD3dImmediateContext->PSSetShaderResources(0, 1, &pNullSRV);
					pD3dImmediateContext->PSSetShaderResources(1, 1, &pNullSRV);
					pD3dImmediateContext->PSSetSamplers(0, 1, &pNullSampler);
					pD3dImmediateContext->CSSetShader(pLightCullCS, nullptr, 0);
					pD3dImmediateContext->CSSetShaderResources(0, 1, g_Util.GetPointLightBufferCenterAndRadiusSrvParam());
					pD3dImmediateContext->CSSetShaderResources(1, 1, g_Util.GetSpotLightBufferCenterAndRadiusSrvParam());
					pD3dImmediateContext->CSSetShaderResources(2, 1, &pDepthSRV);
					pD3dImmediateContext->CSSetUnorderedAccessViews(0, 1, g_Util.GetLightIndexBufferUavParam(), nullptr); // target UAV;
					pD3dImmediateContext->Dispatch(g_Util.GetNumTilesX(), g_Util.GetNumTilesY(), 1);

					//clear state
					pD3dImmediateContext->CSSetShader(nullptr, nullptr, 0);
					pD3dImmediateContext->CSSetShaderResources(0, 1, &pNullSRV);
					pD3dImmediateContext->CSSetShaderResources(1, 1, &pNullSRV);
					pD3dImmediateContext->CSSetShaderResources(2, 1, &pNullSRV);
					pD3dImmediateContext->CSSetUnorderedAccessViews(0, 1, &pNullUAV, nullptr);
				}
			}
			TIMER_End(); //Light culling


			TIMER_Begin(0, L"Forward rendering");
			{
				// forward rendering
				pD3dImmediateContext->OMSetRenderTargets(1, (ID3D11RenderTargetView*const*)&pRTV, g_pDepthStencilView);
				pD3dImmediateContext->OMSetDepthStencilState(g_pDepthEqualAndDisableDepthWrite, 0x00);
				pD3dImmediateContext->IASetInputLayout(g_pLayout11);
				pD3dImmediateContext->VSSetShader(g_pSceneVS, nullptr, 0);
				pD3dImmediateContext->PSSetShader(pScenePS, nullptr, 0);
				pD3dImmediateContext->PSSetSamplers(0, 1, &g_pSamLinear);
				pD3dImmediateContext->PSSetShaderResources(2, 1, g_Util.GetPointLightBufferCenterAndRadiusSrvParam());
				pD3dImmediateContext->PSSetShaderResources(3, 1, g_Util.GetPointLightBufferColorSrvParam());
				pD3dImmediateContext->PSSetShaderResources(4, 1, g_Util.GetSpotLightBufferCenterAndRadiusSrvParam());
				pD3dImmediateContext->PSSetShaderResources(5, 1, g_Util.GetSpotLightBufferColorSrvParam());
				pD3dImmediateContext->PSSetShaderResources(6, 1, g_Util.GetSpotLightBufferSpotParamsSrvParam());
				pD3dImmediateContext->PSSetShaderResources(7, 1, g_Util.GetLightIndexBufferSrvParam());
				g_SceneMesh.Render(pD3dImmediateContext, 0, 1);

				// More forward rendering,for alpha test geometry
				pD3dImmediateContext->RSSetState(g_pDisableCullingRS);
				pD3dImmediateContext->PSSetShader(pScenePSAlphaTest, nullptr, 0);
				g_AlphaMesh.Render(pD3dImmediateContext, 0, 1);
				pD3dImmediateContext->RSSetState(nullptr);


				// restore to default
				pD3dImmediateContext->PSSetShaderResources(2, 1, &pNullSRV);
				pD3dImmediateContext->PSSetShaderResources(3, 1, &pNullSRV);
				pD3dImmediateContext->PSSetShaderResources(4, 1, &pNullSRV);
				pD3dImmediateContext->PSSetShaderResources(5, 1, &pNullSRV);
				pD3dImmediateContext->PSSetShaderResources(6, 1, &pNullSRV);
				pD3dImmediateContext->PSSetShaderResources(7, 1, &pNullSRV);
				pD3dImmediateContext->OMSetDepthStencilState(g_pDepthGreater, 0x00); // we are using inverted 32-bit float depth for better precision
			}
			TIMER_End();// Forward rendering

			TIMER_Begin(0, L"Light debug drawing");
			{
				// Light debug drawing
				if (g_HUD.m_GUI.GetCheckBox(IDC_CHECKBOX_ENABLE_LIGHT_DRAWING)->GetEnabled() &&
					g_HUD.m_GUI.GetCheckBox(IDC_CHECKBOX_ENABLE_LIGHT_DRAWING)->GetChecked())
				{
					g_Util.OnRender(fElapsedTime, (unsigned)g_iNumActivePointLights, (unsigned)g_iNumActiveSpotLights);
				}
			}
			TIMER_End(); //Light debug drawing
		}
		TIMER_End();//Render


		DXUT_BeginPerfEvent(DXUT_PERFEVENTCOLOR, L"HUD/Stats");

		// Render the HUD
		if (g_bRenderHUD)
		{
			g_MagnifyTool.Render();
			g_HUD.OnRender(fElapsedTime);
		}

		RenderText(); //jingz todo

		DXUT_EndPerfEvent();
	}
	else
	{
		// Render shader cache progress if still processing
		pD3dImmediateContext->OMSetRenderTargets(1, (ID3D11RenderTargetView*const*)&pRTV, g_pDepthStencilView);
		g_ShaderCache.RenderProgress(g_pTextHelper, TEXT_LINE_HEIGHT, XMVectorSet(1.0f, 1.0f, 0.0f, 1.0f));
	}

	static DWORD dwTimefirst = GetTickCount();
	if (GetTickCount() - dwTimefirst > 5000)
	{
		OutputDebugString(DXUTGetFrameStats(DXUTIsVsyncEnabled()));
		OutputDebugString(L"\n");
		dwTimefirst = GetTickCount();
	}

}

//---------------------------------------------------------------------------------------
// Render the help and statistics text.This function uses the ID3DXFont interface for
// efficient text rendering.
//--------------------------------------------------------------------------------------
void RenderText()
{
	g_pTextHelper->Begin();
	g_pTextHelper->SetInsertionPos(5, 5);
	g_pTextHelper->SetForegroundColor(XMVectorSet(1.0f, 1.0f, 0.0f, 1.0f));
	g_pTextHelper->DrawTextLine(DXUTGetFrameStats(DXUTIsVsyncEnabled()));
	g_pTextHelper->DrawTextLine(DXUTGetDeviceStats());

	const float fGpuTime = (float)TIMER_GetTime(Gpu, L"Render")*1000.0f;

	// count digits in the total time
	int iIntegerPart = (int)fGpuTime;
	int iNumDigits = 0;
	while (iIntegerPart > 0)
	{
		iIntegerPart /= 10;
		iNumDigits++;
	}
	iNumDigits = (iNumDigits == 0) ? 1 : iNumDigits;
	// three digits after decimal,plus the decimal and point itself
	int iNumChars = iNumDigits + 4;

	// dynamic formatting for swprintf_s
	WCHAR szPrecision[16];
	swprintf_s(szPrecision, 16, L"%%%d.3f", iNumChars);

	WCHAR szBuf[256];
	WCHAR szFormat[256];
	swprintf_s(szFormat, 256, L"Total:      %s", szPrecision);
	swprintf_s(szBuf, 256, szFormat, fGpuTime);
	g_pTextHelper->DrawTextLine(szBuf);

	const float fGpuTimeDepthPrePass = (float)TIMER_GetTime(Gpu, L"Render|Depth pre-pass")*1000.0f;
	swprintf_s(szFormat, 256, L"+----Z Pass: %s", szPrecision);
	swprintf_s(szBuf, 256, szFormat, fGpuTimeDepthPrePass);
	g_pTextHelper->DrawTextLine(szBuf);

	const float fGpuTimeLightCulling = (float)TIMER_GetTime(Gpu, L"Render|Light culling")*1000.0f;
	swprintf_s(szFormat, 256, L"+----Cull:%s", szPrecision);
	swprintf_s(szBuf, 256, szFormat, fGpuTimeLightCulling);
	g_pTextHelper->DrawTextLine(szBuf);

	const float fGpuTimeForwardRendering = (float)TIMER_GetTime(Gpu, L"Render|Forward rendering")*1000.0f;
	swprintf_s(szFormat, 256, L"+---Forward:%s", szPrecision);
	swprintf_s(szBuf, 256, szFormat, fGpuTimeForwardRendering);
	g_pTextHelper->DrawTextLine(szBuf);

	const float fGpuTimeLightDebugDrawing = (float)TIMER_GetTime(Gpu, L"Render|Light debug drawing")*1000.0f;
	swprintf_s(szFormat, 256, L"/----Lights:%s", szPrecision);
	swprintf_s(szBuf, 256, szFormat, fGpuTimeLightDebugDrawing);
	g_pTextHelper->DrawTextLine(szBuf);

	g_pTextHelper->SetInsertionPos(5, DXUTGetDXGIBackBufferSurfaceDesc()->Height - AMD::HUD::iElementDelta);
	g_pTextHelper->DrawTextLine(L"Toggle GUI : F1");

	g_pTextHelper->End();

	bool bDebugDrawingEnabled = g_HUD.m_GUI.GetCheckBox(IDC_CHECKBOX_ENABLE_DEBUG_DRAWING)->GetEnabled() &&
		g_HUD.m_GUI.GetCheckBox(IDC_CHECKBOX_ENABLE_DEBUG_DRAWING)->GetChecked();
	if (bDebugDrawingEnabled)
	{
		// method 1 is radar colors,method 2 is grayScale
		bool bDebugDrawMethodOne = g_HUD.m_GUI.GetRadioButton(IDC_RADIOBUTTON_DEBUG_DRAWING_ONE)->GetEnabled() &&
			g_HUD.m_GUI.GetRadioButton(IDC_RADIOBUTTON_DEBUG_DRAWING_ONE)->GetChecked();
		g_Util.RenderLegend(g_pTextHelper, TEXT_LINE_HEIGHT, XMFLOAT4(1.0f, 1.0f, 1.0f, 0.75f), !bDebugDrawMethodOne);
	}

}

//------------------------------------------------------
//  Initialize the app
//------------------------------------------------------
void InitApp()
{
	WCHAR szTemp[256];

	D3DCOLOR DlgColor = 0x88888888; // Semi-transparent background for the dialog

	g_SettingsDlg.Init(&g_DialogResourceManager);
	g_HUD.m_GUI.Init(&g_DialogResourceManager);
	g_HUD.m_GUI.SetBackgroundColors(DlgColor);
	g_HUD.m_GUI.SetCallback(OnGUIEvent);

	int iY = AMD::HUD::iElementDelta;

	g_HUD.m_GUI.AddButton(IDC_BUTTON_TOGGLE_FULLSCREEN, L"Toggle full screen", AMD::HUD::iElementOffset, iY, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight);
	g_HUD.m_GUI.AddButton(IDC_BUTTON_TOGGLE_REF, L"Toggle REF (F3)", AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, VK_F3);
	g_HUD.m_GUI.AddButton(IDC_BUTTON_CHANGEDEVICE, L"Change device (F2)", AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, VK_F2);

	// Point LightsRenderText
	iY += AMD::HUD::iGroupDelta;
	g_HUD.m_GUI.AddCheckBox(IDC_CHECKBOX_ENABLE_LIGHT_DRAWING, L"Show Lights", AMD::HUD::iElementOffset, iY, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, true);
	swprintf_s(szTemp, L"Active Point Lights: %d", g_iNumActivePointLights);
	g_HUD.m_GUI.AddStatic(IDC_STATIC_NUM_POINT_LIGHTS, szTemp, AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight);
	g_HUD.m_GUI.AddSlider(IDC_SLIDER_NUM_POINT_LIGHTS, AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, 0, MAX_NUM_LIGHTS, g_iNumActivePointLights, true);

	//Spot lights
	swprintf_s(szTemp, L"Active Spot Lights:%d", g_iNumActiveSpotLights);
	g_HUD.m_GUI.AddStatic(IDC_STATIC_NUM_SPOT_LIGHTS, szTemp, AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight);
	g_HUD.m_GUI.AddSlider(IDC_SLIDER_NUM_SPOT_LIGHTS, AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, 0, MAX_NUM_LIGHTS, g_iNumActiveSpotLights);

	iY += AMD::HUD::iGroupDelta;

	g_HUD.m_GUI.AddCheckBox(IDC_CHECKBOX_ENABLE_LIGHT_CULLING, L"Enable Light Culling", AMD::HUD::iElementOffset, iY, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, true);
	g_HUD.m_GUI.AddCheckBox(IDC_CHECKBOX_ENABLE_DEPTH_BOUNDS, L"Enable Depth Bounds", AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, true);
	g_HUD.m_GUI.AddCheckBox(IDC_CHECKBOX_ENABLE_DEBUG_DRAWING, L"Show Lights Per Tile", AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, false);
	g_HUD.m_GUI.AddRadioButton(IDC_RADIOBUTTON_DEBUG_DRAWING_ONE, IDC_TILE_DRAWING_GROUP, L"Radar Colors", 2 * AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth - AMD::HUD::iElementOffset, AMD::HUD::iElementHeight, true);
	g_HUD.m_GUI.AddRadioButton(IDC_RADIOBUTTON_DEBUG_DRAWING_TWO, IDC_TILE_DRAWING_GROUP, L"GrayScale", 2 * AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth - AMD::HUD::iElementOffset, AMD::HUD::iElementHeight, false);
	g_HUD.m_GUI.GetRadioButton(IDC_RADIOBUTTON_DEBUG_DRAWING_ONE)->SetEnabled(false);
	g_HUD.m_GUI.GetRadioButton(IDC_RADIOBUTTON_DEBUG_DRAWING_TWO)->SetEnabled(false);

	iY += AMD::HUD::iGroupDelta;

	// Add the magnify tool UI to our HUD
	g_MagnifyTool.InitApp(&g_HUD.m_GUI, iY, true);
}


//--------------------------------------------------------------
// Handle the GUI events
//--------------------------------------------------------------
void CALLBACK OnGUIEvent(UINT nEvent, int nControlID, CDXUTControl*pControl, void* pUserContext)
{
	WCHAR szTemp[256];

	switch (nControlID)
	{
	case IDC_BUTTON_TOGGLE_FULLSCREEN:
		DXUTToggleFullScreen();
		break;

	case IDC_BUTTON_TOGGLE_REF:
		DXUTToggleREF();
		break;

	case IDC_BUTTON_CHANGEDEVICE:
		g_SettingsDlg.SetActive(!g_SettingsDlg.IsActive());
		break;

	case IDC_SLIDER_NUM_POINT_LIGHTS:
	{
		// update
		g_iNumActivePointLights = ((CDXUTSlider*)pControl)->GetValue();
		swprintf_s(szTemp, L"Active Point Light : %d", g_iNumActivePointLights);
		g_HUD.m_GUI.GetStatic(IDC_STATIC_NUM_POINT_LIGHTS)->SetText(szTemp);
	}
		break;

	case IDC_SLIDER_NUM_SPOT_LIGHTS:
	{
		// update
		g_iNumActiveSpotLights = ((CDXUTSlider*)pControl)->GetValue();
		swprintf_s(szTemp, L"Active Spot Lights: %d", g_iNumActiveSpotLights);
		g_HUD.m_GUI.GetStatic(IDC_STATIC_NUM_SPOT_LIGHTS)->SetText(szTemp);
	}
		break;

	case IDC_CHECKBOX_ENABLE_LIGHT_CULLING:
	{
		bool bLightCullingEnabled = g_HUD.m_GUI.GetCheckBox(IDC_CHECKBOX_ENABLE_LIGHT_CULLING)->GetEnabled() &&
			g_HUD.m_GUI.GetCheckBox(IDC_CHECKBOX_ENABLE_LIGHT_CULLING)->GetChecked();
		g_HUD.m_GUI.GetCheckBox(IDC_CHECKBOX_ENABLE_DEBUG_DRAWING)->SetEnabled(bLightCullingEnabled);
		g_HUD.m_GUI.GetCheckBox(IDC_CHECKBOX_ENABLE_DEPTH_BOUNDS)->SetEnabled(bLightCullingEnabled);
		if (bLightCullingEnabled == false)
		{
			g_HUD.m_GUI.GetCheckBox(IDC_CHECKBOX_ENABLE_DEBUG_DRAWING)->SetChecked(false);
		}
	}

	// intentional fall-through to IDC_ENABLE_DEBUG_DRAWING case
	case IDC_CHECKBOX_ENABLE_DEBUG_DRAWING:
	{
		bool bTileDrawingEnabled = g_HUD.m_GUI.GetCheckBox(IDC_CHECKBOX_ENABLE_DEBUG_DRAWING)->GetEnabled() &&
			g_HUD.m_GUI.GetCheckBox(IDC_CHECKBOX_ENABLE_DEBUG_DRAWING)->GetChecked();
		g_HUD.m_GUI.GetRadioButton(IDC_RADIOBUTTON_DEBUG_DRAWING_ONE)->SetEnabled(bTileDrawingEnabled);
		g_HUD.m_GUI.GetRadioButton(IDC_RADIOBUTTON_DEBUG_DRAWING_TWO)->SetEnabled(bTileDrawingEnabled);
	}
		break;
	}//switch

	// Call the MagnifyTool gui event handler
	g_MagnifyTool.OnGUIEvent(nEvent, nControlID, pControl, pUserContext);

}