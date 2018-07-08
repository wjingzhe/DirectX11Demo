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

//-------------------------------------------------------------------------------
// File: ForwardPlus11.hlsl
//
// HLSL file for the ForwardPlus11 sample. Tiled light culling.
//------------------------------------------------------------------------------


#include "ForwardPlus11Common.hlsl"

#define FLT_MAX 3.402823466e+38F


//----------------------------------
//Textures and Buffers
//-----------------------------------
Buffer<float4> g_PointLightBufferCenterAndRadius:register(t0);
Buffer<float4> g_SpotLightBufferCenterAndRadius:register(t1);

#if (USE_DEPTH_BOUNDS == 1) // non-MSAA
Texture2D<float> g_DepthTexture : register(t2);
#elif ( USE_DEPTH_BOUNDS == 2 ) // MSAA
Texture2DMS<float> g_DepthTexture : register(t2);
#endif

RWBuffer<uint> g_PerTileLightIndexBufferOut:register(u0);

//-----------------------------------------------------------
// Group Shared Memory ( aka local data share, or LDS)
//-----------------------------------------------------------
#if(USE_DEPTH_BOUNDS == 1) || (USE_DEPTH_BOUNDS == 2)
groupshared uint ldsMaxZ;
groupshared uint ldsMinZ;
#endif

groupshared uint ldsLightIndexCounter;
groupshared uint ldsLightIndex[MAX_NUM_LIGHTS_PER_TILE];

//---------------------------------------------------------------
// Helper functions
//---------------------------------------------------------------

//This creates the standard Hessian-normal-form plane  equation from three points
// except it is simplified for the case where the first point is the origin(0,0)
float3 CreatePlaneEquation(float3 b, float3 c)
{
	// normalize(cross(b-a,c-a)),except we know 'a' is the origin
	// also, typically there would be a fouth term of the plane equation,
	// - (n dot a),except know 'a' is the origin
	return normalize(cross(b, c));
}


// point-plane distance, simplified for the case where the plane passes through the origin
float GetSignedDistanceFromPlane(float3 p, float3 eqn)
{
	// dot(eqn.xyz,p) + eqn.w,except we know eqn.w is zero
	// see CreatePlaneEquation above
	return dot(eqn, p);
}


bool TestFrustumSides(float3 c, float r, float3 plane0, float3 plane1, float3 plane2, float3 plane3)
{
	bool intersectingOrInside0 = GetSignedDistanceFromPlane(c, plane0) < r;
	bool intersectingOrInside1 = GetSignedDistanceFromPlane(c, plane1) < r;
	bool intersectingOrInside2 = GetSignedDistanceFromPlane(c, plane2) < r;
	bool intersectingOrInside3 = GetSignedDistanceFromPlane(c, plane3) < r;

	return (intersectingOrInside0 && intersectingOrInside1 && intersectingOrInside2 && intersectingOrInside3);
}

// calculate the number of tiles in the horizontal direction
uint GetNumTilesX()
{
	return (uint)((g_uWindowWidth + TILE_RES - 1) / (float)TILE_RES);
}

// calculate the number of tiles in the vertical direction
uint GetNumTilesY()
{
	return (uint)((g_uWindowHeight + TILE_RES - 1) / (float)TILE_RES);
}

// convert a point from post-projection space into view space
float4 ConvertProjToView(float4 p)
{
	p = mul(p, g_mProjectionInv);
	p /= p.w;
	return p;
}

// convert a depth value from post-projection space into view space
float ConvertProjDepthToView(float z)
{
	z = 1.f / (z*g_mProjectionInv._34 + g_mProjectionInv._44);
	return z;
}

#if(USE_DEPTH_BOUNDS == 1) // non-MSAA
void CalculateMinMaxDepthInLds(uint3 globalThreadIndex)
{
	float depth = g_DepthTexture.Load(uint3(globalThreadIndex.x, globalThreadIndex.y, 0)).x;
	float viewPosZ = ConvertProjDepthToView(depth);
	uint z = asuint(viewPosZ);
	if (depth != 0.f)
	{
		InterlockedMax(ldsMaxZ, z);
		InterlockedMin(ldsMinZ, z);
	}
}
#endif

#if(USE_DEPTH_BOUNDS == 2) //MSAA
void CalculateMinMaxDepthInLdsMSAA(uint3 globalThreadIndex, uint depthBufferNumSamples)
{
	float maxZForThisPixel = 0.f;
	float minZForThisPixel = FLT_MAX;

	float depth0 = g_DepthTexture.Load(uint2(globalThreadIndex.x, globalThreadIndex.y), 0).x;
	float viewPosZ0 = ConvertProjDepthToView(depth0);
	if (depth0 != 0.f)
	{
		maxZForThisPixel = max(maxZForThisPixel, viewPosZ0);
		minZForThisPixel = min(minZForThisPixel, viewPosZ0);
	}


	for (uint sampleIndex = 1; sampleIndex < depthBufferNumSamples; sampleIndex++)
	{
		float depth = g_DepthTexture.Load(uint2(globalThreadIndex.x, globalThreadIndex.y), sampleIndex).x;
		float viewPosZ = ConvertProjDepthToView(depth);
		if (depth != 0.f)
		{
			maxZForThisPixel = max(maxZForThisPixel, viewPosZ);
			minZForThisPixel = min(minZForThisPixel, viewPosZ);
		}
	}


	uint zMaxForThisPixel = asuint(maxZForThisPixel);
	uint zMinForThisPixel = asuint(minZForThisPixel);

	InterlockedMax(ldsMaxZ, zMaxForThisPixel);
	InterlockedMin(ldsMinZ, zMinForThisPixel);

}
#endif


//--------------------------------------------------------------
// Parameters for the light culling shader
//--------------------------------------------------------------
#define NUM_THREADS_X TILE_RES
#define NUM_THREADS_Y TILE_RES
#define NUM_THREADS_PER_TILE (NUM_THREADS_X*NUM_THREADS_Y)

//--------------------------------------------------------------
// Light culling shader
//--------------------------------------------------------------
[numthreads(NUM_THREADS_X, NUM_THREADS_Y,1)]
void CullLightsCS(uint3 globalIndex:SV_DispatchThreadID, uint3 localIndex : SV_GroupThreadID, uint3 groupIndex : SV_GroupID)
{
	uint localIndexFlattened = localIndex.x + localIndex.y*NUM_THREADS_X;

	if (localIndexFlattened == 0)
	{
#if (USE_DEPTH_BOUNDS == 1 || USE_DEPTH_BOUNDS ==2)
		ldsMinZ = 0x7f7fffff;// FLT_MAX as a uint 为什么不是0x7FFFFFFF
		ldsMaxZ = 0;
#endif
		ldsLightIndexCounter = 0;
	}

	float3 frustumEqn0, frustumEqn1, frustumEqn2, frustumEqn3;
	{
		// construct frustum for this tile
		uint pxm = TILE_RES*groupIndex.x;
		uint pym = TILE_RES*groupIndex.y;
		uint pxp = TILE_RES*(groupIndex.x + 1);
		uint pyp = TILE_RES*(groupIndex.y + 1);

		uint uWindowWidthEvenlyDivisibleByTileRes = TILE_RES*GetNumTilesX();
		uint uWindowHeightEvenlyDivisibleByTileRes = TILE_RES*GetNumTilesY();

		// four corners of the tile,clockwise from top-left
		float3 frustum0 = ConvertProjToView(
			float4(pxm / (float)uWindowWidthEvenlyDivisibleByTileRes*2.0f - 1.0f,
			(uWindowHeightEvenlyDivisibleByTileRes - pym) / (float)uWindowHeightEvenlyDivisibleByTileRes*2.f - 1.f,
				1.f, 1.f)
		).xyz;
		float3 frustum1 = ConvertProjToView(
			float4(pxp / (float)uWindowWidthEvenlyDivisibleByTileRes*2.0f - 1.0f,
			(uWindowHeightEvenlyDivisibleByTileRes - pym) / (float)uWindowHeightEvenlyDivisibleByTileRes*2.f - 1.f,
				1.f, 1.f)
		).xyz;
		float3 frustum2 = ConvertProjToView(
			float4(pxp / (float)uWindowWidthEvenlyDivisibleByTileRes*2.0f - 1.0f,
			(uWindowHeightEvenlyDivisibleByTileRes - pyp) / (float)uWindowHeightEvenlyDivisibleByTileRes*2.f - 1.f,
				1.f, 1.f)
		).xyz;
		float3 frustum3 = ConvertProjToView(
			float4(pxm / (float)uWindowWidthEvenlyDivisibleByTileRes*2.0f - 1.0f,
			(uWindowHeightEvenlyDivisibleByTileRes - pyp) / (float)uWindowHeightEvenlyDivisibleByTileRes*2.f - 1.f,
				1.f, 1.f)
		).xyz;

		//create plane equation for the four sides of the frustum.
		// which the positive half-space outside the frustum
		// (and remenber view space is left handed,so use the left-hand rule to determine cross product direction)
		frustumEqn0 = CreatePlaneEquation(frustum0, frustum1);
		frustumEqn1 = CreatePlaneEquation(frustum1, frustum2);
		frustumEqn2 = CreatePlaneEquation(frustum2, frustum3);
		frustumEqn3 = CreatePlaneEquation(frustum3, frustum0);
	}

	GroupMemoryBarrierWithGroupSync();

	//Calculate the min and max depth for this tile,
	// to form the front and back of the frustum

#if(USE_DEPTH_BOUNDS == 1 || USE_DEPTH_BOUNDS == 2)
	float maxZ = 0.f;
	float minZ = FLT_MAX;

#if(USE_DEPTH_BOUNDS == 1) // non-MSAA
	CalculateMinMaxDepthInLds(globalIndex);
#elif(USE_DEPTH_BOUNDS == 2) // MSAA
	uint depthBufferWidth, depthBufferHeight, depthBufferNumSamples;
	g_DepthTexture.GetDimensions(depthBufferWidth, depthBufferHeight, depthBufferNumSamples);
	CalculateMinMaxDepthInLdsMSAA(globalIndex, depthBufferNumSamples);
#endif

	GroupMemoryBarrierWithGroupSync();
	maxZ = asfloat(ldsMaxZ);
	minZ = asfloat(ldsMinZ);

#endif


	// loop over the lights and do a sphere vs frustum interdection test
	uint uNumPointLights = g_uNumLights & 0xFFFFu;
	for (uint i = localIndexFlattened; i < uNumPointLights; i += NUM_THREADS_PER_TILE)
	{
		float4 center = g_PointLightBufferCenterAndRadius[i];
		float r = center.w;
		center.xyz = mul(float4(center.xyz, 1), g_mWorldView).xyz;


		//test if sphere is intersecting or inside frustum
		if (TestFrustumSides(center.xyz, r, frustumEqn0, frustumEqn1, frustumEqn2, frustumEqn3))
		{
#if(USE_DEPTH_BOUNDS !=0)
			if (-center.z + minZ < r && center.z - maxZ < r)
#else
			if (-center.z < r)
#endif
			{
				// do a thread-safe increment of the list counter
				// and put the index of this light into the list
				uint dstIndex = 0;
				InterlockedAdd(ldsLightIndexCounter, 1, dstIndex);
				ldsLightIndex[dstIndex] = i;
			}
		}
	}//for

	GroupMemoryBarrierWithGroupSync();


	// and again for spot lights
	uint uNumPointLightsInThisTile = ldsLightIndexCounter;
	uint uNumSpotLights = (g_uNumLights & 0xFFFF0000u) >> 16;
	for (uint j = localIndexFlattened; j < uNumSpotLights; j += NUM_THREADS_PER_TILE)
	{
		float4 center = g_SpotLightBufferCenterAndRadius[j];
		float r = center.w;
		center.xyz = mul(float4(center.xyz, 1), g_mWorldView).xyz;

		// test if sphere is intersecting or inside frustum
		if (TestFrustumSides(center.xyz, r, frustumEqn0, frustumEqn1, frustumEqn2, frustumEqn3))
		{

#if(USE_DEPTH_BOUNDS != 0)
			if (-center.z + minZ < r && center.z - maxZ < r)
#else
			if (-center.z < r)
#endif
			{
				// do a thread-safe increment of the list counter
				// and put the index of his light into the list
				uint dstIndex = 0;
				InterlockedAdd(ldsLightIndexCounter, 1, dstIndex);
				ldsLightIndex[dstIndex] = j;
			}
		}
	}//for


	GroupMemoryBarrierWithGroupSync();
	
	// write back
	{
		uint tileIndexFlattened = groupIndex.x + groupIndex.y*GetNumTilesX();
		uint startOffset = g_uMaxNumLightsPerTile*tileIndexFlattened;

		for (uint i = localIndexFlattened; i < uNumPointLightsInThisTile; i += NUM_THREADS_PER_TILE)
		{
			// per-tile list of light indices
			g_PerTileLightIndexBufferOut[startOffset + i] = ldsLightIndex[i];
		}

		for (uint j = (localIndexFlattened + uNumPointLightsInThisTile); j < ldsLightIndexCounter; j += NUM_THREADS_PER_TILE)
		{
			// per-tile list of light indices
			g_PerTileLightIndexBufferOut[startOffset + j+1] = ldsLightIndex[j];
		}
		
		if (localIndexFlattened == 0)
		{
			// mark the end of each per-tile list with a sentinel(point lights)
			g_PerTileLightIndexBufferOut[startOffset + uNumPointLightsInThisTile] = LIGHT_INDEX_BUFFER_SENTINEL;

			// mark the end of each per - tile list with a sentinel(spot lights)
			g_PerTileLightIndexBufferOut[startOffset + ldsLightIndexCounter + 1] = LIGHT_INDEX_BUFFER_SENTINEL;
		}
	}
}