#pragma once

//jingz copy from d3dUtil.h by Frank Luna (C)
//***************************************************************************************
// d3dUtil.h by Frank Luna (C) 2011 All Rights Reserved.
//***************************************************************************************


//内存測漏语句
//参照博文 http://blog.sina.com.cn/s/blog_51396f890100qjyc.html
#if defined(DEBUG) || defined (_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include <d3dx11.h> //项目需要配置dx sdk路径
#include <xnamath.h>
#include <dxerr.h>
#include <cassert>
#include <ctime>
#include <algorithm>
#include <string>
#include <sstream>
#include <fstream>
#include <vector>


#if defined(DEBUG) || defined(_DEBUG)
	#ifndef HR
	#define  HR(x) \
	{\
		HRESULT hr = (x);\
		if(FAILED(hr)) \
		{\
			DXTrace(__FILE__,(DWORD)__LINE__,hr,L#x,true);\
		}\
	}
	#endif // !HR

#else
#ifndef HR
#define HR(x) (x)
#endif
#endif


#define ReleaseCOM(x) {if(x){x->Release();x = nullptr;}}

#define SafeDelete(x) {delete x;x = nullptr;}


class d3dHelper
{
public:
	// Does not work with compressed formats.
	static ID3D11ShaderResourceView * CreateTexture2DArraySRV(
		ID3D11Device * device,
		ID3D11DeviceContext * context,
		std::vector<std::wstring> &fileName,
		DXGI_FORMAT format = DXGI_FORMAT_FROM_FILE,
		UINT  filter = D3DX11_FILTER_NONE,
		UINT mipFilter = D3DX11_FILTER_LINEAR
		);

private:

};
