// TestTriangle.cpp : Defines the entry point for the application.
//

#include "ForwardPlus11.h"
#include "../../DXUT/Core/DXUT.h"
#include "../../DXUT/Optional/DXUTcamera.h"
#include "../../DXUT/Optional/SDKmisc.h"
#include "../source/TriangleRender.h"
#include "../source/ForwardPlusRender.h"
#include "../source/DeferredDecalRender.h"
#include "../source/ScreenBlendRender.h"
#include "../source/RadialBlurRender.h"
#include "../source/DeferredVoxelCutoutRender.h"
#include "../source/SphereRender.h"
#include <algorithm>
#include "../../DXUT/Optional/DXUTSettingsDlg.h"
#include "../../DXUT/Core/WICTextureLoader.h"
#include <stdlib.h>
#include <string>
#include "../source/stb_image.h"
#include "../source/CubeMapCaptureRender.h"
#include "../source/PbrRender.h"

#define FORWARDPLUS
#define TRIANGLE
#define DECAL
#define GodRay
//#define PBR

#ifdef PBR
	#define EXPORT_CUBEMAP
#else
#undef EXPORT_CUBEMAP
#endif

#define MAX_TEMP_SCENE_TEXTURE 2

using namespace DirectX;
using namespace ForwardRender;

#pragma warning( disable : 4100 ) // disable unreference formal parameter warnings for /W4 builds
//-----------------------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------------------
static const int TEXT_LINE_HEIGHT = 15;

#ifdef FORWARDPLUS
static ForwardPlus11::ForwardPlusRender s_ForwardPlusRender;
#endif // FORWARDPLUS

#ifdef TRIANGLE
static ForwardRender::TriangleRender s_TriangleRender;
#endif

#ifdef DECAL
static PostProcess::DeferredDecalRender s_DeferredDecalRender;
#endif

#ifdef GodRay

static PostProcess::DeferredVoxelCutoutRender s_DeferredVoxelCutoutRender;
static ForwardRender::SphereRender s_SphereRender;
static PostProcess::RadialBlurRender s_RadialBlurRender;
static PostProcess::ScreenBlendRender s_ScreenBlendRender;

#endif

#ifdef PBR

static ForwardRender::CubeMapCaptureRender s_CubeMapCaptureRender;
static ForwardRender::PbrRender s_PbrRender;

#endif // PBR



// Direct3D 11 resources
CDXUTSDKMesh g_SceneMesh;
CDXUTSDKMesh g_SceneAlphaMesh;

//-------------------------------------
// AMD helper classes defined here
//-------------------------------------

static AMD::ShaderCache g_ShaderCache;

ID3D11SamplerState* g_pSamplerAnisotropic = nullptr;

// Depth stencil data
ID3D11Texture2D* g_pDepthStencilTexture = nullptr;
ID3D11DepthStencilView* g_pDepthStencilView = nullptr;
ID3D11ShaderResourceView* g_pDepthStencilSRV = nullptr;

ID3D11Texture2D* g_pTempDepthStencilTexture = nullptr;
ID3D11DepthStencilView* g_pTempDepthStencilView = nullptr;
ID3D11ShaderResourceView* g_pTempDepthStencilSRV = nullptr;

ID3D11Texture2D* g_pDecalTexture = nullptr;
ID3D11ShaderResourceView* g_pDecalTextureSRV = nullptr;

ID3D11Texture2D* g_pTempTexture2D[2] = { nullptr,nullptr };
ID3D11RenderTargetView* g_pTempTextureRenderTargetView[2] = { nullptr,nullptr };
ID3D11ShaderResourceView* g_pTempTextureSRV[2] = { nullptr,nullptr };

ID3D11Texture2D* g_pHdrTexture = nullptr;
ID3D11ShaderResourceView* g_pHdrTextureSRV = nullptr;



//GUI
static bool g_bRenderHUD = false;
static bool g_bCD3DSettingsDlgActive = false;
//Debug
static bool g_bDebugDrawingEnabled = false;
static bool g_bDebugDrawMethodOne = true;
//Render
static bool g_bLightCullingEnabled = true;
static bool g_bDepthBoundsEnabled = true;

static bool s_bFirstPass = true;

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
enum
{
	IDC_BUTTON_TOGGLEFULLSCREEN = 1,
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

// Number of currently active lights
static int g_iNumActivePointLights = 2048;
static int g_iNumActiveSpotLights = 0;

static float g_fMaxDistance = 1000.0f;

CFirstPersonCamera g_Camera;

static AMD::HUD             g_HUD;
static CDXUTDialogResourceManager  g_DialogResourceManager; // manager for shared resources of dialogs
static CDXUTTextHelper*			g_pTextHelper = nullptr;
CD3DSettingsDlg             g_D3dSettingsGUI;           // Device settings dialog
														// Depth stencil states
ID3D11DepthStencilState* g_pDepthStencilDefaultDS = nullptr;

LRESULT CALLBACK MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
	void* pUserContext);

bool CALLBACK ModifyDeviceSettings(DXUTDeviceSettings* pDeviceSetting, void* pUserContext);

void CALLBACK OnFrameTimeUpdated(double fTotalTime, float fElaspedTime, void* pUserContext);

bool CALLBACK IsD3D11DeviceAcceptable(const CD3D11EnumAdapterInfo* AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo* DeviceInfo,
	DXGI_FORMAT BackBufferFormat, bool bWindowed, void*pUserContext);

HRESULT CALLBACK OnD3D11DeviceCreated(ID3D11Device* pD3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc/*包含了Buffer格式/SampleDesc*/,
	void* pUserContext);

HRESULT AddShadersToCache();

HRESULT CALLBACK OnD3D11ResizedSwapChain(ID3D11Device* pD3dDevice, IDXGISwapChain* pSwapChain,
	const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext);

void CALLBACK OnD3D11ReleasingSwapChain(void* pUserContext);

void CALLBACK OnD3D11DestroyDevice(void* pUserContext);

void CALLBACK OnFrameRender(ID3D11Device* pD3dDevice, ID3D11DeviceContext* pD3dImmediateContext,
	double fTime, float fElapsedTime, void* pUserContext);

void RenderText();

void InitApp();

void CALLBACK OnKeyboard(UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext);
void CALLBACK OnGUIEvent(UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

	// Enable run-time memory check for debug builds.
#if defined(DEBUG) || defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    // TODO: Place code here.

	// Set DXUT callbacks

	DXUTSetCallbackMsgProc(MsgProc);
	DXUTSetCallbackKeyboard(OnKeyboard);
	DXUTSetCallbackDeviceChanging(ModifyDeviceSettings);
	DXUTSetCallbackFrameMove(OnFrameTimeUpdated);


	//D3D callback
	// D3DDevice流程： 先枚举硬件可接受类型，然后回调DXUTSetCallbackDeviceChanging，之后才派发DXUTSetCallbackD3D11DeviceCreated事件

	DXUTSetCallbackD3D11DeviceAcceptable(IsD3D11DeviceAcceptable);
	DXUTSetCallbackD3D11DeviceCreated(OnD3D11DeviceCreated);
	DXUTSetCallbackD3D11SwapChainResized(OnD3D11ResizedSwapChain);//重设渲染方案大小,调整DepthStencil和RenderTarget、ShaderResource
	DXUTSetCallbackD3D11SwapChainReleasing(OnD3D11ReleasingSwapChain);//正在释放SwapChain.调整窗口或者刷新等操作（非关闭）释放资源
	DXUTSetCallbackD3D11DeviceDestroyed(OnD3D11DestroyDevice);
	DXUTSetCallbackD3D11FrameRender(OnFrameRender);

	InitApp();
	DXUTInit(true,true,NULL);//Parse the command line,show msgBoxes on error,no extra command line params
	DXUTSetCursorSettings(true, false);
	DXUTCreateWindow(L"TestTriangle v1.2");





	DXUTCreateDevice(D3D_FEATURE_LEVEL_11_0, true, g_Viewport.Width, g_Viewport.Height);//jingz 在此窗口为出现全屏化之前，其分辨率一直有个错误bug，偶然消除了

	DXUTMainLoop();

	//Ensure the shaderCache aborts if in a lengthy generation process
	g_ShaderCache.Abort();

    return DXUTGetExitCode();
}


LRESULT  CALLBACK MsgProc(HWND hWnd, UINT uMsg,WPARAM wParam, LPARAM lParam, bool * pbNoFurtherProcessing, void * pUserContext)
{
	switch (uMsg)
	{
		//调整窗口大小
	case WM_GETMINMAXINFO:
		// override DXUT_MIN_WINDOW_SIZE_X and DXUT_MIN_WINDOW_SIZE_Y
		// to prevent windows that are small
		((MINMAXINFO*)lParam)->ptMinTrackSize.x = 512;
		((MINMAXINFO*)lParam)->ptMinTrackSize.y = 512;
		*pbNoFurtherProcessing = true;
		break;

	}

	

	//在调整窗口的过程中，不需要进一步处理
	if (*pbNoFurtherProcessing)
	{
		return 0;
	}

	// Pass messages to dialog resource manager calls so GUI state is updated correctly
	*pbNoFurtherProcessing = g_DialogResourceManager.MsgProc(hWnd, uMsg, wParam, lParam);
	if (*pbNoFurtherProcessing)
		return 0;

	// Pass messages to settings dialog if its active
	if (g_D3dSettingsGUI.IsActive())
	{
		g_D3dSettingsGUI.MsgProc(hWnd, uMsg, wParam, lParam);
		return 0;
	}

	// Give the dialogs a chance to handle the message first
	*pbNoFurtherProcessing = g_HUD.m_GUI.MsgProc(hWnd, uMsg, wParam, lParam);
	if (*pbNoFurtherProcessing)
		return 0;


	// Pass all remaining windows message to camera ,sot it can respond to user input
	// 常规按键处理
	g_Camera.HandleMessages(hWnd, uMsg, wParam, lParam);


	return 0;
}

// Called right before creating a D3D11 device,allowing the app to modify the device setting as needed
bool ModifyDeviceSettings(DXUTDeviceSettings * pDeviceSettings, void * pUserContext)
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
			pDeviceSettings->d3d11.sd.SampleDesc.Count = 1;//MSAA会影响后处理效果
		}

		//Start with sync disabled
		pDeviceSettings->d3d11.SyncInterval = 0;
	}

	//Muti-sample quality is always zero
	pDeviceSettings->d3d11.sd.SampleDesc.Quality = 0;

	// Don't auto create a depth buffer, as this sample requires a depth buffer be created
	// such that it's bindable as a shader resource
	pDeviceSettings->d3d11.AutoCreateDepthStencil = false;

	return true;
}

void OnFrameTimeUpdated(double fTotalTime, float fElaspedTime, void * pUserContext)
{
	//目前update逻辑只有相机部分，并由相机去驱动场景变化
	g_Camera.FrameMove(fElaspedTime);


	// See if we need to use one of the debug drawing shaders instead
	g_bDebugDrawingEnabled = g_HUD.m_GUI.GetCheckBox(IDC_CHECKBOX_ENABLE_DEBUG_DRAWING)->GetEnabled() &&
		g_HUD.m_GUI.GetCheckBox(IDC_CHECKBOX_ENABLE_DEBUG_DRAWING)->GetChecked();
	g_bDebugDrawMethodOne = g_HUD.m_GUI.GetRadioButton(IDC_RADIOBUTTON_DEBUG_DRAWING_ONE)->GetEnabled() &&
		g_HUD.m_GUI.GetRadioButton(IDC_RADIOBUTTON_DEBUG_DRAWING_ONE)->GetChecked();

	// And see if we need to use the no-cull pixel shader instead
	g_bLightCullingEnabled = g_HUD.m_GUI.GetCheckBox(IDC_CHECKBOX_ENABLE_LIGHT_CULLING)->GetEnabled() &&
		g_HUD.m_GUI.GetCheckBox(IDC_CHECKBOX_ENABLE_LIGHT_CULLING)->GetChecked();

	// Determine which compute shader we should use
	g_bDepthBoundsEnabled = g_HUD.m_GUI.GetCheckBox(IDC_CHECKBOX_ENABLE_DEPTH_BOUNDS)->GetEnabled() &&
		g_HUD.m_GUI.GetCheckBox(IDC_CHECKBOX_ENABLE_DEPTH_BOUNDS)->GetChecked();

#ifdef FORWARDPLUS
	s_ForwardPlusRender.SetLightCullingEnable(g_bLightCullingEnabled);
	s_ForwardPlusRender.SetLightCullingDepthBoundEnable(g_bDepthBoundsEnabled);
#endif

}


//-----------------------------------------------------------------------------------------
// Reject any D3D11 devices that aren;t acceptable by returning false
//-----------------------------------------------------------------------------------------
bool CALLBACK IsD3D11DeviceAcceptable(const CD3D11EnumAdapterInfo* AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo* DeviceInfo,
	DXGI_FORMAT BackBufferFormat, bool bWindowed, void*pUserContext)
{
	return true;
}
//#include <d3d11.h>
//#include <d3dx11.h>

// Create any D3D11 resource that aren't dependent on the back buffer
HRESULT CALLBACK OnD3D11DeviceCreated(ID3D11Device * pD3dDevice, const DXGI_SURFACE_DESC * pBackBufferSurfaceDesc, void * pUserContext)
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
	V_RETURN(pD3dDevice->CreateDepthStencilState(&DepthStencilDesc, &g_pDepthStencilDefaultDS));

	V_RETURN(CreateWICTextureFromFile(pD3dDevice, L"../../len_full.jpg", (ID3D11Resource**)&g_pDecalTexture, &g_pDecalTextureSRV));
	
	//{// HDR CubeMap

	//	//V_RETURN(D3DX11CreateShaderResourceViewFromFile(pD3dDevice, L"../../newport_loft.dds", nullptr, nullptr, &g_pHdrTextureSRV, nullptr));
	//	//V_RETURN(CreateWICTextureFromFile(pD3dDevice, L"../../newport_loft.dds", (ID3D11Resource**)&g_pHdrTexture, &g_pHdrTextureSRV));

	//	stbi_set_flip_vertically_on_load(true);
	//	int width, height, nrComponents;
	//	void *data = stbi_load("../../ForwardPlus11/media/hdr/newport_loft.hdr", &width, &height, &nrComponents, 0);

	//	D3D11_TEXTURE2D_DESC texDesc;

	//	texDesc.Width = width;
	//	texDesc.Height = height;
	//	texDesc.MipLevels = 1;
	//	texDesc.ArraySize = 1;
	//	texDesc.Format = DXGI_FORMAT_BC6H_UF16;
	//	texDesc.SampleDesc.Count = 1;
	//	texDesc.SampleDesc.Quality = 0;
	//	texDesc.Usage = D3D11_USAGE_DEFAULT;
	//	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	//	texDesc.CPUAccessFlags = 0;
	//	texDesc.MiscFlags = 0;


	//	D3D11_SUBRESOURCE_DATA InitData = { 0 };
	//	InitData.SysMemPitch = width * nrComponents;
	//	InitData.pSysMem = data;
	//	InitData.SysMemSlicePitch = width *height * nrComponents;

	//	V_RETURN(pD3dDevice->CreateTexture2D(&texDesc, &InitData, &g_pHdrTexture));
	//	V_RETURN(pD3dDevice->CreateShaderResourceView(g_pHdrTexture, nullptr, &g_pHdrTextureSRV));



	//}



	//V_RETURN(CreateWICTextureFromFile(pD3dDevice, L"../media/hdr/newport_loft.hdr", (ID3D11Resource**)&g_pHdrTexture, &g_pHdrTextureSRV));

	//V_RETURN(CreateWICTextureFromFile(pD3dDevice, L"../media/hdr/sky_0.png", (ID3D11Resource**)&g_pHdrTexture, &g_pHdrTextureSRV,false));

	XMVECTOR SceneMin, SceneMax;

	bool bIsSceneMinMaxInited = false;

	ID3D11DeviceContext* pD3dImmediateContext = DXUTGetD3D11DeviceContext();


#ifdef FORWARDPLUS
	//load the scene mesh
	g_SceneMesh.Create(pD3dDevice, L"sponza\\sponza.sdkmesh", nullptr);
	g_SceneAlphaMesh.Create(pD3dDevice, L"sponza\\sponza_alpha.sdkmesh", nullptr);
	g_SceneMesh.CalculateMeshMinMax(&SceneMin, &SceneMax);
	bIsSceneMinMaxInited = true;
	V_RETURN(s_ForwardPlusRender.OnCreateDevice(pD3dDevice, SceneMin, SceneMax));
#endif // FORWARDPLUS

#ifdef TRIANGLE

	s_TriangleRender.GenerateMeshData(100, 100, 100, 300.0f, 200.0f, 300.0f);

	if (!bIsSceneMinMaxInited)
	{
		ForwardRender::TriangleRender::CalculateBoundingBoxMinMax(s_TriangleRender.m_MeshData, &SceneMin, &SceneMax);
		bIsSceneMinMaxInited = true;
	}
	V_RETURN(s_TriangleRender.OnD3DDeviceCreated(pD3dDevice, pBackBufferSurfaceDesc, pUserContext));
#endif

#ifdef DECAL
	s_DeferredDecalRender.GenerateMeshData(101, 101, 101);
	if (!bIsSceneMinMaxInited)
	{
		PostProcess::DeferredDecalRender::CalculateSceneMinMax(s_DeferredDecalRender.m_MeshData, &SceneMin, &SceneMax);
		bIsSceneMinMaxInited = true;
	}

	V_RETURN(s_DeferredDecalRender.OnD3DDeviceCreated(pD3dDevice, pBackBufferSurfaceDesc, pUserContext));
	s_DeferredDecalRender.SetDecalTextureSRV(g_pDecalTextureSRV);







#endif

#ifdef GodRay

	V_RETURN(s_DeferredVoxelCutoutRender.OnD3DDeviceCreated(pD3dDevice, pBackBufferSurfaceDesc, pUserContext));

	V_RETURN(s_SphereRender.OnD3DDeviceCreated(pD3dDevice, pBackBufferSurfaceDesc, pUserContext));

	V_RETURN(s_RadialBlurRender.OnD3DDeviceCreated(pD3dDevice, pBackBufferSurfaceDesc, pUserContext));
	//s_RadialBlurRender.SetRadialBlurTextureSRV(g_pDecalTextureSRV);

	V_RETURN(s_ScreenBlendRender.OnD3DDeviceCreated(pD3dDevice, pBackBufferSurfaceDesc, pUserContext));
	//s_ScreenBlendRender.SetSrcTextureSRV(g_pDecalTextureSRV);
#endif


#ifdef PBR
	V_RETURN(s_CubeMapCaptureRender.OnD3DDeviceCreated(pD3dDevice, pBackBufferSurfaceDesc, pUserContext));
	V_RETURN(s_PbrRender.OnD3DDeviceCreated(pD3dDevice, pBackBufferSurfaceDesc, pUserContext));
	
#endif // PBR


	//Create state objects
	D3D11_SAMPLER_DESC SamplerDesc;
	ZeroMemory(&SamplerDesc, sizeof(SamplerDesc));
	SamplerDesc.Filter = D3D11_FILTER_ANISOTROPIC;
	SamplerDesc.AddressU = SamplerDesc.AddressV = SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	SamplerDesc.MaxAnisotropy = 16;
	SamplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
	V_RETURN(pD3dDevice->CreateSamplerState(&SamplerDesc, &g_pSamplerAnisotropic));
	g_pSamplerAnisotropic->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("Anisotropic"), "Anisotropic");


	//create AMD_SDK resource here
	TIMER_Init(pD3dDevice);
	g_HUD.OnCreateDevice(pD3dDevice);

	// Create render resource here

	static bool bCameraInit = false;
	if (!bCameraInit)
	{
#ifdef FORWARDPLUS
		// Setup the camera's view parameters
		XMVECTOR SceneCenter = 0.5f*(SceneMax + SceneMin);
		XMVECTOR SceneExtents = 0.5f*(SceneMax - SceneMin);
		XMVECTOR BoundaryMin = SceneCenter - 2.0f*SceneExtents;
		XMVECTOR BoundaryMax = SceneCenter + 2.0f*SceneExtents;
		XMVECTOR BoundaryDiff = 4.0f*SceneExtents;

		g_fMaxDistance = XMVectorGetX(XMVector3Length(BoundaryDiff));
		XMVECTOR vEye = XMVectorSet(150.0f, 200.0f, 0.0f, 1.0f);
		XMVECTOR vLookAtPos = XMVectorSet(150.0f, 200.0f, 100.0f, 0.0f);
		g_Camera.SetRotateButtons(true, false, false);
		//g_Camera.SetButtonMasks(MOUSE_MIDDLE_BUTTON, MOUSE_WHEEL, MOUSE_LEFT_BUTTON);//left_button can rotate camera
		g_Camera.SetEnablePositionMovement(true);
		g_Camera.SetViewParams(vEye, vLookAtPos);
		g_Camera.SetScalers(0.005f, 0.1f*g_fMaxDistance);

	//	s_DeferredVoxelCutoutRender.SetVoxelPosition(vLookAtPos);

#else
		// Setup the camera's view parameters
		XMVECTOR SceneCenter = 0.5f*(SceneMax + SceneMin);
		XMVECTOR SceneExtends = 0.5f*(SceneMax - SceneMin);
		XMVECTOR BoundaryMin = SceneCenter - 20.0f*SceneExtends;
		XMVECTOR BoundaryMax = SceneCenter + 20.0f*SceneExtends;
		XMVECTOR BoundaryDiff = 40.0f*SceneExtends;


		XMVECTOR vEye = XMVectorSet(0.0, 0.0f, 0.0f,0.0f);
		XMVECTOR vLookAtPos = SceneCenter + XMVectorSet(0.0f,0.0f, 10.0f, 0.0f);//要看到前面的剑法
		g_Camera.SetRotateButtons(true, false, false);
		g_Camera.SetEnablePositionMovement(true);
		g_Camera.SetViewParams(vEye, vLookAtPos);
		g_Camera.SetScalers(0.005f, 0.1f*g_fMaxDistance);
#endif


		XMFLOAT3 vBoundaryMin, vBoundaryMax;
		XMStoreFloat3(&vBoundaryMin, BoundaryMin);
		XMStoreFloat3(&vBoundaryMax, BoundaryMax);
		g_Camera.SetClipToBoundary(true, &vBoundaryMin, &vBoundaryMax);

#ifdef DECAL
#ifdef FORWARDPLUS
		s_DeferredDecalRender.SetDecalPosition(XMVectorSet(0.0f, 200.0f, 300.0f, 0.0f));

#else
		s_DeferredDecalRender.SetDecalPosition(SceneCenter);
#endif
#endif // DECAL


		bCameraInit = true;
	}

	// Generate shaders ( this is an async operation - call AMD::ShaderCache::ShadersReady() to find out if they are complete)
	if (s_bFirstPass)
	{
		AddShadersToCache();



		g_ShaderCache.GenerateShaders(AMD::ShaderCache::CREATE_TYPE_COMPILE_CHANGES);//Only compile shaders that have changed(development mode)

		s_bFirstPass = false;
	}


	V_RETURN(g_DialogResourceManager.OnD3D11CreateDevice(pD3dDevice, pD3dImmediateContext));
	V_RETURN(g_D3dSettingsGUI.OnD3D11CreateDevice(pD3dDevice));
	SAFE_DELETE(g_pTextHelper);
	g_pTextHelper = new CDXUTTextHelper(pD3dDevice, pD3dImmediateContext, &g_DialogResourceManager, TEXT_LINE_HEIGHT);

	return hr;
}

HRESULT AddShadersToCache()
{

	HRESULT hr = S_OK;

#ifdef FORWARDPLUS
	s_ForwardPlusRender.AddShadersToCache(&g_ShaderCache);
#endif // FORWARDPLUS

#ifdef TRIANGLE
	s_TriangleRender.AddShadersToCache(&g_ShaderCache);
#endif

#ifdef DECAL
	s_DeferredDecalRender.AddShadersToCache(&g_ShaderCache);
#endif

#ifdef GodRay
	s_DeferredVoxelCutoutRender.AddShadersToCache(&g_ShaderCache);
	s_SphereRender.AddShadersToCache(&g_ShaderCache);
	s_RadialBlurRender.AddShadersToCache(&g_ShaderCache);
	s_ScreenBlendRender.AddShadersToCache(&g_ShaderCache);
#endif

#ifdef PBR

	s_CubeMapCaptureRender.AddShadersToCache(&g_ShaderCache);
	s_PbrRender.AddShadersToCache(&g_ShaderCache);
#endif // PBR


	return hr;
}

HRESULT OnD3D11ResizedSwapChain(ID3D11Device * pD3dDevice, IDXGISwapChain * pSwapChain, const DXGI_SURFACE_DESC * pBackBufferSurfaceDesc, void * pUserContext)
{
	HRESULT hr; 

	g_Viewport.Width = pBackBufferSurfaceDesc->Width;
	g_Viewport.Height = pBackBufferSurfaceDesc->Height;



	V_RETURN(g_DialogResourceManager.OnD3D11ResizedSwapChain(pD3dDevice, pBackBufferSurfaceDesc));
	V_RETURN(g_D3dSettingsGUI.OnD3D11ResizedSwapChain(pD3dDevice, pBackBufferSurfaceDesc));

	//Set the location and size of AMD standard HUD
	g_HUD.m_GUI.SetLocation(pBackBufferSurfaceDesc->Width - AMD::HUD::iDialogWidth, 0);
	g_HUD.m_GUI.SetSize(AMD::HUD::iDialogWidth, pBackBufferSurfaceDesc->Height);
	g_HUD.OnResizedSwapChain(pBackBufferSurfaceDesc);


	// Setup the camera's projection parameter
	float fApsectRatio = pBackBufferSurfaceDesc->Width / (FLOAT)pBackBufferSurfaceDesc->Height;
	g_Camera.SetProjParams(XM_PI / 4, fApsectRatio, 1.0f, g_fMaxDistance);


	//create our own depth stencil surface that'bindable as a shader
	V_RETURN(AMD::CreateDepthStencilSurface(&g_pDepthStencilTexture, &g_pDepthStencilSRV, &g_pDepthStencilView,
		DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_R24_UNORM_X8_TYPELESS, pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height, pBackBufferSurfaceDesc->SampleDesc.Count));
	g_pDepthStencilTexture->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("DepthStencilTexture"), "DepthStencilTexture");
	g_pDepthStencilSRV->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("DepthStencilSRV"), "DepthStencilSRV");
	g_pDepthStencilView->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("DepthStencilView"), "DepthStencilView");

	//create our own depth stencil surface that'bindable as a shader
	V_RETURN(AMD::CreateDepthStencilSurface(&g_pTempDepthStencilTexture, &g_pTempDepthStencilSRV, &g_pTempDepthStencilView,
		DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_R24_UNORM_X8_TYPELESS, pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height, pBackBufferSurfaceDesc->SampleDesc.Count));
	g_pTempDepthStencilTexture->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("Temp DepthStencilTexture"), "Temp DepthStencilTexture");
	g_pTempDepthStencilSRV->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("Temp DepthStencilSRV"), "Temp DepthStencilSRV");
	g_pTempDepthStencilView->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("Temp DepthStencilView"), "Temp DepthStencilView");


	// Get the back buffer and desc
	ID3D11Texture2D* pBackBuffer;
	V_RETURN(pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer)));
	D3D11_TEXTURE2D_DESC backBufferSurfaceDesc;
	pBackBuffer->GetDesc(&backBufferSurfaceDesc);
	backBufferSurfaceDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	
	for (int i = 0; i < MAX_TEMP_SCENE_TEXTURE; ++i)
	{
		// Create the temp back buffer
		V_RETURN(pD3dDevice->CreateTexture2D(&backBufferSurfaceDesc, nullptr, &g_pTempTexture2D[i]));
		auto tempString = std::string("Temp Tx2D_")+std::to_string(i);
		g_pTempTexture2D[i]->SetPrivateData(WKPDID_D3DDebugObjectName, tempString.length(), tempString.data());

		// Create the render target view and SRV
		V_RETURN(pD3dDevice->CreateRenderTargetView(g_pTempTexture2D[i], nullptr, &g_pTempTextureRenderTargetView[i]));
		V_RETURN(pD3dDevice->CreateShaderResourceView(g_pTempTexture2D[i], nullptr, &g_pTempTextureSRV[i]));
	}
	


	SAFE_RELEASE(pBackBuffer);



	//jingz
#ifdef FORWARDPLUS
	V_RETURN(s_ForwardPlusRender.OnResizedSwapChain(pD3dDevice, pBackBufferSurfaceDesc));
#endif // FORWARDPLUS

#ifdef TRIANGLE
	s_TriangleRender.OnResizedSwapChain(pD3dDevice, pBackBufferSurfaceDesc);
#endif

#ifdef DECAL
	s_DeferredDecalRender.OnResizedSwapChain(pD3dDevice, pBackBufferSurfaceDesc);
#endif

#ifdef GodRay
	s_DeferredVoxelCutoutRender.OnResizedSwapChain(pD3dDevice, pBackBufferSurfaceDesc);
	s_SphereRender.OnResizedSwapChain(pD3dDevice, pBackBufferSurfaceDesc);
	s_RadialBlurRender.OnResizedSwapChain(pD3dDevice, pBackBufferSurfaceDesc);
	s_ScreenBlendRender.OnResizedSwapChain(pD3dDevice, pBackBufferSurfaceDesc);
#endif

#ifdef PBR
	s_CubeMapCaptureRender.OnResizedSwapChain(pD3dDevice, pBackBufferSurfaceDesc);
	s_PbrRender.OnResizedSwapChain(pD3dDevice, pBackBufferSurfaceDesc);
#endif // PBR


	return hr;
}

void OnD3D11ReleasingSwapChain(void * pUserContext)
{
	//Release my RT/SRV/Buffer

	SAFE_RELEASE(g_pDepthStencilView);
	SAFE_RELEASE(g_pDepthStencilSRV);
	SAFE_RELEASE(g_pDepthStencilTexture);


	SAFE_RELEASE(g_pTempDepthStencilView);
	SAFE_RELEASE(g_pTempDepthStencilSRV);
	SAFE_RELEASE(g_pTempDepthStencilTexture);


	for (int i = 0; i < MAX_TEMP_SCENE_TEXTURE; ++i)
	{
		SAFE_RELEASE(g_pTempTexture2D[i]);
		SAFE_RELEASE(g_pTempTextureRenderTargetView[i]);
		SAFE_RELEASE(g_pTempTextureSRV[i]);
	}

#ifdef FORWARDPLUS
	s_ForwardPlusRender.OnReleasingSwapChain();
#endif
#ifdef TRIANGLE
	s_TriangleRender.OnReleasingSwapChain();
#endif
#ifdef DECAL
	s_DeferredDecalRender.OnReleasingSwapChain();
#endif

#ifdef GodRay
	s_DeferredVoxelCutoutRender.OnReleasingSwapChain();
	s_SphereRender.OnReleasingSwapChain();
	s_RadialBlurRender.OnReleasingSwapChain();
	s_ScreenBlendRender.OnReleasingSwapChain();
#endif

#ifdef PBR
	s_CubeMapCaptureRender.OnReleasingSwapChain();
	s_PbrRender.OnReleasingSwapChain();
#endif // PBR

	g_DialogResourceManager.OnD3D11ReleasingSwapChain();
}

void OnD3D11DestroyDevice(void * pUserContext)
{
	g_DialogResourceManager.OnD3D11DestroyDevice();
	g_D3dSettingsGUI.OnD3D11DestroyDevice();
	SAFE_DELETE(g_pTextHelper);
	DXUTGetGlobalResourceCache().OnDestroyDevice();

	g_SceneMesh.Destroy();
	g_SceneAlphaMesh.Destroy();


	SAFE_RELEASE(g_pSamplerAnisotropic);
	SAFE_RELEASE(g_pDepthStencilDefaultDS);

	SAFE_RELEASE(g_pDepthStencilView);
	SAFE_RELEASE(g_pDepthStencilSRV);
	SAFE_RELEASE(g_pDepthStencilTexture);

	SAFE_RELEASE(g_pTempDepthStencilView);
	SAFE_RELEASE(g_pTempDepthStencilSRV);
	SAFE_RELEASE(g_pTempDepthStencilTexture);

	SAFE_RELEASE(g_pDecalTexture);
	SAFE_RELEASE(g_pDecalTextureSRV);

	SAFE_RELEASE(g_pHdrTexture);
	SAFE_RELEASE(g_pHdrTextureSRV);



	for (int i = 0; i < MAX_TEMP_SCENE_TEXTURE; ++i)
	{
		SAFE_RELEASE(g_pTempTexture2D[i]);
		SAFE_RELEASE(g_pTempTextureRenderTargetView[i]);
		SAFE_RELEASE(g_pTempTextureSRV[i]);
	}

#ifdef FORWARDPLUS
	s_ForwardPlusRender.OnDestroyDevice(pUserContext);
#endif // FORWARDPLUS
#ifdef TRIANGLE
	s_TriangleRender.OnD3D11DestroyDevice(pUserContext);
#endif
#ifdef DECAL
	s_DeferredDecalRender.OnD3D11DestroyDevice(pUserContext);
#endif

#ifdef GodRay
	s_DeferredVoxelCutoutRender.OnD3D11DestroyDevice(pUserContext);
	s_SphereRender.OnD3D11DestroyDevice(pUserContext);
	s_RadialBlurRender.OnD3D11DestroyDevice(pUserContext);
	s_ScreenBlendRender.OnD3D11DestroyDevice(pUserContext);
#endif

#ifdef PBR
	s_CubeMapCaptureRender.OnD3D11DestroyDevice(pUserContext);
	s_PbrRender.OnD3D11DestroyDevice(pUserContext);
#endif // PBR
	
	//AMD
	g_ShaderCache.OnDestroyDevice();
	g_HUD.OnDestroyDevice();
	TIMER_Destroy();

	s_bFirstPass = true;
}


//----------------------------------------------
// Striped down version of DXUT ClearD3D11DeviceConetext.
// For this sample, the HS,DS,and GS are not used.
// And it is assumed that drawing code will always call
// VSSetShader, PSSetShader,IASetVertexBuffers,IASetIndexBuffer(if applicable),and IASetInputLayout.
void ClearD3D11DeviceContext()
{
	ID3D11DeviceContext* pD3dDeviceContext = DXUTGetD3D11DeviceContext();

	ID3D11ShaderResourceView* pSRVs[16] =
	{
		nullptr,nullptr,nullptr,nullptr,
		nullptr,nullptr,nullptr,nullptr,
		nullptr,nullptr,nullptr,nullptr,
		nullptr,nullptr,nullptr,nullptr
	};

	ID3D11RenderTargetView* pRTVs[16]=
	{
		nullptr,nullptr,nullptr,nullptr,
		nullptr,nullptr,nullptr,nullptr,
		nullptr,nullptr,nullptr,nullptr,
		nullptr,nullptr,nullptr,nullptr
	};

	ID3D11DepthStencilView* pDSV = nullptr;

	ID3D11Buffer* pBuffers[16] = 
	{
		nullptr,nullptr,nullptr,nullptr,
		nullptr,nullptr,nullptr,nullptr,
		nullptr,nullptr,nullptr,nullptr,
		nullptr,nullptr,nullptr,nullptr
	};

	ID3D11SamplerState* pSamplers[16]=
	{
		nullptr,nullptr,nullptr,nullptr,
		nullptr,nullptr,nullptr,nullptr,
		nullptr,nullptr,nullptr,nullptr,
		nullptr,nullptr,nullptr,nullptr
	};

	//Constant buffers
	pD3dDeviceContext->VSSetConstantBuffers(0, 14, pBuffers);//jingz todo 为什么是14，constantbuffer不是有1024？
	pD3dDeviceContext->PSSetConstantBuffers(0, 14, pBuffers);
	pD3dDeviceContext->CSSetConstantBuffers(0, 14, pBuffers);

	//SRV
	pD3dDeviceContext->VSSetShaderResources(0, 16, pSRVs);
	pD3dDeviceContext->PSSetShaderResources(0, 16, pSRVs);
	pD3dDeviceContext->CSSetShaderResources(0, 16, pSRVs);

	//SamplerStates
	pD3dDeviceContext->VSSetSamplers(0, 16, pSamplers);
	pD3dDeviceContext->PSSetSamplers(0, 16, pSamplers);
	pD3dDeviceContext->CSSetSamplers(0, 16, pSamplers);

	// Render targets
	pD3dDeviceContext->OMSetRenderTargets(8, pRTVs, pDSV);

	// States
	FLOAT BlendFactor[4] ={ 0,0,0,0 };
	pD3dDeviceContext->OMSetBlendState(nullptr, BlendFactor, 0xFFFFFFFF);
	pD3dDeviceContext->OMSetDepthStencilState(nullptr,0x00);
	pD3dDeviceContext->RSSetState(nullptr);

	pD3dDeviceContext = nullptr;
}

// ---------------
// Render the scene using the D3D11 Device
void OnFrameRender(ID3D11Device * pD3dDevice, ID3D11DeviceContext * pD3dImmediateContext, double fTime, float fElapsedTime, void* pUserContext)
{

	// Reset the timer at start of frame
	TIMER_Reset();
	
	// If the settings dialog is being shown, then render it instead of rendering the app's scene
	if (g_D3dSettingsGUI.IsActive())
	{
		g_D3dSettingsGUI.OnRender(fElapsedTime);
		return;
	}

	//本地清理环境
	ClearD3D11DeviceContext();


	DXUTSetupD3D11Views(pD3dImmediateContext);//设置viewport和rt/ds

	const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc = DXUTGetDXGIBackBufferSurfaceDesc();

	pD3dImmediateContext->RSSetViewports(1, &g_Viewport);



	//Clear the backBuffer and depth stencil
	float ClearColor[4] = { 0.0013f, 0.0015f, 0.0050f, 0.0f };
	ID3D11RenderTargetView* pRTV = DXUTGetD3D11RenderTargetView();
	pD3dImmediateContext->ClearRenderTargetView(pRTV, ClearColor);
	pD3dImmediateContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	// Switch off alpha blending
	// float BlendFactor[4] = { 1.0f,1.0f,0.0f,0.0f }; 这些参数没什么用
	pD3dImmediateContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);

	//Render or objects here..
	if (g_ShaderCache.ShadersReady(false))
	{
		TIMER_Begin(0, L"Render");
		//render triangle for debug
		pD3dImmediateContext->OMSetDepthStencilState(nullptr, 0x00);
		pD3dImmediateContext->RSSetState(nullptr);

		std::vector< CDXUTSDKMesh*>MeshArray;
		MeshArray.push_back(&g_SceneMesh);

		std::vector< CDXUTSDKMesh*>AlphaMeshArray;
		AlphaMeshArray.push_back(&g_SceneAlphaMesh);

#ifdef FORWARDPLUS
		s_ForwardPlusRender.OnRender(pD3dDevice, pD3dImmediateContext, &g_Camera, pRTV, pBackBufferSurfaceDesc,
			g_pDepthStencilTexture, g_pDepthStencilView, g_pDepthStencilSRV,
			fElapsedTime, MeshArray.data(), 1, AlphaMeshArray.data(), 1, g_iNumActivePointLights, g_iNumActiveSpotLights);
#endif // FORWARDPLUS


#ifdef TRIANGLE
		pD3dImmediateContext->RSSetState(nullptr);
		pD3dImmediateContext->OMSetDepthStencilState(g_pDepthStencilDefaultDS, 0x00);

		s_TriangleRender.OnRender(pD3dDevice, pD3dImmediateContext, &g_Camera, pRTV, g_pDepthStencilView);
#endif

#ifdef DECAL
		ID3D11RenderTargetView* pNullRTV = nullptr;
		pD3dImmediateContext->OMSetRenderTargets(1, &pNullRTV, nullptr);
		pD3dImmediateContext->CopyResource(g_pTempDepthStencilTexture, g_pDepthStencilTexture);
		s_DeferredDecalRender.OnRender(pD3dDevice, pD3dImmediateContext, pBackBufferSurfaceDesc, &g_Camera, pRTV, g_pTempDepthStencilView, g_pDepthStencilSRV);


#endif

#ifdef GodRay
		pD3dImmediateContext->CopyResource(g_pTempDepthStencilTexture, g_pDepthStencilTexture);
		s_DeferredVoxelCutoutRender.OnRender(pD3dDevice, pD3dImmediateContext, pBackBufferSurfaceDesc, &g_Camera, g_pTempTextureRenderTargetView[0], g_pTempDepthStencilView, g_pDepthStencilSRV);

		//只绘制DepthStencil信息
		pD3dImmediateContext->ClearDepthStencilView(g_pTempDepthStencilView, D3D11_CLEAR_STENCIL, 1.0f, 0);
		s_SphereRender.OnRender(pD3dDevice, pD3dImmediateContext, &g_Camera, pRTV, g_pTempDepthStencilView);

		auto p = s_DeferredVoxelCutoutRender.GetVoxelPosition();
		XMMATRIX mWorld = DirectX::XMMatrixIdentity();
		mWorld.r[3].m128_f32[0] = p.x;
		mWorld.r[3].m128_f32[1] = p.y;
		mWorld.r[3].m128_f32[2] = p.z;
		//Get the projection & view matrix from the camera class
		XMMATRIX mView = g_Camera.GetViewMatrix();
		XMMATRIX mProj = g_Camera.GetProjMatrix();
		XMMATRIX mWorldViewPrjection = mWorld*mView*mProj;
		XMVECTOR temp = DirectX::XMVector4Transform(DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f),mWorldViewPrjection);

		XMFLOAT4 uv;
		DirectX::XMStoreFloat4(&uv, temp);
		s_RadialBlurRender.SetRadialBlurCenter(uv.x/ uv.w*0.5f+0.5f,0.5f-uv.y / uv.w*0.5f);
		
		s_RadialBlurRender.SetRadialBlurTextureSRV(g_pTempTextureSRV[0]);
		s_RadialBlurRender.OnRender(pD3dDevice, pD3dImmediateContext, pBackBufferSurfaceDesc, &g_Camera, g_pTempTextureRenderTargetView[1], g_pTempDepthStencilView, g_pDepthStencilSRV);
		s_ScreenBlendRender.SetBlendTextureSRV(g_pTempTextureSRV[1]);
		s_ScreenBlendRender.OnRender(pD3dDevice, pD3dImmediateContext, pBackBufferSurfaceDesc, &g_Camera, pRTV, g_pTempDepthStencilView, g_pDepthStencilSRV);
#endif

#ifdef PBR

		s_CubeMapCaptureRender.SetSrcTextureSRV(g_pHdrTextureSRV);
		s_CubeMapCaptureRender.RenderHDRtoCubeMap(pD3dDevice, pD3dImmediateContext, pBackBufferSurfaceDesc, &g_Camera, nullptr, nullptr);

		s_CubeMapCaptureRender.RenderIrradiance(pD3dDevice, pD3dImmediateContext, pBackBufferSurfaceDesc,&g_Camera);
		s_CubeMapCaptureRender.RenderPrefilter(pD3dDevice, pD3dImmediateContext, pBackBufferSurfaceDesc, &g_Camera, pRTV, g_pTempDepthStencilView);



		////s_PbrRender
		D3D11_VIEWPORT vp;
		vp.Width = (FLOAT)pBackBufferSurfaceDesc->Width;
		vp.Height = (FLOAT)pBackBufferSurfaceDesc->Height;
		vp.MinDepth = 0;
		vp.MaxDepth = 1;
		vp.TopLeftX = 0;
		vp.TopLeftY = 0;
		int Count = 1;
		pD3dImmediateContext->RSSetViewports(Count, &vp);
		s_PbrRender.OnRender(pD3dDevice, pD3dImmediateContext, pBackBufferSurfaceDesc, &g_Camera, pRTV, g_pDepthStencilView, s_CubeMapCaptureRender.GetIrradianceSRV(), s_CubeMapCaptureRender.GetPrefilterSRV());

#endif // PBR


		pD3dImmediateContext->OMSetRenderTargets(1, &pRTV, g_pDepthStencilView);

		TIMER_End(); // Render


		RenderText();


		if (g_bRenderHUD)
		{
			g_HUD.OnRender(fElapsedTime);
		}

	}
	else
	{
		// Render shader cache progress if still processing
		pD3dImmediateContext->OMSetRenderTargets(1, (ID3D11RenderTargetView*const*)&pRTV, g_pDepthStencilView);
		g_ShaderCache.RenderProgress(g_pTextHelper, 15, XMVectorSet(1.0f, 1.0f, 0.0f, 1.0f));
	}

}

void RenderText()
{
	g_pTextHelper->Begin();
	g_pTextHelper->SetInsertionPos(5, 5);
	g_pTextHelper->SetForegroundColor(XMVectorSet(1.0f, 1.0f, 0.0f, 1.0f));
	g_pTextHelper->DrawTextLine(DXUTGetFrameStats(DXUTIsVsyncEnabled()));
	g_pTextHelper->DrawTextLine(DXUTGetDeviceStats());

	const float fGpuTime = (float)TIMER_GetTime(Gpu, L"Render")*1000.0f;

	// count digits in the total time;
	int iIntergetPart = (int)fGpuTime;
	int iNumDigits = 0;
	while (iIntergetPart > 0)
	{
		iIntergetPart /= 10;
		iNumDigits++;
	}
	iNumDigits = (iNumDigits == 0) ? 1 : iNumDigits;
	//three digits after decimal,
	// plus the decimal point itself
	int iNumChars = iNumDigits + 4;

	//dynamic formating for sprintf_s
	WCHAR szPrecision[16];
	swprintf_s(szPrecision, 16, L"%%%d.3f", iNumChars);

	WCHAR szBuf[256];
	WCHAR szFormat[256];
	swprintf_s(szFormat, 256, L"Total:        %s", szPrecision);
	swprintf_s(szBuf, 256, szFormat, fGpuTime);
	g_pTextHelper->DrawTextLine(szBuf);


	const float fGpuTimeDepthPrePass = (float)TIMER_GetTime(Gpu, L"Render|ForwardPlus|Depth pre-pass") * 1000.0f;
	swprintf_s(szFormat, 256, L"+----Z Pass: %s", szPrecision);
	swprintf_s(szBuf, 256, szFormat, fGpuTimeDepthPrePass);
	g_pTextHelper->DrawTextLine(szBuf);

	const float fGpuTimeLightCulling = (float)TIMER_GetTime(Gpu, L"Render|ForwardPlus|Light culling") * 1000.0f;
	swprintf_s(szFormat, 256, L"+------Cull: %s", szPrecision);
	swprintf_s(szBuf, 256, szFormat, fGpuTimeLightCulling);
	g_pTextHelper->DrawTextLine(szBuf);

	const float fGpuTimeForwardRendering = (float)TIMER_GetTime(Gpu, L"Render|ForwardPlus|Forward rendering") * 1000.0f;
	swprintf_s(szFormat, 256, L"+---Forward: %s", szPrecision);
	swprintf_s(szBuf, 256, szFormat, fGpuTimeForwardRendering);
	g_pTextHelper->DrawTextLine(szBuf);

	g_pTextHelper->SetInsertionPos(5, DXUTGetDXGIBackBufferSurfaceDesc()->Height - AMD::HUD::iElementDelta);
	g_pTextHelper->DrawTextLine(L"Toggle GUI    : F1");

	g_pTextHelper->End();
}

void InitApp()
{

	g_Viewport.MinDepth = 0;
	g_Viewport.MaxDepth = 1;
	g_Viewport.TopLeftX = 0;
	g_Viewport.TopLeftY = 0;
	g_Viewport.Width = 1920;
	g_Viewport.Height = 1080;

	g_D3dSettingsGUI.Init(&g_DialogResourceManager);


	WCHAR szTemp[256];

	D3DCOLOR DlgColor = 0x88888888;//Semi-transparent background for the dialog

	auto& GUI = g_HUD.m_GUI;

	GUI.Init(&g_DialogResourceManager);
	GUI.SetBackgroundColors(DlgColor);
	GUI.SetCallback(OnGUIEvent);

	int iY = AMD::HUD::iElementDelta;

	GUI.AddButton(IDC_BUTTON_TOGGLEFULLSCREEN, L"Toggle full screen", AMD::HUD::iElementOffset, iY, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, VK_F11);
	GUI.AddButton(IDC_BUTTON_CHANGEDEVICE, L"Change device (F2)", AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, VK_F2);

	iY += AMD::HUD::iGroupDelta;

	//Point Light
	GUI.AddCheckBox(IDC_CHECKBOX_ENABLE_LIGHT_DRAWING, L"Show Lights", AMD::HUD::iElementOffset, iY, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, false);
	swprintf_s(szTemp, L"Active Point Lights : %d", g_iNumActivePointLights);
	GUI.AddStatic(IDC_STATIC_NUM_POINT_LIGHTS, szTemp, AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight);
	GUI.AddSlider(IDC_SLIDER_NUM_POINT_LIGHTS, AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, 0, ForwardPlus11::MAX_NUM_LIGHTS, g_iNumActivePointLights, true);

	swprintf_s(szTemp, L"Active Spot Lights : %d", g_iNumActiveSpotLights);
	GUI.AddStatic(IDC_STATIC_NUM_SPOT_LIGHTS, szTemp, AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight);
	GUI.AddSlider(IDC_SLIDER_NUM_SPOT_LIGHTS, AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, 0, ForwardPlus11::MAX_NUM_LIGHTS, g_iNumActiveSpotLights);

	iY += AMD::HUD::iGroupDelta;

	GUI.AddCheckBox(IDC_CHECKBOX_ENABLE_LIGHT_CULLING, L"Enable Light Culling", AMD::HUD::iElementOffset, iY, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, true);
	GUI.AddCheckBox(IDC_CHECKBOX_ENABLE_DEPTH_BOUNDS, L"Enable Depth Bounds", AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, true);
	GUI.AddCheckBox(IDC_CHECKBOX_ENABLE_DEBUG_DRAWING, L"Show Lights Per Tile", AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, false);
	GUI.AddRadioButton(IDC_RADIOBUTTON_DEBUG_DRAWING_ONE, IDC_TILE_DRAWING_GROUP, L"Radar Colors", 2 * AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth - AMD::HUD::iElementOffset, AMD::HUD::iElementHeight, true);
	GUI.AddRadioButton(IDC_RADIOBUTTON_DEBUG_DRAWING_TWO, IDC_TILE_DRAWING_GROUP, L"Grayscale", 2 * AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth - AMD::HUD::iElementOffset, AMD::HUD::iElementHeight, false);
	GUI.GetRadioButton(IDC_RADIOBUTTON_DEBUG_DRAWING_ONE)->SetEnabled(false);
	GUI.GetRadioButton(IDC_RADIOBUTTON_DEBUG_DRAWING_TWO)->SetEnabled(false);

	iY += AMD::HUD::iGroupDelta;

}

void SaveCubeMap();
//--------------------------------------------------------------------------------------
// Handle key presses
//--------------------------------------------------------------------------------------
void CALLBACK OnKeyboard(UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext)
{
	if (bKeyDown)
	{
		switch (nChar)
		{
		case VK_F1:
			g_bRenderHUD = !g_bRenderHUD;
			break;

		case  VK_F9:
			SaveCubeMap();
			break;
		}
	}
}

//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent(UINT nEvent, int nControlID, CDXUTControl * pControl, void * pUserContext)
{
	WCHAR szTemp[256];

	switch (nControlID)
	{
	case IDC_BUTTON_TOGGLEFULLSCREEN:
		DXUTToggleFullScreen();
		break;
	case IDC_BUTTON_CHANGEDEVICE:
		g_D3dSettingsGUI.SetActive(!g_D3dSettingsGUI.IsActive());
		break;
	case IDC_SLIDER_NUM_POINT_LIGHTS:
	{
		// update
		g_iNumActivePointLights = ((CDXUTSlider*)pControl)->GetValue();
		swprintf_s(szTemp, L"Active Point Lights : %d", g_iNumActivePointLights);
		g_HUD.m_GUI.GetStatic(IDC_STATIC_NUM_POINT_LIGHTS)->SetText(szTemp);
	}
	break;
	case IDC_SLIDER_NUM_SPOT_LIGHTS:
	{
		// update
		g_iNumActiveSpotLights = ((CDXUTSlider*)pControl)->GetValue();
		swprintf_s(szTemp, L"Active Spot Lights : %d", g_iNumActiveSpotLights);
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
	}
}



void SaveCubeMap()
{
	ID3D11DeviceContext* pD3dDeviceContext = DXUTGetD3D11DeviceContext();

	ID3D11Resource* tempTexture2D = nullptr;

#ifdef EXPORT_CUBEMAP
	for (int i = 0; i < 6; ++i)
	{
		wchar_t  strPath[128];
		
		swprintf(strPath, 128, L"irradiane_%d_32x32.jpg", i);
		s_CubeMapCaptureRender.GetIrradiance32x32(i)->GetResource(&tempTexture2D);
		DXUTSaveTextureToFile(pD3dDeviceContext, tempTexture2D, false, strPath);
		SAFE_RELEASE(tempTexture2D);

		swprintf(strPath, 128, L"icecube_%d_8x8.jpg", i);
		s_CubeMapCaptureRender.GetPrefilter8x8(i)->GetResource(&tempTexture2D);
		DXUTSaveTextureToFile(pD3dDeviceContext, tempTexture2D, false, strPath);
		SAFE_RELEASE(tempTexture2D);

		swprintf(strPath, 128, L"icecube_%d_16x16.jpg", i);
		s_CubeMapCaptureRender.GetPrefilter16x16(i)->GetResource(&tempTexture2D);
		DXUTSaveTextureToFile(pD3dDeviceContext, tempTexture2D, false, strPath);
		SAFE_RELEASE(tempTexture2D);

		swprintf(strPath, 128, L"icecube_%d_32x32.jpg", i);
		s_CubeMapCaptureRender.GetPrefilter32x32(i)->GetResource(&tempTexture2D);
		DXUTSaveTextureToFile(pD3dDeviceContext, tempTexture2D, false, strPath);
		SAFE_RELEASE(tempTexture2D);

		swprintf(strPath, 128, L"icecube_%d_64x64.jpg", i);
		s_CubeMapCaptureRender.GetPrefilter64x64(i)->GetResource(&tempTexture2D);
		DXUTSaveTextureToFile(pD3dDeviceContext, tempTexture2D, false, strPath);
		SAFE_RELEASE(tempTexture2D);

		swprintf(strPath, 128, L"icecube_%d_128x128.jpg", i);
		s_CubeMapCaptureRender.GetPrefilter128x128(i)->GetResource(&tempTexture2D);
		DXUTSaveTextureToFile(pD3dDeviceContext, tempTexture2D, false, strPath);
		SAFE_RELEASE(tempTexture2D);
	}
#endif

	//ID3D11Resource* tempTexture2D = nullptr;
	//s_CubeMapCaptureRender.GetIrradianceSRV()->GetResource(&tempTexture2D);
	//DXUTSaveTextureToFile(pD3dDeviceContext, tempTexture2D, true, L"Irradiance.dds");
	//SAFE_RELEASE(tempTexture2D);

	//s_CubeMapCaptureRender.GetPrefilterSRV()->GetResource(&tempTexture2D);
	//DXUTSaveTextureToFile(pD3dDeviceContext, static_cast<ID3D11Texture2D*>(tempTexture2D), true, L"Prefilter.dds");
	//SAFE_RELEASE(tempTexture2D);

	
}