// TestTriangle.cpp : Defines the entry point for the application.
//

#include "DepthOfField_Sample.h"
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


//AMD includes
#include "AMD_DepthOfFieldFX.h"
#include "../source/Shaders/inc/VS_FULLSCREEN.inc"
#include "../source/Shaders/inc/VS_SCREENQUAD.inc"
#include "../source/Shaders/inc/PS_FULLSCREEN.inc"
#include "../source/AMD_Lib/AMD_Texture2D.h"
#include "../src/AMD_DepthOfFieldFX_Opaque.h"

//#define FORWARDPLUS
//#define TRIANGLE
//#define DECAL
#define DOF_GUI


#define MAX_TEMP_SCENE_TEXTURE 2

using namespace DirectX;
using namespace Triangle;

#pragma warning( disable : 4100 ) // disable unreference formal parameter warnings for /W4 builds
//-----------------------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------------------
static const int TEXT_LINE_HEIGHT = 15;

#ifdef FORWARDPLUS
static ForwardPlus11::ForwardPlusRender s_ForwardPlusRender;
#endif // FORWARDPLUS

#ifdef TRIANGLE
static Triangle::TriangleRender s_TriangleRender;
#endif

#ifdef DECAL
static PostProcess::DeferredDecalRender s_DeferredDecalRender;
static PostProcess::DeferredVoxelCutoutRender s_DeferredVoxelCutoutRender;

static PostProcess::SphereRender s_SphereRender;
static PostProcess::RadialBlurRender s_RadialBlurRender;
static PostProcess::ScreenBlendRender s_ScreenBlendRender;

#endif



// Direct3D 11 resources
CDXUTSDKMesh g_SceneMesh;
CDXUTSDKMesh g_SceneAlphaMesh;



struct float2
{
	union
	{
		struct
		{
			float x, y;
		};

		float v[2];
	};
};

struct float3
{
	union
	{
		struct
		{
			float x, y, z;
		};
		float v[3];
	};
};

struct float4
{
	union
	{
		DirectX::XMVECTOR v;
		float f[4];
		struct
		{
			float x, y, z, w;
		};
	};


	float4(float ax = 0, float ay = 0, float az = 0, float aw = 0)
	{
		x = ax;
		y = ay;
		z = az;
		w = aw;
	}

	float4(DirectX::XMVECTOR av)
	{
		v = av;
	}

	float4(DirectX::XMVECTOR xyz, float aw)
	{
		v = xyz;
		w = aw;
	}

	inline operator DirectX::XMVECTOR() const
	{
		return v;
	}

	inline operator const float*() const
	{
		return f;
	}

#if !defined(_XM_NO_INTRINSICS_) && defined(_XM_SSE_INTRINSICS_)
	inline operator __m128i() const
	{
		return _mm_castps_si128(v);
	}

	inline operator __m128d() const
	{
		return _mm_castps_pd(v);
	}

#endif // 


};

#define float4x4 DirectX::XMMATRIX

// The declaration of S_CAMERA_DESC matches the declaration of Camera type defined
// in AMD_Types and used throughout the libraries
__declspec(align(16)) typedef struct S_CAMERA_DESC_t
{
	float4x4 m_View;
	float4x4 m_Projection;
	float4x4 m_ViewProjection;
	float4x4 m_View_Inv;
	float4x4 m_Projection_Inv;
	float4x4 m_ViewProjection_Inv;
	float3 m_Position;
	float m_Fov;
	float3 m_Direction;
	float m_FarPlane;
	float3 m_Right;
	float m_NearPlane;
	float3 m_Up;
	float m_Aspect;
	float4 m_Color;
} S_CAMERA_DESC;

__declspec(align(16)) typedef struct S_MODEL_DESC_t
{
	float4x4 m_World;
	float4x4 m_World_Inv;
	float4x4 m_WorldView;
	float4x4 m_WorldView_Inv;
	float4x4 m_WorldViewProjection;
	float4x4 m_WorldViewProjection_Inv;
	float4 m_Position;
	float4 m_Orientation;
	float4 m_Scale;
	float4 m_Ambient;
	float4 m_Diffuse;
	float4 m_Specular;
} S_MODEL_DESC;


//DepthOfField
CDXUTSDKMesh g_TankMesh;
S_MODEL_DESC g_ModelDesc;



//-------------------------------------
// AMD helper classes defined here
//-------------------------------------

static AMD::ShaderCache g_ShaderCache;

ID3D11SamplerState* g_pSamplerAnisotropic = nullptr;
ID3D11SamplerState* g_pSamplerLinearWrap = nullptr;

ID3D11RasterizerState* g_pBackCullingSolidRS = nullptr;
ID3D11RasterizerState* g_pNoCullingSolidRS = nullptr;

ID3D11BlendState* g_pOpaqueBS = nullptr;


ID3D11DepthStencilState* g_pDepthStencilDefaultDS = nullptr;
ID3D11DepthStencilState* g_pDepthLessEqualDS = nullptr;


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


AMD::Texture2D g_appColorBuffer;
AMD::Texture2D g_appDepthBuffer;
AMD::Texture2D g_appDofSurface;
AMD::Texture2D g_appCircleOfConfusionTexture;

//GUI
static bool g_bRenderHUD = true;
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


	//DepthOfField
	IDC_CHECKBOX_SHOW_DOF_RESULT,
	IDC_CHECKBOX_DEBUG_CIRCLE_OF_CONFUSION,

	IDC_COMBOBOX_DOF_METHOD,

	IDC_STATIC_FOCAL_DISTANCE,
	IDC_SLIDER_FOCAL_DISTANCE,
	IDC_STATIC_FSTOP,
	IDC_SLIDER_FSTOP,
	IDC_STATIC_FOCAL_LENGTH,
	IDC_SLIDER_FOCAL_LENGTH,
	IDC_STATIC_SENSOR_WIDTH,
	IDC_SLIDER_SENSOR_WIDTH,
	IDC_STATIC_MAX_RADIUS,
	IDC_SLIDER_MAX_RADIUS,
	IDC_STATIC_FORCE_COC,
	IDC_SLIDER_FORCE_COC,

	IDC_BUTTON_SAVE_SCREEN_SHOT,



	//Total IDC Count
	IDC_NUM_CONTROL_IDS

};


DepthOfFieldMode g_depthOfFieldMode = DOF_FastFilterSpread;
static int g_defaultCameraParameterIndex = 0;

struct CameraParameters
{
	float4 vecEye;
	float4 vecAt;
	float focalLength;
	float focusDistance;
	float sensorWidth;
	float fStop;//ratioFocalLengthToLensDiameter
};

static const CameraParameters g_defaultCameraParameters[] = {
	{ { 20.2270432f, 4.19414091f, 16.7282600f },{ 19.4321709f, 4.09884357f, 16.1290131f }, 400.0f, 21.67f, 100.0f, 1.4f, },
	{ { -14.7709570f, 5.55706882f, -17.5470028f },{ -14.1790190f, 5.42186546f, -16.7524414f }, 218.0f, 23.3f, 100.0f, 1.6f, },
	{ { 2.34538126f, -0.0807961449f, -12.6757965f },{ 2.23687410f, 0.0531809852f, -11.6907701f }, 190.0f, 14.61f, 100.0f, 1.8f, },
	{ { 25.5143566f, 5.54141998f, -20.4762344f },{ 24.8163872f, 5.42109346f, -19.7702885f }, 133.0f, 34.95f, 50.0f, 1.6f, },
	{ { 5.513732f, 0.803944f, -18.025604f },{ 5.315537f, 0.848312f, -17.046444f }, 205.0f, 39.47f, 85.4f, 2.6f },
	{ { -15.698505f, 6.656400f, -21.832394f },{ -15.187683f, 6.442449f, -20.999754f }, 229.0f, 11.3f, 100.00f, 3.9f },
	{ { 10.018296f, 0.288034f, -1.364868f },{ 9.142344f, 0.441804f, -0.907634f }, 157.0f, 10.9f, 100.00f, 2.2f },
	{ { -3.399786f, 0.948747f, -15.984277f },{ -3.114154f, 1.013084f, -15.028101f }, 366.0f, 16.8f, 100.00f, 1.4f },
	{ { -14.941996f, 4.904000f, -17.381784f },{ -14.348591f, 4.798616f, -16.583803f }, 155.0f, 24.9f, 42.70f, 1.4f },
	
};

#define MAX_DOF_RADIUS 64
float g_FocalLength = 190.0f;// in mm
float g_FocalDistance = 14.61f; // in meters
float g_SensorWidth = 100.0f;//in mm
float g_fStop = 1.8f;//jingz 焦距比镜头直径的比率值
float g_ForceCircleOfConfusion = 0.0f;
unsigned int g_MaxRadius = 57;
unsigned int g_ScaleFactor = 30;
unsigned int g_BoxScaleFactor = 24;

bool g_bShowDepthOfFieldResult = true;
bool g_bDebugCircleOfConfusion = false;
bool g_bSaveScreenShot = false;

// DepthOfFieldFX global variables
AMD::DEPTHOFFIELDFX_DESC g_AMD_DofFX_Desc;

ID3D11InputLayout* g_pD3DModelInputLayout = nullptr;
ID3D11VertexShader* g_pD3DModelVS = nullptr;
ID3D11PixelShader* g_pD3DModelPS = nullptr;
ID3D11Buffer* g_pD3DModelConstBuffer = nullptr;

ID3D11ComputeShader* g_pCalcCirlceOfConfusionCS = nullptr;
ID3D11ComputeShader* g_pDebugCirlceOfConfusionCS = nullptr;

ID3D11ComputeShader* g_pDoubleVerticalIntegrateCS = nullptr;
ID3D11ComputeShader* g_pVerticalIntegrateCS = nullptr;
ID3D11ComputeShader* g_pFastFilterSetupCS = nullptr;
ID3D11ComputeShader* g_pReadFinalResultCS = nullptr;
ID3D11ComputeShader* g_pFastFilterSetupQuarterResCS = nullptr;
ID3D11ComputeShader* g_pBoxFastFilterSetupCS = nullptr;



ID3D11Buffer* g_pD3DViewerConsterBuffer = nullptr;
ID3D11Buffer* g_pD3DCalcDofConsterBuffer = nullptr;

ID3D11VertexShader* g_pFullScreenVS = nullptr;
ID3D11PixelShader* g_pFullScreenPS = nullptr;


// Number of currently active lights
static int g_iNumActivePointLights = 2048;
static int g_iNumActiveSpotLights = 0;

static float g_fMaxDistance = 1000.0f;

AMD::uint32 g_ScreenWidth = 1920;
AMD::uint32 g_ScreenHeight = 1080;


CFirstPersonCamera g_Camera;
static AMD::HUD             g_HUD;
static CDXUTDialogResourceManager  g_DialogResourceManager; // manager for shared resources of dialogs
static CDXUTTextHelper*			g_pTextHelper = nullptr;
CD3DSettingsDlg             g_D3dSettingsGUI;           // Device settings dialog
														// Depth stencil states

S_CAMERA_DESC g_ViewerData;
CFirstPersonCamera* g_pCurrentCamera = &g_Camera;
S_CAMERA_DESC* g_pCurrentData = &g_ViewerData;


void SetupCameraParameters()
{
	const CameraParameters& params = g_defaultCameraParameters[g_defaultCameraParameterIndex];

	//Setup the camera's view parameters
	float4 vecEyePosi(params.vecEye);
	float4 vecLookAt(params.vecAt);
	float4 vecDirToLookAt = vecLookAt.v - vecEyePosi.v;
	vecDirToLookAt.f[1] = 0.0f;
	vecDirToLookAt.v = XMVector3Normalize(vecDirToLookAt.v);

	g_Camera.SetViewParams(vecEyePosi, vecLookAt);

	g_FocalLength = params.focalLength;
	g_FocalDistance = params.focusDistance;
	g_SensorWidth = params.sensorWidth;
	g_fStop = params.fStop;

	g_HUD.m_GUI.GetSlider(IDC_SLIDER_FOCAL_LENGTH)->SetValue((int)g_FocalLength);
	g_HUD.m_GUI.GetSlider(IDC_SLIDER_FOCAL_DISTANCE)->SetValue((int)(g_FocalDistance*100.0f));//2位小数点
	g_HUD.m_GUI.GetSlider(IDC_SLIDER_SENSOR_WIDTH)->SetValue((int)(g_SensorWidth*10.0f));
	g_HUD.m_GUI.GetSlider(IDC_SLIDER_FSTOP)->SetValue((int)(g_fStop/10.0f));//slider小数点精度处理问题

}


void SetCameraProjectionParameters()
{
	float fov = 2 * atan(0.5f* g_SensorWidth / g_FocalLength);

	FLOAT fAspectRatio = (float)g_ScreenWidth / (float)g_ScreenHeight;
	g_Camera.SetProjParams(fov, fAspectRatio, 1.0f, 100.0f);
	//g_Camera.SetProjParams(XM_PI / 4, fAspectRatio, 1.0f, 1000.0f);

}


void ReleaseMeshes()
{
	g_TankMesh.Destroy();
}


LRESULT CALLBACK MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
	void* pUserContext);

bool CALLBACK ModifyDeviceSettings(DXUTDeviceSettings* pDeviceSetting, void* pUserContext);

void CALLBACK OnFrameTimeUpdated(double fTotalTime, float fElaspedTime, void* pUserContext);

bool CALLBACK IsD3D11DeviceAcceptable(const CD3D11EnumAdapterInfo* AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo* DeviceInfo,
	DXGI_FORMAT BackBufferFormat, bool bWindowed, void*pUserContext);

HRESULT CALLBACK OnD3D11DeviceCreated(ID3D11Device* pD3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc/*包含了Buffer格式/SampleDesc*/,
	void* pUserContext);

HRESULT AddShadersToCache(ID3D11Device * pD3dDevice);

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
	DXUTSetCallbackD3D11SwapChainReleasing(OnD3D11ReleasingSwapChain);//正在释放SwapChain
	DXUTSetCallbackD3D11DeviceDestroyed(OnD3D11DestroyDevice);
	DXUTSetCallbackD3D11FrameRender(OnFrameRender);

	InitApp();
	DXUTInit(true,true,NULL);//Parse the command line,show msgBoxes on error,no extra command line params
	DXUTSetCursorSettings(true, true);
	DXUTCreateWindow(L"TestTriangle v1.2");


	DXUTCreateDevice(D3D_FEATURE_LEVEL_11_0, true, g_ScreenWidth, g_ScreenHeight);//jingz 在此窗口为出现全屏化之前，其分辨率一直有个错误bug，偶然消除了

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
			pDeviceSettings->d3d11.sd.SampleDesc.Count = 1;
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


	//// See if we need to use one of the debug drawing shaders instead
	//g_bDebugDrawingEnabled = g_HUD.m_GUI.GetCheckBox(IDC_CHECKBOX_ENABLE_DEBUG_DRAWING)->GetEnabled() &&
	//	g_HUD.m_GUI.GetCheckBox(IDC_CHECKBOX_ENABLE_DEBUG_DRAWING)->GetChecked();
	//g_bDebugDrawMethodOne = g_HUD.m_GUI.GetRadioButton(IDC_RADIOBUTTON_DEBUG_DRAWING_ONE)->GetEnabled() &&
	//	g_HUD.m_GUI.GetRadioButton(IDC_RADIOBUTTON_DEBUG_DRAWING_ONE)->GetChecked();

	//// And see if we need to use the no-cull pixel shader instead
	//g_bLightCullingEnabled = g_HUD.m_GUI.GetCheckBox(IDC_CHECKBOX_ENABLE_LIGHT_CULLING)->GetEnabled() &&
	//	g_HUD.m_GUI.GetCheckBox(IDC_CHECKBOX_ENABLE_LIGHT_CULLING)->GetChecked();

	//// Determine which compute shader we should use
	//g_bDepthBoundsEnabled = g_HUD.m_GUI.GetCheckBox(IDC_CHECKBOX_ENABLE_DEPTH_BOUNDS)->GetEnabled() &&
	//	g_HUD.m_GUI.GetCheckBox(IDC_CHECKBOX_ENABLE_DEPTH_BOUNDS)->GetChecked();

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


HRESULT SetupCameraForDof(ID3D11Device* device)
{
	HRESULT hr = S_OK;

	SetupCameraParameters();

	D3D11_BUFFER_DESC tempDesc;
	tempDesc.Usage = D3D11_USAGE_DYNAMIC;
	tempDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	tempDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	tempDesc.MiscFlags = 0;
	tempDesc.ByteWidth = sizeof(S_CAMERA_DESC);
	hr = device->CreateBuffer(&tempDesc, NULL, &g_pD3DViewerConsterBuffer);

	assert(hr == S_OK);

	return hr;
}

struct CalcDepthOfFieldParams
{
	unsigned int ScreenParamsX;
	unsigned int ScreenParamsY;

	float        zNear;
	float        zFar;
	float        FocusDistance;
	float        ratioFocalLengthToLensDiameter;
	float        FocalLength;
	float        MaxRadius;
	float        forceCircleOfConfusion;

	float CircleOfConfusionScale;
	float CircleOfConfusionBias;
	float pad;

};

HRESULT SetupDepthOfField(ID3D11Device* device)
{
	HRESULT hr = S_OK;

	D3D11_BUFFER_DESC tempDesc;
	tempDesc.Usage = D3D11_USAGE_DYNAMIC;
	tempDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	tempDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	tempDesc.MiscFlags = 0;
	tempDesc.ByteWidth = sizeof(CalcDepthOfFieldParams);
	hr = device->CreateBuffer(&tempDesc, NULL, &g_pD3DCalcDofConsterBuffer);

	assert(hr == S_OK);

	return hr;
}




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
	
	XMVECTOR SceneMin, SceneMax;

	bool bIsSceneMinMaxInited = false;

	ID3D11DeviceContext* pD3dImmediateContext = DXUTGetD3D11DeviceContext();

	hr = g_TankMesh.Create(pD3dDevice, L"Tank\\TankScene.sdkmesh", nullptr);
	assert(hr == S_OK);

#ifdef FORWARDPLUS
	//load the scene mesh
	g_SceneMesh.Create(pD3dDevice, L"sponza\\sponza.sdkmesh", nullptr);
	g_SceneAlphaMesh.Create(pD3dDevice, L"sponza\\sponza_alpha.sdkmesh", nullptr);
	g_SceneMesh.CalculateMeshMinMax(&SceneMin, &SceneMax);
	bIsSceneMinMaxInited = true;
	V_RETURN(s_ForwardPlusRender.OnCreateDevice(pD3dDevice, SceneMin, SceneMax));
#endif // FORWARDPLUS

#ifdef TRIANGLE

#ifdef FORWARDPLUS
	s_TriangleRender.GenerateMeshData(100, 100, 100, 6, 0.0f, 200.0f, 300.0f);
#else
	s_TriangleRender.GenerateMeshData(100, 100, 100, 6, 0.0f, -100.0f, 500.0f);
#endif // FORWARDPLUS


	
	if (!bIsSceneMinMaxInited)
	{
		Triangle::TriangleRender::CalculateSceneMinMax(s_TriangleRender.m_MeshData, &SceneMin, &SceneMax);
		bIsSceneMinMaxInited = true;
	}
	V_RETURN(s_TriangleRender.OnD3DDeviceCreated(pD3dDevice, pBackBufferSurfaceDesc, pUserContext));
#endif

#ifdef DECAL
	s_DeferredDecalRender.GenerateMeshData(101, 101, 101, 6);
	if (!bIsSceneMinMaxInited)
	{
		PostProcess::DeferredDecalRender::CalculateSceneMinMax(s_DeferredDecalRender.m_MeshData, &SceneMin, &SceneMax);
		bIsSceneMinMaxInited = true;
	}

	V_RETURN(s_DeferredDecalRender.OnD3DDeviceCreated(pD3dDevice, pBackBufferSurfaceDesc, pUserContext));
	s_DeferredDecalRender.SetDecalTextureSRV(g_pDecalTextureSRV);

	V_RETURN(s_DeferredVoxelCutoutRender.OnD3DDeviceCreated(pD3dDevice, pBackBufferSurfaceDesc, pUserContext));

	V_RETURN(s_SphereRender.OnD3DDeviceCreated(pD3dDevice, pBackBufferSurfaceDesc, pUserContext));

	V_RETURN(s_RadialBlurRender.OnD3DDeviceCreated(pD3dDevice, pBackBufferSurfaceDesc, pUserContext));
	//s_RadialBlurRender.SetRadialBlurTextureSRV(g_pDecalTextureSRV);

	V_RETURN(s_ScreenBlendRender.OnD3DDeviceCreated(pD3dDevice, pBackBufferSurfaceDesc, pUserContext));
	//s_ScreenBlendRender.SetSrcTextureSRV(g_pDecalTextureSRV);




#endif

	CD3D11_DEFAULT defaultDesc;

	//Create state objects
	{
		D3D11_SAMPLER_DESC SamplerDesc;
		ZeroMemory(&SamplerDesc, sizeof(SamplerDesc));
		SamplerDesc.Filter = D3D11_FILTER_ANISOTROPIC;
		SamplerDesc.AddressU = SamplerDesc.AddressV = SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		SamplerDesc.MaxAnisotropy = 16;
		SamplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
		SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
		V_RETURN(pD3dDevice->CreateSamplerState(&SamplerDesc, &g_pSamplerAnisotropic));
		g_pSamplerAnisotropic->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("Anisotropic"), "Anisotropic");

	}
	
	{
		CD3D11_SAMPLER_DESC SamplerDesc(defaultDesc);
		SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		V_RETURN(pD3dDevice->CreateSamplerState(&SamplerDesc, &g_pSamplerLinearWrap));
		g_pSamplerLinearWrap->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("LinearWarp Sampler"), "LinearWarp Sampler");
	}

	{
		CD3D11_RASTERIZER_DESC rasterDesc(defaultDesc);
		rasterDesc.CullMode = D3D11_CULL_NONE;
		rasterDesc.DepthClipEnable = FALSE;
		V_RETURN(pD3dDevice->CreateRasterizerState(&rasterDesc, &g_pNoCullingSolidRS));
	}

	{
		CD3D11_DEPTH_STENCIL_DESC tempDesc(defaultDesc);
		tempDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		V_RETURN(pD3dDevice->CreateDepthStencilState(&tempDesc, &g_pDepthLessEqualDS));
	}


	//create AMD_SDK resource here
	TIMER_Init(pD3dDevice);
	g_HUD.OnCreateDevice(pD3dDevice);

	// Create render resource here

	static bool bCameraInit = false;
	if (!bCameraInit)
	{
		bCameraInit = true;
#ifdef FORWARDPLUS
		// Setup the camera's view parameters
		XMVECTOR SceneCenter = 0.5f*(SceneMax + SceneMin);
		XMVECTOR SceneExtents = 0.5f*(SceneMax - SceneMin);
		XMVECTOR BoundaryMin = SceneCenter - 2.0f*SceneExtents;
		XMVECTOR BoundaryMax = SceneCenter + 2.0f*SceneExtents;
		XMVECTOR BoundaryDiff = 4.0f*SceneExtents;

		g_fMaxDistance = XMVectorGetX(XMVector3Length(BoundaryDiff));
		XMVECTOR vEye = XMVectorSet(150.0f, 200.0f, 0.0f, 0.0f);
		XMVECTOR vLookAtPos = XMVectorSet(150.0f, 200.0f, 100.0f, 0.0f);
		g_Camera.SetRotateButtons(true, false, false);
		//g_Camera.SetButtonMasks(MOUSE_MIDDLE_BUTTON, MOUSE_WHEEL, MOUSE_LEFT_BUTTON);//left_button can rotate camera
		g_Camera.SetEnablePositionMovement(true);
		g_Camera.SetViewParams(vEye, vLookAtPos);
		g_Camera.SetScalers(0.005f, 0.1f*g_fMaxDistance);

	//	s_DeferredVoxelCutoutRender.SetVoxelPosition(vLookAtPos);

#else

		g_TankMesh.CalculateMeshMinMax(&SceneMin, &SceneMax);
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
		s_DeferredVoxelCutoutRender.SetVoxelPosition(XMVectorSet(0.0f, 200.0f, 300.0f, 0.0f));
		s_SphereRender.SetMeshCenterOffset(XMVectorSet(0.0f, 200.0f, 300.0f, 0.0f));
#else
		s_DeferredDecalRender.SetDecalPosition(SceneCenter);
#endif
#endif // DECAL



	}

	// Generate shaders ( this is an async operation - call AMD::ShaderCache::ShadersReady() to find out if they are complete)
	if (s_bFirstPass)
	{
		AddShadersToCache(pD3dDevice);



		g_ShaderCache.GenerateShaders(AMD::ShaderCache::CREATE_TYPE_COMPILE_CHANGES);//Only compile shaders that have changed(development mode)

		s_bFirstPass = false;
	}


	
	//DepthOfField

	

	g_ModelDesc.m_World = XMMatrixScaling(1.0f, 1.0f, 1.0f);
	g_ModelDesc.m_World_Inv = XMMatrixInverse(&XMMatrixDeterminant(g_ModelDesc.m_World), g_ModelDesc.m_World);
	g_ModelDesc.m_Position = float4(0.0f, 0.0f, 0.0f, 1.0f);
	g_ModelDesc.m_Orientation = float4(0.0f, 1.0f, 0.0f, 0.0f);
	g_ModelDesc.m_Scale = float4(0.001f, 0.001f, 0.001f, 1);
	g_ModelDesc.m_Ambient = float4(0.1f, 0.1f, 0.1f, 1.0f);
	g_ModelDesc.m_Diffuse = float4(1.0f, 1.0f, 1.0f, 1.0f);
	g_ModelDesc.m_Specular = float4(0.5f, 0.5f, 0.0f, 1.0f);;


	D3D11_BUFFER_DESC DofConstBufferDesc;
	DofConstBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	DofConstBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	DofConstBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	DofConstBufferDesc.MiscFlags = 0;
	DofConstBufferDesc.ByteWidth = sizeof(S_MODEL_DESC);
	hr = pD3dDevice->CreateBuffer(&DofConstBufferDesc, NULL, &g_pD3DModelConstBuffer);
	assert(hr == S_OK);

	SetupCameraForDof(pD3dDevice);

	SetupDepthOfField(pD3dDevice);
	g_AMD_DofFX_Desc.m_pDevice = pD3dDevice;
	g_AMD_DofFX_Desc.m_pDeviceContext = pD3dImmediateContext;
	g_AMD_DofFX_Desc.m_screenSize.x = g_ScreenWidth;
	g_AMD_DofFX_Desc.m_screenSize.y = g_ScreenHeight;
	AMD::DEPTHOFFIELDFX_RETURN_CODE amdResult = AMD::DepthOfFieldFX_Initialize(g_AMD_DofFX_Desc);
	if (amdResult != AMD::DEPTHOFFIELDFX_RETURN_CODE_SUCCESS)
	{
		return E_FAIL;
	}

	//GUI
	V_RETURN(g_DialogResourceManager.OnD3D11CreateDevice(pD3dDevice, pD3dImmediateContext));
	V_RETURN(g_D3dSettingsGUI.OnD3D11CreateDevice(pD3dDevice));
	SAFE_DELETE(g_pTextHelper);
	g_pTextHelper = new CDXUTTextHelper(pD3dDevice, pD3dImmediateContext, &g_DialogResourceManager, TEXT_LINE_HEIGHT);

	return hr;
}

HRESULT CompileShadersForDoF(ID3D11Device* device)
{
	HRESULT hr = S_OK;

	const D3D11_INPUT_ELEMENT_DESC InputLayout[] =
	{
		{ "POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0 },
		{ "NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0 },
		{ "TEXCOORD",0,DXGI_FORMAT_R32G32B32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0 },
	};

	const wchar_t pSourceFileName[] = L"DepthOfFieldFX_Sample.hlsl";

	//jingz todo

	UINT size = ARRAYSIZE(InputLayout);

	g_ShaderCache.AddShader((ID3D11DeviceChild**)&g_pD3DModelVS, AMD::ShaderCache::SHADER_TYPE::SHADER_TYPE_VERTEX,
		L"vs_5_0", L"VS_RenderModel", L"DepthOfFieldFX_Sample.hlsl", 0, nullptr, &g_pD3DModelInputLayout, (D3D11_INPUT_ELEMENT_DESC*)InputLayout, size);

	g_ShaderCache.AddShader((ID3D11DeviceChild**)&g_pD3DModelPS, AMD::ShaderCache::SHADER_TYPE::SHADER_TYPE_PIXEL,
		L"ps_5_0", L"PS_RenderModel", L"DepthOfFieldFX_Sample.hlsl", 0, nullptr, nullptr, nullptr, 0);

	g_ShaderCache.AddShader((ID3D11DeviceChild**)&g_pCalcCirlceOfConfusionCS, AMD::ShaderCache::SHADER_TYPE_COMPUTE, L"cs_5_0", L"CalcDOF", L"DepthOfField_CsCalcDOF.hlsl", 0, nullptr, nullptr, nullptr, 0);
	g_ShaderCache.AddShader((ID3D11DeviceChild**)&g_pDebugCirlceOfConfusionCS, AMD::ShaderCache::SHADER_TYPE_COMPUTE, L"cs_5_0", L"DebugVisDOF", L"DepthOfField_CsCalcDOF.hlsl", 0, nullptr, nullptr, nullptr, 0);

	{
		AMD::ShaderCache::Macro ShaderMacros[1];
		wcscpy_s(ShaderMacros[0].m_wsName, AMD::ShaderCache::m_uMACRO_MAX_LENGTH, L"DOUBLE_INTEGRATE");
		g_ShaderCache.AddShader((ID3D11DeviceChild**)&g_pDoubleVerticalIntegrateCS, AMD::ShaderCache::SHADER_TYPE_COMPUTE, L"cs_5_0", L"VerticalIntegrate", L"DepthOfFieldFX_FastFilterDOF.hlsl", 1, ShaderMacros, nullptr, nullptr, 0);

	}

	{
		//AMD::ShaderCache::Macro ShaderMacros[1];
		//wcscpy_s(ShaderMacros[0].m_wsName, AMD::ShaderCache::m_uMACRO_MAX_LENGTH, L"DOUBLE_INTEGRATE");
		g_ShaderCache.AddShader((ID3D11DeviceChild**)&g_pVerticalIntegrateCS, AMD::ShaderCache::SHADER_TYPE_COMPUTE, L"cs_5_0", L"VerticalIntegrate", L"DepthOfFieldFX_FastFilterDOF.hlsl", 0, nullptr, nullptr, nullptr, 0);
	}

	{
		
		g_ShaderCache.AddShader((ID3D11DeviceChild**)&g_pFastFilterSetupCS, AMD::ShaderCache::SHADER_TYPE_COMPUTE, L"cs_5_0", L"FastFilterSetup", L"DepthOfFieldFX_FastFilterDOF.hlsl", 0, nullptr, nullptr, nullptr, 0);

	}

	{
		AMD::ShaderCache::Macro ShaderMacros[1];
		wcscpy_s(ShaderMacros[0].m_wsName, AMD::ShaderCache::m_uMACRO_MAX_LENGTH, L"CONVERT_TO_SRGB");
		g_ShaderCache.AddShader((ID3D11DeviceChild**)&g_pReadFinalResultCS, AMD::ShaderCache::SHADER_TYPE_COMPUTE, L"cs_5_0", L"ReadFinalResult", L"DepthOfFieldFX_FastFilterDOF.hlsl", 1, ShaderMacros, nullptr, nullptr, 0);

	}

	{

		g_ShaderCache.AddShader((ID3D11DeviceChild**)&g_pFastFilterSetupQuarterResCS, AMD::ShaderCache::SHADER_TYPE_COMPUTE, L"cs_5_0", L"QuaterResFastFilterSetup", L"DepthOfFieldFX_FastFilterDOF.hlsl", 0, nullptr, nullptr, nullptr, 0);

	}

	{
		g_ShaderCache.AddShader((ID3D11DeviceChild**)&g_pBoxFastFilterSetupCS, AMD::ShaderCache::SHADER_TYPE_COMPUTE, L"cs_5_0", L"BoxFastFilterSetup", L"DepthOfFieldFX_FastFilterDOF.hlsl", 0, nullptr, nullptr, nullptr, 0);
	}

	device->CreateVertexShader(VS_FULLSCREEN_Data, sizeof(VS_FULLSCREEN_Data), NULL, &g_pFullScreenVS);
	device->CreatePixelShader(PS_FULLSCREEN_Data, sizeof(PS_FULLSCREEN_Data), NULL, &g_pFullScreenPS);


	return hr;

}

HRESULT AddShadersToCache(ID3D11Device * pD3dDevice)
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
	s_DeferredVoxelCutoutRender.AddShadersToCache(&g_ShaderCache);
	s_SphereRender.AddShadersToCache(&g_ShaderCache);
	s_RadialBlurRender.AddShadersToCache(&g_ShaderCache);
	s_ScreenBlendRender.AddShadersToCache(&g_ShaderCache);

#endif

	CompileShadersForDoF(pD3dDevice);

	return hr;
}

HRESULT OnD3D11ResizedSwapChain(ID3D11Device * pD3dDevice, IDXGISwapChain * pSwapChain, const DXGI_SURFACE_DESC * pBackBufferSurfaceDesc, void * pUserContext)
{
	HRESULT hr;

	g_ScreenWidth = pBackBufferSurfaceDesc->Width;
	g_ScreenHeight = pBackBufferSurfaceDesc->Height;

	V_RETURN(g_DialogResourceManager.OnD3D11ResizedSwapChain(pD3dDevice, pBackBufferSurfaceDesc));
	V_RETURN(g_D3dSettingsGUI.OnD3D11ResizedSwapChain(pD3dDevice, pBackBufferSurfaceDesc));

	//Set the location and size of AMD standard HUD
	g_HUD.m_GUI.SetLocation(pBackBufferSurfaceDesc->Width - AMD::HUD::iDialogWidth, 0);
	g_HUD.m_GUI.SetSize(AMD::HUD::iDialogWidth, pBackBufferSurfaceDesc->Height);
	g_HUD.OnResizedSwapChain(pBackBufferSurfaceDesc);

	//jingz 重要代码
	g_AMD_DofFX_Desc.m_screenSize.x = g_ScreenWidth;
	g_AMD_DofFX_Desc.m_screenSize.y = g_ScreenHeight;

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
	
	// App specific resources
	// scene render target
	g_appColorBuffer.Release();
	hr = g_appColorBuffer.CreateSurface(pD3dDevice, pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height, pBackBufferSurfaceDesc->SampleDesc.Count, 1, 1,
		DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN,
		D3D11_USAGE_DEFAULT, false, 0, NULL);

	//scene depth buffer
	g_appDepthBuffer.Release();
	g_appDepthBuffer.CreateSurface(pD3dDevice, pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height, pBackBufferSurfaceDesc->SampleDesc.Count, 1, 1,
		DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_UNKNOWN,
		DXGI_FORMAT_D32_FLOAT, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN,
		D3D11_USAGE_DEFAULT, false, 0, NULL);

	// circle of confusion target
	g_appCircleOfConfusionTexture.Release();
	g_appCircleOfConfusionTexture.CreateSurface(pD3dDevice, pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height, pBackBufferSurfaceDesc->SampleDesc.Count, 1, 1,
		DXGI_FORMAT_R16_FLOAT, DXGI_FORMAT_R16_FLOAT, DXGI_FORMAT_UNKNOWN,
		DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R16_FLOAT, DXGI_FORMAT_UNKNOWN,
		D3D11_USAGE_DEFAULT, false, 0, NULL);


	g_appDofSurface.Release();
	DXGI_FORMAT appDebugFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	hr = g_appDofSurface.CreateSurface(pD3dDevice, pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height, pBackBufferSurfaceDesc->SampleDesc.Count, 1, 1,
		DXGI_FORMAT_R8G8B8A8_TYPELESS, appDebugFormat, appDebugFormat,
		DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_UNKNOWN, D3D11_USAGE_DEFAULT, false, 0, NULL);

	g_AMD_DofFX_Desc.m_pCircleOfConfusionSRV = g_appCircleOfConfusionTexture._srv;
	g_AMD_DofFX_Desc.m_pColorSRV = g_appColorBuffer._srv;
	g_AMD_DofFX_Desc.m_pResultUAV = g_appDofSurface._uav;
	g_AMD_DofFX_Desc.m_maxBlurRadius = g_MaxRadius;
	AMD::DepthOfFieldFX_Resize(g_AMD_DofFX_Desc);



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
	s_DeferredVoxelCutoutRender.OnResizedSwapChain(pD3dDevice, pBackBufferSurfaceDesc);
	s_SphereRender.OnResizedSwapChain(pD3dDevice, pBackBufferSurfaceDesc);
	s_RadialBlurRender.OnResizedSwapChain(pD3dDevice, pBackBufferSurfaceDesc);
	s_ScreenBlendRender.OnResizedSwapChain(pD3dDevice, pBackBufferSurfaceDesc);

#endif

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
	s_DeferredVoxelCutoutRender.OnReleasingSwapChain();
	s_SphereRender.OnReleasingSwapChain();
	s_RadialBlurRender.OnReleasingSwapChain();
	s_ScreenBlendRender.OnReleasingSwapChain();

#endif


	g_DialogResourceManager.OnD3D11ReleasingSwapChain();
}

void OnD3D11DestroyDevice(void * pUserContext)
{
	DepthOfFieldFX_Release(g_AMD_DofFX_Desc);

	ReleaseMeshes();

	g_DialogResourceManager.OnD3D11DestroyDevice();
	g_D3dSettingsGUI.OnD3D11DestroyDevice();
	SAFE_DELETE(g_pTextHelper);
	DXUTGetGlobalResourceCache().OnDestroyDevice();

	g_SceneMesh.Destroy();
	g_SceneAlphaMesh.Destroy();


	SAFE_RELEASE(g_pSamplerAnisotropic);
	SAFE_RELEASE(g_pSamplerLinearWrap);

	SAFE_RELEASE(g_pNoCullingSolidRS);

	SAFE_RELEASE(g_pDepthLessEqualDS);
	SAFE_RELEASE(g_pDepthStencilDefaultDS);

	SAFE_RELEASE(g_pDepthStencilView);
	SAFE_RELEASE(g_pDepthStencilSRV);
	SAFE_RELEASE(g_pDepthStencilTexture);

	SAFE_RELEASE(g_pTempDepthStencilView);
	SAFE_RELEASE(g_pTempDepthStencilSRV);
	SAFE_RELEASE(g_pTempDepthStencilTexture);

	SAFE_RELEASE(g_pDecalTexture);
	SAFE_RELEASE(g_pDecalTextureSRV);

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
	s_DeferredVoxelCutoutRender.OnD3D11DestroyDevice(pUserContext);
	s_SphereRender.OnD3D11DestroyDevice(pUserContext);
	s_RadialBlurRender.OnD3D11DestroyDevice(pUserContext);
	s_ScreenBlendRender.OnD3D11DestroyDevice(pUserContext);


#endif
	
	//AMD
	SAFE_RELEASE(g_pD3DModelInputLayout);
	SAFE_RELEASE(g_pD3DModelVS);
	SAFE_RELEASE(g_pD3DModelPS);
	SAFE_RELEASE(g_pD3DModelConstBuffer);

	SAFE_RELEASE(g_pCalcCirlceOfConfusionCS);
	SAFE_RELEASE(g_pDebugCirlceOfConfusionCS);

	SAFE_RELEASE(g_pDoubleVerticalIntegrateCS);
	SAFE_RELEASE(g_pVerticalIntegrateCS);
	SAFE_RELEASE(g_pFastFilterSetupCS);
	SAFE_RELEASE(g_pReadFinalResultCS);
	SAFE_RELEASE(g_pFastFilterSetupQuarterResCS);
	SAFE_RELEASE(g_pBoxFastFilterSetupCS);
	//g_pDoubleVerticalIntegrateCS = nullptr;
	//g_pVerticalIntegrateCS = nullptr;
	//g_pFastFilterSetupCS = nullptr;
	//g_pReadFinalResultCS = nullptr;
	//g_pFastFilterSetupQuarterResCS = nullptr;
	//g_pBoxFastFilterSetupCS = nullptr;

	SAFE_RELEASE(g_pD3DViewerConsterBuffer);
	SAFE_RELEASE(g_pD3DCalcDofConsterBuffer);


	SAFE_RELEASE(g_pFullScreenVS);
	SAFE_RELEASE(g_pFullScreenPS);

	g_appColorBuffer.Release();
	g_appDepthBuffer.Release();
	g_appDofSurface.Release();
	g_appCircleOfConfusionTexture.Release();



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

void SetCameraConstantBuffer(ID3D11DeviceContext* pD3DContext, ID3D11Buffer* pD3DCameraConstBufer, S_CAMERA_DESC* pCameraDesc, CFirstPersonCamera* pCamera, unsigned int nCount)
{
	if (pD3DContext == nullptr)
	{
		OutputDebugString(AMD_FUNCTION_WIDE_NAME L" received a NULL D3D11 Context pointer \n");
		return;
	}

	if (pD3DCameraConstBufer == NULL)
	{
		OutputDebugString(AMD_FUNCTION_WIDE_NAME L"received a NULL D3D11 Constant Buffer pointer \n");
		return;
	}

	D3D11_MAPPED_SUBRESOURCE MappedResource;

	for (unsigned int i = 0;i< nCount;++i)
	{
		CFirstPersonCamera& camera = pCamera[i];
		S_CAMERA_DESC& cameraDesc = pCameraDesc[i];

		XMMATRIX view = camera.GetViewMatrix();
		XMMATRIX proj = camera.GetProjMatrix();
		XMMATRIX viewProj = view* proj;
		XMMATRIX viewInv = XMMatrixInverse(&XMMatrixDeterminant(view), view);
		XMMATRIX projInv = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
		XMMATRIX viewProjInv = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

		cameraDesc.m_View = XMMatrixTranspose(view);
		cameraDesc.m_Projection = XMMatrixTranspose(proj);
		cameraDesc.m_View_Inv = XMMatrixTranspose(viewInv);
		cameraDesc.m_Projection_Inv = XMMatrixTranspose(projInv);
		cameraDesc.m_ViewProjection = XMMatrixTranspose(viewProj);
		cameraDesc.m_ViewProjection_Inv = XMMatrixTranspose(viewProjInv);
		cameraDesc.m_Fov = camera.GetFOV();
		cameraDesc.m_Aspect = camera.GetAspect();
		cameraDesc.m_NearPlane = camera.GetNearClip();
		cameraDesc.m_FarPlane = camera.GetFarClip();


		memcpy(&cameraDesc.m_Position, &(camera.GetEyePt()), sizeof(cameraDesc.m_Position));
		memcpy(&cameraDesc.m_Direction, &(XMVector3Normalize(camera.GetLookAtPt() - camera.GetEyePt())), sizeof(cameraDesc.m_Direction));
		memcpy(&cameraDesc.m_Up, &(camera.GetWorldUp()), sizeof(cameraDesc.m_Position));

	}

	HRESULT hr = pD3DContext->Map(pD3DCameraConstBufer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
	if (hr == S_OK && MappedResource.pData != NULL)
	{
		memcpy(MappedResource.pData, pCameraDesc, sizeof(S_CAMERA_DESC)*nCount);
		pD3DContext->Unmap(pD3DCameraConstBufer, 0);
	}

	auto tempFocalLength = g_FocalLength / 1000.0f;//换算回米做计算
	float CircleOfConfusionScale =  (1.0f / g_fStop) * tempFocalLength * (g_ViewerData.m_FarPlane - g_ViewerData.m_NearPlane) / ((g_FocalDistance - tempFocalLength)*g_ViewerData.m_NearPlane*g_ViewerData.m_FarPlane);
	float CircleOfConfusionBias = ((1.0f / g_fStop) * tempFocalLength * (g_ViewerData.m_NearPlane - g_FocalDistance)) / ((g_FocalDistance * tempFocalLength) * g_ViewerData.m_NearPlane);
	float temp = (0.95f * CircleOfConfusionScale + CircleOfConfusionBias)*900.0f;

	hr = pD3DContext->Map(g_pD3DCalcDofConsterBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
	if (hr == S_OK && MappedResource.pData != NULL)
	{
		CalcDepthOfFieldParams* pParams = reinterpret_cast<CalcDepthOfFieldParams*>(MappedResource.pData);


		pParams->CircleOfConfusionScale = CircleOfConfusionScale;
		pParams->CircleOfConfusionBias = CircleOfConfusionBias;

		pParams->FocalLength = g_FocalLength / 1000.0f;
		pParams->FocusDistance = g_FocalDistance;
		pParams->ratioFocalLengthToLensDiameter = g_fStop;
		pParams->ScreenParamsX = g_ScreenWidth;
		pParams->ScreenParamsY = g_ScreenHeight;
		pParams->zNear = g_ViewerData.m_NearPlane;
		pParams->zFar = g_ViewerData.m_FarPlane;
		pParams->MaxRadius = static_cast<float>(g_MaxRadius);
		pParams->forceCircleOfConfusion = g_ForceCircleOfConfusion;
		pD3DContext->Unmap(g_pD3DCalcDofConsterBuffer, 0);
	}


}


void SetModelConstantBuffer(ID3D11DeviceContext* pD3DContext, ID3D11Buffer* pD3DModelConstBuffer, S_MODEL_DESC* pModelDesc, CFirstPersonCamera* pCamera)
{
	D3D11_MAPPED_SUBRESOURCE MappedResource;

	XMMATRIX world = pModelDesc->m_World;
	XMMATRIX view = pCamera->GetViewMatrix();
	XMMATRIX proj = pCamera->GetProjMatrix();
	XMMATRIX viewProj = view*proj;
	XMMATRIX worldView = world*view;
	XMMATRIX worldViewProj = world*view*proj;
	XMMATRIX worldViewInv = XMMatrixInverse(&XMMatrixDeterminant(worldView), worldView);
	XMMATRIX worldViewProjInv = XMMatrixInverse(&XMMatrixDeterminant(worldViewProj), worldViewProj);

	pModelDesc->m_WorldView = XMMatrixTranspose(worldView);
	pModelDesc->m_WorldView_Inv = XMMatrixTranspose(worldViewInv);
	pModelDesc->m_WorldViewProjection = XMMatrixTranspose(worldViewProj);
	pModelDesc->m_WorldViewProjection_Inv = XMMatrixTranspose(worldViewProjInv);

	HRESULT hr = pD3DContext->Map(pD3DModelConstBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
	if (hr == S_OK && MappedResource.pData)
	{
		memcpy(MappedResource.pData, pModelDesc, sizeof(*pModelDesc));
		pD3DContext->Unmap(pD3DModelConstBuffer, 0);
	}

}

void RenderScene(ID3D11DeviceContext* pContext, CDXUTSDKMesh** pMesh, S_MODEL_DESC* pMeshDesc, unsigned int nMeshCount,
	D3D11_VIEWPORT* pViewPort, unsigned int nViewPortCount, D3D11_RECT* pScissorRect, unsigned int nScissorRectCount,
	ID3D11RasterizerState* pRasterState, ID3D11BlendState* pBlendState, float* pFactorBS, ID3D11DepthStencilState*   pDSS,unsigned int dssRef,
	ID3D11InputLayout* pInputLayout,ID3D11VertexShader* pVertexShader,ID3D11HullShader* pHullShader,ID3D11DomainShader* pDomainShader,ID3D11GeometryShader* pGeometryShader,ID3D11PixelShader* pPixelShader,
	ID3D11Buffer* pModelConstBuffer,
	ID3D11Buffer** ppConstBuffers,unsigned int nConstBufferStart,unsigned int nConstBufferCount,
	ID3D11SamplerState** ppSS, unsigned int nSSStart,unsigned int nSSCount,
	ID3D11ShaderResourceView** ppSRV,unsigned int nSRVStart,unsigned int nSRVCount,
	ID3D11RenderTargetView** ppRTV,unsigned int nRTVCount,ID3D11DepthStencilView* pDSV,CFirstPersonCamera* pCamera)
{
	ID3D11RenderTargetView* const pNullRTV[8] = { 0 };
	ID3D11ShaderResourceView* const pNullSRV[128] = { 0 };

	//Unbind anything that could be still bound on input or output
	// If this doesn't happen,DX Runtime will spam with warnings
	pContext->OMSetRenderTargets(AMD_ARRAY_SIZE(pNullRTV), pNullRTV, NULL);
	pContext->CSSetShaderResources(0, AMD_ARRAY_SIZE(pNullSRV), pNullSRV);
	pContext->VSSetShaderResources(0, AMD_ARRAY_SIZE(pNullSRV), pNullSRV);
	pContext->HSSetShaderResources(0, AMD_ARRAY_SIZE(pNullSRV), pNullSRV);
	pContext->DSSetShaderResources(0, AMD_ARRAY_SIZE(pNullSRV), pNullSRV);
	pContext->GSSetShaderResources(0, AMD_ARRAY_SIZE(pNullSRV), pNullSRV);
	pContext->PSSetShaderResources(0, AMD_ARRAY_SIZE(pNullSRV), pNullSRV);

	pContext->IASetInputLayout(pInputLayout);

	pContext->VSSetShader(pVertexShader, NULL, 0);
	pContext->HSSetShader(pHullShader, NULL, 0);
	pContext->DSSetShader(pDomainShader, NULL, 0);
	pContext->GSSetShader(pGeometryShader, NULL, 0);
	pContext->PSSetShader(pPixelShader, NULL, 0);

	if (nSSCount)
	{
		pContext->VSSetSamplers(nSSStart, nSSCount, ppSS);
		pContext->HSSetSamplers(nSSStart, nSSCount, ppSS);
		pContext->DSSetSamplers(nSSStart, nSSCount, ppSS);
		pContext->GSSetSamplers(nSSStart, nSSCount, ppSS);
		pContext->PSSetSamplers(nSSStart, nSSCount, ppSS);
	}

	if (nSRVCount)
	{
		pContext->VSSetShaderResources(nSRVStart, nSRVCount, ppSRV);
		pContext->HSSetShaderResources(nSRVStart, nSRVCount, ppSRV);
		pContext->DSSetShaderResources(nSRVStart, nSRVCount, ppSRV);
		pContext->GSSetShaderResources(nSRVStart, nSRVCount, ppSRV);
		pContext->PSSetShaderResources(nSRVStart, nSRVCount, ppSRV);

	}

	if (nConstBufferCount)
	{
		pContext->VSSetConstantBuffers(nConstBufferStart, nConstBufferCount, ppConstBuffers);
		pContext->HSSetConstantBuffers(nConstBufferStart, nConstBufferCount, ppConstBuffers);
		pContext->DSSetConstantBuffers(nConstBufferStart, nConstBufferCount, ppConstBuffers);
		pContext->GSSetConstantBuffers(nConstBufferStart, nConstBufferCount, ppConstBuffers);
		pContext->PSSetConstantBuffers(nConstBufferStart, nConstBufferCount, ppConstBuffers);
	}


	pContext->OMSetRenderTargets(nRTVCount, ppRTV, pDSV);
	pContext->OMSetBlendState(pBlendState, pFactorBS, 0xf);
	pContext->OMSetDepthStencilState(pDSS, dssRef);
	pContext->RSSetState(pRasterState);
	pContext->RSSetScissorRects(nScissorRectCount, pScissorRect);
	pContext->RSSetViewports(nViewPortCount, pViewPort);

	for (unsigned int i = 0;i<nMeshCount;++i)
	{
		SetModelConstantBuffer(pContext, pModelConstBuffer, &pMeshDesc[i], pCamera);

		pMesh[i]->Render(pContext,0);
	}

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


	ClearD3D11DeviceContext();

	const DXGI_SURFACE_DESC* pBackBufferDesc = DXUTGetDXGIBackBufferSurfaceDesc();


	//Clear the backBuffer and depth stencil
	float ClearColor[4] = { 0.0013f, 0.0015f, 0.0050f, 0.0f };
	ID3D11RenderTargetView* pRTV = DXUTGetD3D11RenderTargetView();
	pD3dImmediateContext->OMSetRenderTargets(1, &pRTV, g_pDepthStencilView);
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

		const DXGI_SURFACE_DESC* pBackBufferSurfaceDes = DXUTGetDXGIBackBufferSurfaceDesc();

#ifdef FORWARDPLUS
		s_ForwardPlusRender.OnRender(pD3dDevice, pD3dImmediateContext, &g_Camera, pRTV, pBackBufferSurfaceDes,
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
		s_DeferredDecalRender.OnRender(pD3dDevice, pD3dImmediateContext, pBackBufferDesc, &g_Camera, pRTV, g_pTempDepthStencilView, g_pDepthStencilSRV);
		s_DeferredVoxelCutoutRender.OnRender(pD3dDevice, pD3dImmediateContext, pBackBufferDesc, &g_Camera, g_pTempTextureRenderTargetView[0], g_pTempDepthStencilView, g_pDepthStencilSRV);

		s_SphereRender.OnRender(pD3dDevice, pD3dImmediateContext, pBackBufferDesc, &g_Camera, g_pTempTextureRenderTargetView[0], g_pTempDepthStencilView, g_pDepthStencilSRV);

		s_RadialBlurRender.SetRadialBlurTextureSRV(g_pTempTextureSRV[0]);
		s_RadialBlurRender.OnRender(pD3dDevice, pD3dImmediateContext, pBackBufferDesc, &g_Camera, g_pTempTextureRenderTargetView[1], g_pTempDepthStencilView, g_pDepthStencilSRV);
		s_ScreenBlendRender.SetBlendTextureSRV(g_pTempTextureSRV[1]);
		s_ScreenBlendRender.OnRender(pD3dDevice, pD3dImmediateContext, pBackBufferDesc, &g_Camera, pRTV, g_pTempDepthStencilView, g_pDepthStencilSRV);

#endif


		D3D11_RECT* pNullSR = nullptr;
		ID3D11HullShader* pNullHS = nullptr;
		ID3D11DomainShader* pNullDS = nullptr;
		ID3D11GeometryShader* pNullGS = nullptr;
		ID3D11ShaderResourceView* pNullSRV = nullptr;
		ID3D11UnorderedAccessView* pNullUAV = nullptr;

		ID3D11RenderTargetView* pOriginalRTV = NULL;
		ID3D11DepthStencilView* pOriginalDSV = NULL;

		const int nFrameCountMax = 60;
		static int nFrameCount = 0;
		static float fTimeSceneRendering = 0.0f;
		static float fTimeDofRendering = 0.0f;

		float4 blue(0.176f, 0.196f, 0.667f, 0.000f);
		float4 white(1.0f, 1.0f, 1.0f, 1.0f);

		SetCameraProjectionParameters();

		// Store the original render target and depth buffer
		pD3dImmediateContext->OMGetRenderTargets(1, &pOriginalRTV, &pOriginalDSV);


		//绘制场景至GBuffer

		// Clear the depth stencil and shadow map
		pD3dImmediateContext->ClearRenderTargetView(g_appColorBuffer._rtv, blue.f);
		pD3dImmediateContext->ClearDepthStencilView(g_appDepthBuffer._dsv, D3D11_CLEAR_DEPTH, 1.0f, 0);

		SetCameraConstantBuffer(pD3dImmediateContext, g_pD3DViewerConsterBuffer, &g_ViewerData, &g_Camera, 1);


		{
			ID3D11Buffer* pCB[] = { g_pD3DModelConstBuffer,g_pD3DViewerConsterBuffer };
			ID3D11SamplerState* pSS[] = { g_pSamplerLinearWrap };
			ID3D11RenderTargetView* pRTVs[] = { g_appColorBuffer._rtv };

			CDXUTSDKMesh* pMeshes[] = { &g_TankMesh };

			auto tempViewPort = CD3D11_VIEWPORT(0.0, 0.0f, (float)g_ScreenWidth, (float)g_ScreenHeight);
			
			RenderScene(pD3dImmediateContext, pMeshes, &g_ModelDesc, 1, &tempViewPort, 1, nullptr, 0,
				g_pBackCullingSolidRS, g_pOpaqueBS, white.f, g_pDepthLessEqualDS, 0, g_pD3DModelInputLayout,
				g_pD3DModelVS, pNullHS, pNullDS, pNullGS, g_pD3DModelPS, g_pD3DModelConstBuffer, pCB, 0, AMD_ARRAY_SIZE(pCB),
				pSS, 0, AMD_ARRAY_SIZE(pSS), &pNullSRV, 1, 0, pRTVs, AMD_ARRAY_SIZE(pRTVs), g_appDepthBuffer._dsv, g_pCurrentCamera);

		}

		TIMER_End();

		TIMER_Begin(0, L"Depth Of Field");

		pD3dImmediateContext->OMSetRenderTargets(1, &pOriginalRTV, pOriginalDSV);

		//计算景深
		pD3dImmediateContext->CSSetShader(g_pCalcCirlceOfConfusionCS, NULL, 0);
		pD3dImmediateContext->CSSetConstantBuffers(0, 1, &g_pD3DCalcDofConsterBuffer);
		pD3dImmediateContext->CSSetUnorderedAccessViews(0, 1, &g_appCircleOfConfusionTexture._uav, NULL);
		pD3dImmediateContext->CSSetShaderResources(0, 1, &g_appDepthBuffer._srv);
		int threadCountX = (g_ScreenWidth + 7) / 8;
		int threadCountY = (g_ScreenHeight + 7) / 8;
		pD3dImmediateContext->Dispatch(threadCountX, threadCountY, 1);


		pD3dImmediateContext->CSSetUnorderedAccessViews(0, 1, &pNullUAV, NULL);
		pD3dImmediateContext->CSSetShaderResources(0, 1, &pNullSRV);
		pD3dImmediateContext->CSSetShader(nullptr, NULL, 0);






		g_AMD_DofFX_Desc.m_scaleFactor = g_ScaleFactor;

		switch (g_depthOfFieldMode)
		{
		case DOF_BoxFastFilterSpread:
			g_AMD_DofFX_Desc.m_scaleFactor = g_BoxScaleFactor;
			g_AMD_DofFX_Desc.m_pOpaque->m_pBoxFastFilterSetupCS = g_pBoxFastFilterSetupCS;
			
			AMD::DepthOfFieldFX_RenderBox(g_AMD_DofFX_Desc);//jingz 暂时不关注这些问题
			break;
		case DOF_FastFilterSpread:
			g_AMD_DofFX_Desc.m_pOpaque->m_pFastFilterSetupCS = g_pFastFilterSetupCS;
			g_AMD_DofFX_Desc.m_pOpaque->m_pVerticalIntegrateCS = g_pVerticalIntegrateCS;
			g_AMD_DofFX_Desc.m_pOpaque->m_pDoubleVerticalIntegrateCS = g_pDoubleVerticalIntegrateCS;
			g_AMD_DofFX_Desc.m_pOpaque->m_pReadFinalResultCS = g_pReadFinalResultCS;
			
			AMD::DepthOfFieldFX_Render(g_AMD_DofFX_Desc);
			break;
		case DOF_QuarterResFastFilterSpread:
			g_AMD_DofFX_Desc.m_pOpaque->m_pFastFilterSetupQuarterResCS = g_pFastFilterSetupQuarterResCS;
			AMD::DepthOfFieldFX_RenderQuarterRes(g_AMD_DofFX_Desc);
			break;
		case DOF_Disabled:
		default:
			pD3dImmediateContext->CopyResource(g_appDofSurface._t2d, g_appColorBuffer._t2d);
			break;
		}
		

		TIMER_End(); // Render


		//Blend DOF To Scene

		pD3dImmediateContext->OMSetRenderTargets(1, &pOriginalRTV, pOriginalDSV);
		pD3dImmediateContext->VSSetShader(g_pFullScreenVS, NULL, 0);
		pD3dImmediateContext->PSSetShader(g_pFullScreenPS, NULL, 0);
		pD3dImmediateContext->PSSetShaderResources(0, 1, g_bShowDepthOfFieldResult ? &g_appDofSurface._srv : &g_appColorBuffer._srv);
		//jingz todo
		pD3dImmediateContext->PSSetSamplers(0, 1, &g_pSamplerLinearWrap);
		pD3dImmediateContext->OMSetBlendState(g_pOpaqueBS, white.f, 0xf);
		pD3dImmediateContext->OMSetDepthStencilState(g_pDepthLessEqualDS, 0);
		pD3dImmediateContext->RSSetState(g_pNoCullingSolidRS);
		pD3dImmediateContext->Draw(6, 0);


		SAFE_RELEASE(pOriginalRTV);
		SAFE_RELEASE(pOriginalDSV);

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

#ifdef FORWARDPLUS_GUI

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

#endif
	
#ifdef DOF_GUI

	g_HUD.m_GUI.AddStatic(IDC_STATIC_FOCAL_LENGTH, L"Focal Length (mm)", AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight);
	g_HUD.m_GUI.AddSlider(IDC_SLIDER_FOCAL_LENGTH, AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, 18, 500, (int)(g_FocalLength));
	g_HUD.m_GUI.AddStatic(IDC_STATIC_SENSOR_WIDTH, L"Sensor Width (mm)", AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight);
	g_HUD.m_GUI.AddSlider(IDC_SLIDER_SENSOR_WIDTH, AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, 45, 1000, (int)(g_ScreenWidth * 10.0f));
	g_HUD.m_GUI.AddStatic(IDC_STATIC_FOCAL_DISTANCE, L"Focal Distance (meters)", AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight);
	g_HUD.m_GUI.AddSlider(IDC_SLIDER_FOCAL_DISTANCE, AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, 20, 10000,
		(int)(g_FocalDistance * 100.0f));//2位小数点
	g_HUD.m_GUI.AddStatic(IDC_STATIC_FSTOP, L"F value(ratioFocalLengthToLensDiameter)", AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight);
	g_HUD.m_GUI.AddSlider(IDC_SLIDER_FSTOP, AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, 14, 220, (int)(g_fStop*10));//1位小数点精度放大，之后再除以10还原

	g_HUD.m_GUI.AddStatic(IDC_STATIC_MAX_RADIUS, L"MaxRadius", AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight);
	g_HUD.m_GUI.AddSlider(IDC_SLIDER_MAX_RADIUS, AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, 5, MAX_DOF_RADIUS, g_MaxRadius);
	g_HUD.m_GUI.AddStatic(IDC_STATIC_FORCE_COC, L"ForceCoC", AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight);
	g_HUD.m_GUI.AddSlider(IDC_SLIDER_FORCE_COC, AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight, 0, MAX_DOF_RADIUS, (int)(g_ForceCircleOfConfusion));

	g_HUD.m_GUI.AddButton(IDC_BUTTON_SAVE_SCREEN_SHOT, L"ScreenShot", AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, AMD::HUD::iElementWidth, AMD::HUD::iElementHeight);

	CDXUTComboBox* pComboBox = nullptr;
	g_HUD.m_GUI.AddComboBox(IDC_COMBOBOX_DOF_METHOD, AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, 140, 24, 0, false, &pComboBox);
	pComboBox->AddItem(L"Disabled", nullptr);
	pComboBox->AddItem(L"BoxFFS", nullptr);
	pComboBox->AddItem(L"FFS", nullptr);
	pComboBox->AddItem(L"QuarterResFFS", nullptr);

	pComboBox->SetSelectedByIndex(g_depthOfFieldMode);
	g_HUD.m_GUI.AddCheckBox(IDC_CHECKBOX_DEBUG_CIRCLE_OF_CONFUSION, L"Debug Circle Of Conf", AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, 140, 24, g_bDebugCircleOfConfusion);
	g_HUD.m_GUI.AddCheckBox(IDC_CHECKBOX_SHOW_DOF_RESULT, L"Show Depth Of Field", AMD::HUD::iElementOffset, iY += AMD::HUD::iElementDelta, 140, 24, g_bShowDepthOfFieldResult);
#endif


}


//--------------------------------------------------------------------------------------
// Handle key presses
//--------------------------------------------------------------------------------------
void CALLBACK OnKeyboard(UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext)
{
#define VK_C (67)
#define VK_G (71)
	if (bKeyDown)
	{
		switch (nChar)
		{
		case VK_F1:
			g_bRenderHUD = !g_bRenderHUD;
			break;
		case  VK_TAB:
			g_depthOfFieldMode = static_cast<DepthOfFieldMode>((g_depthOfFieldMode + 1) % 4);
			g_HUD.m_GUI.GetComboBox(IDC_COMBOBOX_DOF_METHOD)->SetSelectedByIndex(g_depthOfFieldMode);
			break;

		case VK_C:
			g_defaultCameraParameterIndex = (g_defaultCameraParameterIndex + 1) % (AMD_ARRAY_SIZE(g_defaultCameraParameters));
			SetupCameraParameters();
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
	break;
	// intentional fall-through to IDC_ENABLE_DEBUG_DRAWING case
	case IDC_CHECKBOX_ENABLE_DEBUG_DRAWING:
	{
		bool bTileDrawingEnabled = g_HUD.m_GUI.GetCheckBox(IDC_CHECKBOX_ENABLE_DEBUG_DRAWING)->GetEnabled() &&
			g_HUD.m_GUI.GetCheckBox(IDC_CHECKBOX_ENABLE_DEBUG_DRAWING)->GetChecked();
		g_HUD.m_GUI.GetRadioButton(IDC_RADIOBUTTON_DEBUG_DRAWING_ONE)->SetEnabled(bTileDrawingEnabled);
		g_HUD.m_GUI.GetRadioButton(IDC_RADIOBUTTON_DEBUG_DRAWING_TWO)->SetEnabled(bTileDrawingEnabled);
	}
	break;

	case IDC_COMBOBOX_DOF_METHOD:
	{
		g_depthOfFieldMode = static_cast<DepthOfFieldMode>(g_HUD.m_GUI.GetComboBox(IDC_COMBOBOX_DOF_METHOD)->GetSelectedIndex());
	}
		break;

	case IDC_CHECKBOX_SHOW_DOF_RESULT:
	{
		g_bShowDepthOfFieldResult = g_HUD.m_GUI.GetCheckBox(IDC_CHECKBOX_SHOW_DOF_RESULT)->GetChecked();
	}
	break;

	case IDC_CHECKBOX_DEBUG_CIRCLE_OF_CONFUSION:
	{
		g_bDebugCircleOfConfusion = g_HUD.m_GUI.GetCheckBox(IDC_CHECKBOX_DEBUG_CIRCLE_OF_CONFUSION)->GetChecked();
	}
	break;

	case IDC_SLIDER_FOCAL_DISTANCE:
	{
		g_FocalDistance = float(g_HUD.m_GUI.GetSlider(IDC_SLIDER_FOCAL_DISTANCE)->GetValue()) / 100.0f;//2位小数点
		swprintf_s(szTemp, 64, L"Focal Distance:%.2f", g_FocalDistance);
		g_HUD.m_GUI.GetStatic(IDC_STATIC_FOCAL_DISTANCE)->SetText(szTemp);
	}
	break;

	case IDC_SLIDER_FSTOP:
	{
		g_fStop = float(g_HUD.m_GUI.GetSlider(nControlID)->GetValue()/10.f);
		swprintf_s(szTemp, 64, L"fStop Value(%.1f ratioFocalLengthToLensDiameter)", g_fStop);
		g_HUD.m_GUI.GetStatic(IDC_STATIC_FSTOP)->SetText(szTemp);
	}
	break;

	case IDC_SLIDER_FOCAL_LENGTH:
	{
		g_FocalLength = float(g_HUD.m_GUI.GetSlider(nControlID)->GetValue());
		swprintf_s(szTemp, 64, L"Focal Length:%.0f", g_FocalLength);
		g_HUD.m_GUI.GetStatic(IDC_STATIC_FOCAL_LENGTH)->SetText(szTemp);
	}
	break;

	case IDC_SLIDER_SENSOR_WIDTH:
	{
		g_SensorWidth = float(g_HUD.m_GUI.GetSlider(nControlID)->GetValue()) / 10.0f;
		swprintf_s(szTemp, 64, L"Sensor Width:%.0f", g_FocalLength);
		g_HUD.m_GUI.GetStatic(IDC_STATIC_SENSOR_WIDTH)->SetText(szTemp);
	}
	break;

	case IDC_SLIDER_MAX_RADIUS:
		g_MaxRadius = float(g_HUD.m_GUI.GetSlider(IDC_SLIDER_MAX_RADIUS)->GetValue());
	case IDC_SLIDER_FORCE_COC:
		g_ForceCircleOfConfusion = float(g_HUD.m_GUI.GetSlider(IDC_SLIDER_FORCE_COC)->GetValue());
		if (g_ForceCircleOfConfusion > static_cast<float>(g_MaxRadius))
		{
			g_ForceCircleOfConfusion = static_cast<float>(g_MaxRadius);
		}
		if (static_cast<float>(g_MaxRadius) < g_ForceCircleOfConfusion)
		{
			g_MaxRadius = static_cast<float>(g_MaxRadius);
		}

		swprintf_s(szTemp, 64, L"Max Radius:%d", g_MaxRadius);
		g_HUD.m_GUI.GetStatic(IDC_STATIC_MAX_RADIUS)->SetText(szTemp);
		g_AMD_DofFX_Desc.m_maxBlurRadius = g_MaxRadius;
		AMD::DepthOfFieldFX_Resize(g_AMD_DofFX_Desc);
		swprintf_s(szTemp, 64, L"Force CoC:%.1f", g_ForceCircleOfConfusion);
		g_HUD.m_GUI.GetStatic(IDC_STATIC_FORCE_COC)->SetText(szTemp);
		break;

	case IDC_BUTTON_SAVE_SCREEN_SHOT:
		g_bSaveScreenShot = true;
		break;
	default:
		break;
	}

	//放大器被我移除掉了
}
