#include "ForwardPlus11Common.hlsl"

#define FLT_MAX 3.402823466e+38F

//
//Textures and Buffers
//
Buffer<float4> g_PointLightBufferCenterAndRadius:register(t0);
Buffer<float4> g_SpotLightBufferCenterAndRadius:register(t1);

#if (USE_DEPTH_BOUNDS == 1) // non-MSAA
Texture2D<float> g_DepthTexture:register(t2);
#elif(USE_DEPTH_BOUNDS == 2) // MSAA
Texture2DMS<float> g_DepthTexture:register(t2);
#endif


RWBuffer<uint> g_PerTileLightIndexBufferOut:register(u0);

//
// Group Shared Memory (aka local data share,or LDS)
//
#if(USE_DEPTH_BOUNDS==1)||(USE_DEPTH_BOUNDS==2)
groupshared uint ldsMaxViewZ;
groupshared uint ldsMinViewZ;
#endif

groupshared uint ldsLightIndexCounter;
groupshared uint ldsLightIndex[MAX_NUM_LIGHTS_PER_TILE];

//groupshared float3 TopEqn, RightEqn, BottomEqn, LeftEqn;

//Helper functions

// This create the statndard Hessian-normal-form plane equation from three points
// except it is simplified for the case where the first point is the origin(0,0),normal is the plane equation
float3 CreatePlaneEquation(float3 b, float3 c)
{
	// normalize(cross(b-a,c-a)),except we know 'a' is the origin
	// also,typically there would be a fouth term of the plane equation,
	// -(n dot a),except know 'a' is the origin
	return normalize(cross(b, c));
}

// point-plane distance, simplified for the case where the plane passes through the origin
float GetSignedDistanceFromPlane(float3 p, float3 eqn)
{
	// eqn is normalized，and pass origin point
	// so (Ax+By+Cz+d)/sqrt(A^2+B^2+C^2) = Ax+By+Cz = dot(A,X) = distance

	// dot(eqn.xyz,p) + eqn.w,except we know eqn.w is zero
	// see CreatePlaneEquation above
	return dot(eqn, p);
}


bool TestFrustumSides(float3 center, float radius, float3 plane0, float3 plane1, float3 plane2, float3 plane3)
{
	bool intersectingOrInside0 = GetSignedDistanceFromPlane(center, plane0) < radius;
	bool intersectingOrInside1 = GetSignedDistanceFromPlane(center, plane1) < radius;
	bool intersectingOrInside2 = GetSignedDistanceFromPlane(center, plane2) < radius;
	bool intersectingOrInside3 = GetSignedDistanceFromPlane(center, plane3) < radius;

	return (intersectingOrInside0 && intersectingOrInside1 && intersectingOrInside2 && intersectingOrInside3);
}


// calculate the number of tiles in the horizontal direction
uint GetNumTilesX()
{
	return (uint)((g_uWindowWidth + TILE_SIZE - 1) / (float)TILE_SIZE);
}

// calculate the number of tiles in the vertical direction
uint GetNumTilesY()
{
	return (uint)((g_uWindowHeight + TILE_SIZE - 1) / (float)TILE_SIZE);
}

//Convert a point from post-projection space into view space
float4 ConvertProjToView(float4 p)
{
	p = mul(p, g_mProjectionInv);
	p /= p.w;
	return p;
}


// convert a depth value from post-projection space into view space
float ConvertProjDepthToView(float z)
{
	z = 1.0f / (z*g_mProjectionInv._34 + g_mProjectionInv._44);
	return z;
}

#if(USE_DEPTH_BOUNDS == 1)// non_MSAA
void CalculateMinMaxDepthInLds(uint3 globalThreadIndex)
{
	float depth = g_DepthTexture.Load(uint3(globalThreadIndex.x, globalThreadIndex.y, 0)).x;
	float viewPosZ = ConvertProjDepthToView(depth);
	uint z = asuint(viewPosZ);
	if (depth != 0.f)
	{
		InterlockedMax(ldsMaxViewZ, z);
		InterlockedMin(ldsMinViewZ, z);
	}
}
#elif(USE_DEPTH_BOUNDS == 2) //MSAA
void CalculateMinMaxDepthInLdsMSAA(uint3 globalThreadIndex, uint depthBufferNumSamples)
{
	float maxViewZ = 0.f;
	float minViewZ = FLT_MAX;

	//float depth0 = g_DepthTexture.Load(uint2(globalThreadIndex.x, globalThreadIndex.y), 0).x;
	//float viewPosZ0 = ConvertProjDepthToView(depth0);
	//if (depth0 != 0.0f)
	//{
	//	maxViewZ = max(maxViewZ, viewPosZ0);
	//	minViewZ = min(minViewZ, viewPosZ0);
	//}

	for (uint sampleIndex = 0; sampleIndex < depthBufferNumSamples; sampleIndex++)
	{
		float depth = g_DepthTexture.Load(uint2(globalThreadIndex.x, globalThreadIndex.y), sampleIndex).x;
		float viewPosZ = ConvertProjDepthToView(depth);
		if (depth > 1E-5)
		{
			maxViewZ = max(maxViewZ, viewPosZ);
			minViewZ = min(minViewZ, viewPosZ);
		}
	}

	uint zMaxForThisPixel = asuint(maxViewZ);
	uint zMinForThisPixel = asuint(minViewZ);

	InterlockedMax(ldsMaxViewZ, zMaxForThisPixel);
	InterlockedMin(ldsMinViewZ, zMinForThisPixel);
}

#endif


//
// Parameters for the light culling shader
//
#define NUM_THREADS_X TILE_SIZE
#define NUM_THREADS_Y TILE_SIZE
#define NUM_THREADS_PER_TILE (NUM_THREADS_X*NUM_THREADS_Y)

//------------------------
// Light culling shader
//--------------
[numthreads(NUM_THREADS_X, NUM_THREADS_Y,1)]
void CullLightsCS(uint3 globalIndex:SV_DispatchThreadID, uint3 localIndex : SV_GroupThreadID, uint3 groupIndex : SV_GroupID)
{
	uint localIndexFlattened = localIndex.x + localIndex.y*NUM_THREADS_X;
	if (localIndexFlattened == 0)
	{
#if(USE_DEPTH_BOUNDS ==1 )||(USE_DEPTH_BOUNDS == 2)
		ldsMinViewZ = 0x7f7fffff;// FLT_MAX as a uint 为什么不是0x7FFFFFFF
		ldsMaxViewZ = 0;
#endif
		ldsLightIndexCounter = 0;

		

	}

	float3 TopEqn, RightEqn, BottomEqn, LeftEqn;
	{
		// construct frustum for this tile
		uint pxm = TILE_SIZE* groupIndex.x;
		uint pym = TILE_SIZE* groupIndex.y;
		uint pxp = TILE_SIZE* (groupIndex.x + 1);
		uint pyp = TILE_SIZE* (groupIndex.y + 1);

		uint uWindowWidthEvenlyDivisibleByTileSize = TILE_SIZE* GetNumTilesX();
		uint uWindowHeightEvenlyDivisibleByTileSize = TILE_SIZE* GetNumTilesY();

		//four corners of the tile,clockwise from top-left
		float3 TopLeft = ConvertProjToView(
			float4(pxm / (float)uWindowWidthEvenlyDivisibleByTileSize*2.0f - 1.0f,
			(uWindowHeightEvenlyDivisibleByTileSize - pym) / (float)uWindowHeightEvenlyDivisibleByTileSize*2.0f - 1.0f,
				1.0f, 1.0f)
		).xyz;

		float3 TopRight = ConvertProjToView(
			float4(pxp / (float)uWindowWidthEvenlyDivisibleByTileSize*2.0f - 1.0f,
			(uWindowHeightEvenlyDivisibleByTileSize - pym) / (float)uWindowHeightEvenlyDivisibleByTileSize*2.0f - 1.0f,
				1.0f, 1.0f)
		).xyz;

		float3 BottomRight = ConvertProjToView(
			float4(pxp / (float)uWindowWidthEvenlyDivisibleByTileSize*2.0f - 1.0f,
			(uWindowHeightEvenlyDivisibleByTileSize - pyp) / (float)uWindowHeightEvenlyDivisibleByTileSize*2.0f - 1.0f,
				1.0f, 1.0f)
		).xyz;

		float3 BottomLeft = ConvertProjToView(
			float4(pxm / (float)uWindowWidthEvenlyDivisibleByTileSize*2.0f - 1.0f,
			(uWindowHeightEvenlyDivisibleByTileSize - pyp) / (float)uWindowHeightEvenlyDivisibleByTileSize*2.0f - 1.0f,
				1.0f, 1.0f)
		).xyz;


		// create plane equation for the four sides of the frustum.
		// which the positive half-space outside the frutum
		// (and remember view space is left handed,so use the left-hand rule to determine cross produce direction)
		TopEqn = CreatePlaneEquation(TopLeft, TopRight);
		RightEqn = CreatePlaneEquation(TopRight, BottomRight);
		BottomEqn = CreatePlaneEquation(BottomRight, BottomLeft);
		LeftEqn = CreatePlaneEquation(BottomLeft, TopLeft);

	}


	GroupMemoryBarrierWithGroupSync();

	// Calculate the min and max depth for this tile,
	// to form the front and back of the frustum

#if(USE_DEPTH_BOUNDS == 1)||(USE_DEPTH_BOUNDS == 2)
	float maxViewZ = 0.0f;
	float minViewZ = FLT_MAX;
#if(USE_DEPTH_BOUNDS == 1) //non-MSAA
	CalculateMinMaxDepthInLds(globalIndex);
#elif(USE_DEPTH_BOUNDS == 2)
	uint depthBufferWidth, depthBufferHeight, depthBufferNumSamples;
	g_DepthTexture.GetDimensions(depthBufferWidth, depthBufferHeight, depthBufferNumSamples);
	CalculateMinMaxDepthInLdsMSAA(globalIndex, depthBufferNumSamples);
#endif

	GroupMemoryBarrierWithGroupSync();
	maxViewZ = asfloat(ldsMaxViewZ);
	minViewZ = asfloat(ldsMinViewZ);

#endif


	//Loop over the lights and do a sphere vs frustum interdection test
	uint uNumPointLights = g_uNumLights & 0xFFFFu;
	for (uint i = localIndexFlattened; i < uNumPointLights; i += NUM_THREADS_PER_TILE)
	{
		float4 center = g_PointLightBufferCenterAndRadius[i];
		float r = center.w;
		center.xyz = mul(float4(center.xyz, 1), g_mWorldView).xyz;

		//Test if sphere is intersecting or inside frustum
		if (TestFrustumSides(center.xyz, r, TopEqn, RightEqn, BottomEqn, LeftEqn))
		{
#if(USE_DEPTH_BOUNDS != 0)
			if(-center.z + minViewZ < r && center.z - maxViewZ < r)
#else
			if (-center.z < r)
#endif
			{
				// do a thread-safe increment of the list counter
				// adn put the index of this light into the list
				uint dstIndex = 0;
				InterlockedAdd(ldsLightIndexCounter, 1, dstIndex);
				ldsLightIndex[dstIndex] = i;
			}
		}

	}//for

	GroupMemoryBarrierWithGroupSync();

	uint uNumPointLightsInThisTile = ldsLightIndexCounter;

	// and again for spot lights
	uint uNumSpotLights = (g_uNumLights & 0xFFFF0000u) >> 16;
	for (uint j = localIndexFlattened; j < uNumSpotLights; j += NUM_THREADS_PER_TILE)
	{
		float4 center = g_SpotLightBufferCenterAndRadius[j];
		float r = center.w;
		center.xyz = mul(float4(center.xyz, 1), g_mWorldView).xyz;

		// test if sphere is intersecting or inside frustum
		if (TestFrustumSides(center.xyz, r, TopEqn, RightEqn, BottomEqn, LeftEqn))
		{

#if(USE_DEPTH_BOUNDS != 0)
			if (-center.z + minViewZ < r && center.z - maxViewZ < r)
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

	//


	// write back
	{
		uint tileIndexFlattened = groupIndex.x + groupIndex.y*GetNumTilesX();
		uint startOffset = g_uMaxNumLightsPerTile* tileIndexFlattened;


		//point light
		for (uint i = localIndexFlattened; i < uNumPointLightsInThisTile; i += NUM_THREADS_PER_TILE)
		{
			// per-tile list of light indices
			g_PerTileLightIndexBufferOut[startOffset + i] = ldsLightIndex[i];
		}

		for (uint j = (localIndexFlattened + uNumPointLightsInThisTile); j < ldsLightIndexCounter; j += NUM_THREADS_PER_TILE)
		{
			// per-tile list of light indices
			g_PerTileLightIndexBufferOut[startOffset + j + 1] = ldsLightIndex[j];
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