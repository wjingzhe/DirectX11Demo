#pragma once

#include "resource.h"


#include <DirectXMath.h>


enum DepthOfFieldMode
{
	DOF_Disabled = 0,
	DOF_BoxFastFilterSpread = 1,
	DOF_FastFilterSpread = 2,
	DOF_QuarterResFastFilterSpread = 3,
};

#define AMD_ARRAY_SIZE( arr )     ( sizeof(arr) / sizeof(arr[0]) )