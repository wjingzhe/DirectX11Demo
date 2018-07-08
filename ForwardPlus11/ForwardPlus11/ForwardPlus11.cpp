// TestTriangle.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "ForwardPlus11.h"
#include "../../DXUT/Core/DXUT.h"
#include "../../DXUT/Optional/DXUTcamera.h"
#include "../../DXUT/Optional/SDKmisc.h"
#include "TriangleRender.h"
#include "../src/ForwardPlusRender.h"
#include <algorithm>

using namespace DirectX;
using namespace Triangle;

#pragma warning( disable : 4100 ) // disable unreference formal parameter warnings for /W4 builds
//-----------------------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------------------
static const int TEXT_LINE_HEIGHT = 15;

static TriangleRender s_TriangleRender;
static ForwardPlus11::ForwardPlusRender s_ForwardPlusRender;

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


static float g_fMaxDistance = 500.0f;

//CFirstPersonCamera g_Camera;
CModelViewerCamera g_Camera;
CDXUTDialogResourceManager  g_DialogResourceManager; // manager for shared resources of dialogs
CDXUTTextHelper* g_pTextHelper = nullptr;


LRESULT CALLBACK MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
	void* pUserContext);

bool CALLBACK ModifyDeviceSettings(DXUTDeviceSettings* pDeviceSetting, void* pUserContext);

void CALLBACK OnFrameTimeUpdated(double fTotalTime, float fElaspedTime, void* pUserContext);

bool CALLBACK IsD3D11DeviceAcceptable(const CD3D11EnumAdapterInfo* AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo* DeviceInfo,
	DXGI_FORMAT BackBufferFormat, bool bWindowed, void*pUserContext);

HRESULT CALLBACK OnD3D11DeviceCreated(ID3D11Device* pD3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc/*包含了Buffer格式/SampleDesc*/,
	void* pUserContext);

HRESULT CALLBACK OnD3D11ResizedSwapChain(ID3D11Device* pD3dDevice, IDXGISwapChain* pSwapChain,
	const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext);

void CALLBACK OnD3D11ReleasingSwapChain(void* pUserContext);

void CALLBACK OnD3D11DestroyDevice(void* pUserContext);

void CALLBACK OnFrameRender(ID3D11Device* pD3dDevice, ID3D11DeviceContext* pD3dImmediateContext,
	double fTime, float fElapsedTime, void* pUserContext);

void RenderText();

void InitApp();

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
	DXUTCreateWindow(L"TestTriangle v1.2",hInstance,nullptr,nullptr, 2880, 1880);


	DXUTCreateDevice(D3D_FEATURE_LEVEL_11_0, true, 2880, 1880);

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
			pDeviceSettings->d3d11.sd.SampleDesc.Count = 4;
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
}


//-----------------------------------------------------------------------------------------
// Reject any D3D11 devices that aren;t acceptable by returning false
//-----------------------------------------------------------------------------------------
bool CALLBACK IsD3D11DeviceAcceptable(const CD3D11EnumAdapterInfo* AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo* DeviceInfo,
	DXGI_FORMAT BackBufferFormat, bool bWindowed, void*pUserContext)
{
	return true;
}


// Create any D3D11 resource that aren't dependent on the back buffer
HRESULT CALLBACK OnD3D11DeviceCreated(ID3D11Device * pD3dDevice, const DXGI_SURFACE_DESC * pBackBufferSurfaceDesc, void * pUserContext)
{
	HRESULT hr;

	using namespace DirectX;
		
	XMVECTOR SceneMin, SceneMax;

	ID3D11DeviceContext* pD3dImmediateContext = DXUTGetD3D11DeviceContext();

	//load the scene mesh
	g_SceneMesh.Create(pD3dDevice, L"sponza\\sponza.sdkmesh", nullptr);
	g_SceneAlphaMesh.Create(pD3dDevice, L"sponza\\sponza_alpha.sdkmesh",nullptr);
	g_SceneMesh.CalculateMeshMinMax(&SceneMin, &SceneMax);

	V_RETURN(s_ForwardPlusRender.OnCreateDevice(pD3dDevice,&g_ShaderCache, SceneMin, SceneMax));

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

	// Create render resource here

	static bool bCameraInit = false;
	if (!bCameraInit)
	{
		// Setup the camera's view parameters
		XMVECTOR SceneCenter = 0.5f*(SceneMax + SceneMin);
		XMVECTOR SceneExtents = 0.5f*(SceneMax - SceneMin);
		XMVECTOR BoundaryMin = SceneCenter - 2.0f*SceneExtents;
		XMVECTOR BoundaryMax = SceneCenter + 2.0f*SceneExtents;
		XMVECTOR BoundaryDiff = 4.0f*SceneExtents;

		g_fMaxDistance = XMVectorGetX(XMVector3Length(BoundaryDiff));
		XMVECTOR vEye = SceneCenter - XMVectorSet(0.67f*XMVectorGetX(SceneExtents), 0.5f*XMVectorGetY(SceneExtents), 0.0f, 0.0f);
		XMVECTOR vLookAtPos = SceneCenter - XMVectorSet(0.0f, 0.5f*XMVectorGetY(SceneExtents), 0.0f, 0.0f);
		g_Camera.SetButtonMasks(MOUSE_MIDDLE_BUTTON, MOUSE_WHEEL, MOUSE_LEFT_BUTTON);//left_button can rotate camera
		g_Camera.SetEnablePositionMovement(true);
		g_Camera.SetViewParams(vEye, vLookAtPos);
		g_Camera.SetScalers(0.005f, 0.1f*g_fMaxDistance);

		XMFLOAT3 vBoundaryMin, vBoundaryMax;
		XMStoreFloat3(&vBoundaryMin, BoundaryMin);
		XMStoreFloat3(&vBoundaryMax, BoundaryMax);
		g_Camera.SetClipToBoundary(true, &vBoundaryMin, &vBoundaryMax);
	}

	static bool bFirstPass = true;

	// Generate shaders ( this is an async operation - call AMD::ShaderCache::ShadersReady() to find out if they are complete)
	if (bFirstPass)
	{
		g_ShaderCache.GenerateShaders(AMD::ShaderCache::CREATE_TYPE_COMPILE_CHANGES);//Only compile shaders that have changed(development mode)

		bFirstPass = false;
	}


	V_RETURN(g_DialogResourceManager.OnD3D11CreateDevice(pD3dDevice, pD3dImmediateContext));
	g_pTextHelper = new CDXUTTextHelper(pD3dDevice, pD3dImmediateContext, &g_DialogResourceManager, TEXT_LINE_HEIGHT);

	return hr;
}



HRESULT OnD3D11ResizedSwapChain(ID3D11Device * pD3dDevice, IDXGISwapChain * pSwapChain, const DXGI_SURFACE_DESC * pBackBufferSurfaceDesc, void * pUserContext)
{
	HRESULT hr;

	// Setup the camera's projection parameter
	float fApsectRatio = pBackBufferSurfaceDesc->Width / (FLOAT)pBackBufferSurfaceDesc->Height;
	g_Camera.SetProjParams(XM_PI / 4, fApsectRatio, 1.0f, g_fMaxDistance);


	//create our own depth stencil surface that'bindable as a shader
	V_RETURN(AMD::CreateDepthStencilSurface(&g_pDepthStencilTexture, &g_pDepthStencilSRV, &g_pDepthStencilView,
		DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_R24_UNORM_X8_TYPELESS, pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height, pBackBufferSurfaceDesc->SampleDesc.Count));

	g_pDepthStencilTexture->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("DepthStencilTexture"), "DepthStencilTexture");
	g_pDepthStencilSRV->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("DepthStencilSRV"), "DepthStencilSRV");
	g_pDepthStencilView->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("DepthStencilView"), "DepthStencilView");

	//jingz todo
	//s_TriangleRender.OnResizedSwapChain(pD3dDevice, pBackBufferSurfaceDesc);
	s_ForwardPlusRender.OnResizedSwapChain(pD3dDevice, pBackBufferSurfaceDesc);


	V_RETURN(g_DialogResourceManager.OnD3D11ResizedSwapChain(pD3dDevice, pBackBufferSurfaceDesc));

	return hr;
}

void OnD3D11ReleasingSwapChain(void * pUserContext)
{
	//Release my RT/SRV/Buffer

	SAFE_RELEASE(g_pDepthStencilView);
	SAFE_RELEASE(g_pDepthStencilSRV);
	SAFE_RELEASE(g_pDepthStencilTexture);

	//s_TriangleRender.OnReleasingSwapChain();
	s_ForwardPlusRender.OnReleasingSwapChain();

	g_DialogResourceManager.OnD3D11ReleasingSwapChain();
}

void OnD3D11DestroyDevice(void * pUserContext)
{
	g_DialogResourceManager.OnD3D11DestroyDevice();
	SAFE_DELETE(g_pTextHelper);
	DXUTGetGlobalResourceCache().OnDestroyDevice();

	g_SceneMesh.Destroy();
	g_SceneAlphaMesh.Destroy();


	SAFE_RELEASE(g_pSamplerAnisotropic);

	SAFE_RELEASE(g_pDepthStencilView);
	SAFE_RELEASE(g_pDepthStencilSRV);
	SAFE_RELEASE(g_pDepthStencilTexture);



	//s_TriangleRender.OnD3D11DestroyDevice(pUserContext);
	s_ForwardPlusRender.OnDestroyDevice(pUserContext);
	
	//AMD
	g_ShaderCache.OnDestroyDevice();
	TIMER_Destroy();
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

	ClearD3D11DeviceContext();

	const DXGI_SURFACE_DESC* pBackBufferDesc = DXUTGetDXGIBackBufferSurfaceDesc();


	//Clear the backBuffer and depth stencil
	float ClearColor[4] = { 0.0013f, 0.0015f, 0.0050f, 0.0f };
	ID3D11RenderTargetView* pRTV = DXUTGetD3D11RenderTargetView();
	pD3dImmediateContext->ClearRenderTargetView(pRTV, ClearColor);
	pD3dImmediateContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	// Switch off alpha blending
	// float BlendFactor[4] = { 1.0f,1.0f,0.0f,0.0f }; 这些参数没什么用
	pD3dImmediateContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);

	//Render or objects here..
	if (g_ShaderCache.ShadersReady(true))
	{
		TIMER_Begin(0, L"Render");
		//render triangle for debug
		pD3dImmediateContext->OMSetDepthStencilState(nullptr, 0x00);
		pD3dImmediateContext->RSSetState(nullptr);
		//s_TriangleRender.OnRender(pD3dDevice, pD3dImmediateContext,&g_Camera, pRTV, g_pDepthStencilView);

		std::vector< CDXUTSDKMesh*>MeshArray;
		MeshArray.push_back(&g_SceneMesh);

		std::vector< CDXUTSDKMesh*>AlphaMeshArray;
		AlphaMeshArray.push_back(&g_SceneAlphaMesh);

		const DXGI_SURFACE_DESC* pBackBufferSurfaceDes = DXUTGetDXGIBackBufferSurfaceDesc();
		s_ForwardPlusRender.OnRender(pD3dDevice, pD3dImmediateContext,&g_Camera ,pRTV,pBackBufferSurfaceDes,
			g_pDepthStencilTexture,g_pDepthStencilView,g_pDepthStencilSRV,
			fElapsedTime, MeshArray.data(),1, AlphaMeshArray.data(),1);

		TIMER_End(); // Render

		RenderText();

		DXUT_EndPerfEvent();
	}
	else
	{
		// Render shader cache progress if still processing
		pD3dImmediateContext->OMSetRenderTargets(1, (ID3D11RenderTargetView*const*)&pRTV, g_pDepthStencilView);
		g_ShaderCache.RenderProgress(g_pTextHelper, 15, XMVectorSet(1.0f, 1.0f, 0.0f, 1.0f));

		Sleep(1);
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

	//g_pTextHelper->SetInsertionPos(5, DXUTGetDXGIBackBufferSurfaceDesc()->Height - AMD::HUD::iElementDelta);
	//g_pTextHelper->DrawTextLine(L"Toggle GUI    : F1");

	g_pTextHelper->End();
}

void InitApp()
{

}
