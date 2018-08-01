// TestTriangle.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "TestTriangle.h"
#include "../../DXUT/Core/DXUT.h"
#include "../../DXUT/Optional/DXUTcamera.h"
#include "../../DXUT/Optional/SDKmisc.h"
#include "../source/DeferredDecalRender.h"
#include "../source/TriangleRender.h"
#include "../../DXUT/Core/DDSTextureLoader.h"
#include "../../DXUT/Core/WICTextureLoader.h"
#include <algorithm>

using namespace DirectX;

#define TRIANGLE
#define DECAL

#ifdef TRIANGLE
static Triangle::TriangleRender s_TriangleRender;
#endif

#ifdef DECAL
static PostProcess::DeferredDecalRender s_DeferredDecalRender;
#endif


//-------------------------------------
// AMD helper classes defined here
//-------------------------------------

static AMD::ShaderCache g_ShaderCache;


// Depth stencil data
ID3D11Texture2D* g_pDepthStencilTexture = nullptr;
ID3D11DepthStencilView* g_pDepthStencilView = nullptr;
ID3D11ShaderResourceView* g_pDepthStencilSRV = nullptr;

ID3D11Texture2D* g_pTempDepthStencilTexture = nullptr;
ID3D11DepthStencilView* g_pTempDepthStencilView = nullptr;
ID3D11ShaderResourceView* g_pTempDepthStencilSRV = nullptr;

ID3D11Texture2D* g_pDecalTexture = nullptr;
ID3D11ShaderResourceView* g_pDecalTextureSRV = nullptr;
static float g_fMaxDistance = 1000.0f;
//CFirstPersonCamera g_Camera;
CModelViewerCamera g_Camera;

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

void InitApp();

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

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
	DXUTSetCursorSettings(true, false);
	DXUTCreateWindow(L"TestTriangle v1.2");


	DXUTCreateDevice(D3D_FEATURE_LEVEL_11_0, true, 1920, 1080);
	
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
	

	V_RETURN(CreateWICTextureFromFile(pD3dDevice,L"../../len_full.jpg",(ID3D11Resource**) &g_pDecalTexture, &g_pDecalTextureSRV));
	
	XMVECTOR SceneMin, SceneMax;

	ID3D11DeviceContext* pD3dImmediateContext = DXUTGetD3D11DeviceContext();
	

	//load the scene mesh

#ifdef TRIANGLE
	s_TriangleRender.GenerateMeshData(100, 100, 100, 6, 0.0f, -100.0f, 500.0f);
	Triangle::TriangleRender::CalculateSceneMinMax(s_TriangleRender.m_MeshData, &SceneMin, &SceneMax);
	V_RETURN(s_TriangleRender.OnD3DDeviceCreated(pD3dDevice, pBackBufferSurfaceDesc, pUserContext));
#endif

#ifdef DECAL
	s_DeferredDecalRender.GenerateMeshData(100, 100, 100, 6);
	//PostProcess::DeferredDecalRender::CalculateSceneMinMax(s_DeferredDecalRender.m_MeshData, &SceneMin, &SceneMax);
	V_RETURN(s_DeferredDecalRender.OnD3DDeviceCreated(pD3dDevice, pBackBufferSurfaceDesc, pUserContext));
	s_DeferredDecalRender.SetDecalTextureSRV(g_pDecalTextureSRV);
#endif

	static bool bFirstPass = true;


	// One-time setup
	if (bFirstPass)
	{
		// Setup the camera's view parameters
		XMVECTOR SceneCenter = 0.5f*(SceneMax + SceneMin);
		XMVECTOR SceneExtends = 0.5f*(SceneMax - SceneMin);
		XMVECTOR BoundaryMin = SceneCenter - 20.0f*SceneExtends;
		XMVECTOR BoundaryMax = SceneCenter + 20.0f*SceneExtends;
		XMVECTOR BoundaryDiff = 40.0f*SceneExtends;


		XMVECTOR vEye = XMVectorSet(0.0, 0.0f, 0.0f,0.0f);
		XMVECTOR vLookAtPos = SceneCenter - XMVectorSet(0.0f,0.0f, -10.0f, 0.0f);//要看到前面的剑法
		g_Camera.SetButtonMasks();//left_button can rotate camera
		g_Camera.SetEnablePositionMovement(true);
		g_Camera.SetViewParams(vEye, vLookAtPos);
		g_Camera.SetScalers(0.005f, 0.1f*g_fMaxDistance);

		XMFLOAT3 vBoundaryMin, vBoundaryMax;
		XMStoreFloat3(&vBoundaryMin, BoundaryMin);
		XMStoreFloat3(&vBoundaryMax, BoundaryMax);
		g_Camera.SetClipToBoundary(true, &vBoundaryMin, &vBoundaryMax);
#ifdef DECAL
		s_DeferredDecalRender.SetDecalPosition(SceneCenter - XMVectorSet(0.0f, 50.0f, 0.0f, 0.0f));
#endif // DECAL


	}

	// Generate shaders ( this is an async operation - call AMD::ShaderCache::ShadersReady() to find out if they are complete)
	if (bFirstPass)
	{
		// Add the applications shaders to the cache
		AddShadersToCache();
		
		g_ShaderCache.GenerateShaders(AMD::ShaderCache::CREATE_TYPE_COMPILE_CHANGES);//Only compile shaders that have changed(development mode)

		bFirstPass = false;
	}

	pD3dImmediateContext = nullptr;

	return S_OK;
}

HRESULT AddShadersToCache()
{
	HRESULT hr = E_FAIL;

#ifdef TRIANGLE
	s_TriangleRender.AddShadersToCache(&g_ShaderCache);
#endif

#ifdef DECAL
	s_DeferredDecalRender.AddShadersToCache(&g_ShaderCache);
#endif

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

	//create our own depth stencil surface that'bindable as a shader
	V_RETURN(AMD::CreateDepthStencilSurface(&g_pTempDepthStencilTexture, &g_pTempDepthStencilSRV, &g_pTempDepthStencilView,
		DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_R24_UNORM_X8_TYPELESS, pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height, pBackBufferSurfaceDesc->SampleDesc.Count));
	g_pTempDepthStencilTexture->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("Temp DepthStencilTexture"), "Temp DepthStencilTexture");
	g_pTempDepthStencilSRV->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("Temp DepthStencilSRV"), "Temp DepthStencilSRV");
	g_pTempDepthStencilView->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen("Temp DepthStencilView"), "Temp DepthStencilView");

#ifdef TRIANGLE
	s_TriangleRender.OnResizedSwapChain(pD3dDevice, pBackBufferSurfaceDesc);
#endif

#ifdef DECAL
	s_DeferredDecalRender.OnResizedSwapChain(pD3dDevice, pBackBufferSurfaceDesc);
#endif



	return S_OK;
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

#ifdef TRIANGLE
	s_TriangleRender.OnReleasingSwapChain();
#endif
#ifdef DECAL
	s_DeferredDecalRender.OnReleasingSwapChain();
#endif

}

void OnD3D11DestroyDevice(void * pUserContext)
{
	DXUTGetGlobalResourceCache().OnDestroyDevice();
	g_ShaderCache.OnDestroyDevice();

	SAFE_RELEASE(g_pDepthStencilDefaultDS);

	SAFE_RELEASE(g_pDepthStencilView);
	SAFE_RELEASE(g_pDepthStencilSRV);
	SAFE_RELEASE(g_pDepthStencilTexture);

	SAFE_RELEASE(g_pTempDepthStencilView);
	SAFE_RELEASE(g_pTempDepthStencilSRV);
	SAFE_RELEASE(g_pTempDepthStencilTexture);

	SAFE_RELEASE(g_pDecalTexture);
	SAFE_RELEASE(g_pDecalTextureSRV);

#ifdef TRIANGLE
	s_TriangleRender.OnD3D11DestroyDevice(pUserContext);
#endif
#ifdef DECAL
	s_DeferredDecalRender.OnD3D11DestroyDevice(pUserContext);
#endif
	
	
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

	ClearD3D11DeviceContext();

	const DXGI_SURFACE_DESC* pBackBufferDesc = DXUTGetDXGIBackBufferSurfaceDesc();


	//Clear the backBuffer and depth stencil
	float ClearColor[4] = { 1.0f,1.0f,1.0f,1.0f };
	ID3D11RenderTargetView* pRTV = DXUTGetD3D11RenderTargetView();
	pD3dImmediateContext->ClearRenderTargetView(pRTV, ClearColor);
	pD3dImmediateContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);






	//Render or objects here..
	if (g_ShaderCache.ShadersReady(true))
	{

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
#endif

	
	}
	else
	{
		// Render shader cache progress if still processing
		pD3dImmediateContext->OMSetRenderTargets(1, (ID3D11RenderTargetView*const*)&pRTV, g_pDepthStencilView);
		//g_ShaderCache.RenderProgress(g_pTextHelper, 15, XMVectorSet(1.0f, 1.0f, 0.0f, 1.0f));

		Sleep(1);
	}

}

void InitApp()
{
}
