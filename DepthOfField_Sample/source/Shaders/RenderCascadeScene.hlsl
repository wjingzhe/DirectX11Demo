//------------------
// File: RenderCascadeScene.hlsl
//
// This is the main shader file.This shader is compiled with several different flags
// to provide different customizations based on user controls.
// 
// Copyright(c) Microsoft Corporation. All rights reserved

//-----------------------------
// Globals
//-------------------------------


//The number of cascades
#ifndef CASCADE_COUNT_FLAG
#define CASCADE_COUNT_FLAG 3
#endif

//This flag uses the derivate information to map the texels in a shadow map to the
// view space plane of the primitive being rendered. This depth is then used as the 
// comparison depth and reduces self shadowing aliases.This technique is expensive
// and is only valid when objects are planer(such as a ground plane).
#ifndef USE_DERIVATIVES_FOR_DEPTH_OFFSET_FLAG
#define USE_DERIVATIVES_FOR_DEPTH_OFFSET_FLAG 0
#endif

// This flag enables the shadow to blend between cascades. This is most useful when
// the shadow maps are small and artifacts can be seen between the various cascade layers.
#ifndef BLEND_BETWEEN_CASCADE_LAYERS_FLAG
#define BLEND_BETWEEN_CASCADE_LAYERS_FLAG 0
#endif

//There are two methods for selecting the proper cascade a fragment lies in. Interval selection
// compares the depth of the fragment against the frustum's depth partition.
// Map based selection compares the texture coordinates against the actual cascade maps.
// Map based selection gives better coverage.
// Interval based selection is easier to extend and understand
#ifndef SELECT_CASCADE_BY_INTERVAL_FLAG
#define SELECT_CASCADE_BY_INTERVAL_FLAG 0
#endif



// Most tittles will find that 3-4 cascades with 
// BLEND_BETWEEN_CASCADE_LAYERS_FLAG, is good for lower and PCs.
// High end PCs will be able to handle more cascades,and larger blur bands.
// In some cases such as when large PCF kernels are used,derivate based depth offsets could be used
// with larger PCF blur kernels on high end PCs for the ground plane.

cbuffer cbAllShadowData:register(b0)
{
	matrix m_mWorldViewProjection:packoffset(c0);
	matrix m_mWorld:packoffset(c4);
	matrix m_mWorldView:packoffset(c8);
	matrix m_mShadowView:packoffset(c12);
	float4 m_vCascadeOffset[8]:packoffset(c16);
	float4 m_vCascadeScale[8]:packoffset(c24);
	int m_nCascadeLevels : packoffset(c32.x);//Number of Cascades
	int m_iVisualizeCascades : packoffset(c32.y);//1 is to visualize the cascades in different colors. 0 is to just draw the scene
	int m_iPCFBlurForLoopStart : packoffset(c32.z);// For loop begin value.For a 5x5 kernel this would be -2.
	int m_iPCFBlurForLoopEnd : packoffset(c32.w);// For loop end value.For a 5x5 kernel this would be 3
	
	//For Map based selection scheme,this keeps the pixels inside of the valid range.
	// When there is no boarder,these values are 0 and 1 respectively.
	float m_fMinBorderPadding : packoffset(c33.x);
	float m_fMaxBorderPadding : packoffset(c33.y);
	float m_fShadowBiasFromGUI : packoffset(c33.z); // A shadow map offset to deal with self shadow artifacts.// These artifacts are aggravated by PCF.
	float m_fShadowTexPartitionPerLevel : packoffset(c33.w);

	float m_fCascadeBlendArea : packoffset(c34.x);// Amount to overlap when blending between cascades.
	float m_fLogicTexelSizeInX : packoffset(c34.y);
	float m_fCascadedShadowMapTexelSizeInX : packoffset(c34.z);
//	float m_fPaddingForCB3 : packoffset(c34.w);//Padding variables exist because ConstBuffers must be a multiple of 16 bytes.
	
	float3 m_vLightDir : packoffset(c35);

	float4 m_fCascadeFrustumsEyeSpaceDepths[2]: packoffset(c36);//The values along Z that separate the cascades.
	float4 m_fCascadeFrustumsEyeSpaceDepthsFloat4[8]: packoffset(c38);//The values along Z that separate the cascades.
};

//-------------------------------------------
//Textures and Samplers
//---------------------------------------
Texture2D g_txDiffuse:register(t0);
Texture2D g_txShadow:register(t5);

SamplerState g_SamLinear:register(s0);
SamplerComparisonState g_SamShadow:register(s5);

//-----------------------------------------
// Input/Output structures
//-----------------------------------------
struct VS_INPUT
{
	float4 vPositionL:POSITION;
	float3 vNormal:NORMAL;
	float2 vTexCoord:TEXCOORD0;
};

struct VS_OUTPUT
{
	float3 vNormal:NORMAL;
	float2 vTexCoord:TEXCOORD0;
	float4 vPosInShadowView:TEXCOORD1;
	float4 vPosition:SV_POSITION;
	float4 vInterpPos:TEXCOORD2;
	float fDepthInView:TEXCOORD3;
};

//Vertex Shader
VS_OUTPUT VSMain(VS_INPUT Input)
{
	VS_OUTPUT Output;
	
	Output.vPosition  = mul(Input.vPositionL,m_mWorldViewProjection);
	Output.vNormal = mul(Input.vNormal,(float3x3)m_mWorld);
	Output.vTexCoord = Input.vTexCoord;
	Output.vInterpPos = Input.vPositionL;
	Output.fDepthInView = mul(Input.vPositionL,m_mWorldView).z;
	
	//Transform the shadow texture coordinates for all the cascades.
	Output.vPosInShadowView = mul(Input.vPositionL,m_mShadowView);
	return Output;
};


static const float4 vCascadeColorsMultiplier[8] =
{
	float4(1.5f,0.0f,0.0f,1.0f),
	float4(0.0f, 1.5f, 0.0f, 1.0f),
	float4(0.0f, 0.0f, 5.5f, 1.0f),
	float4(1.5f, 0.0f, 5.5f, 1.0f),
	float4(1.5f, 1.5f, 0.0f, 1.0f),
	float4(1.0f, 1.0f, 1.0f, 1.0f),
	float4(0.0f, 1.0f, 5.5f, 1.0f),
	float4(0.5f, 3.5f, 0.75f, 1.0f)
};

/*
（1）注意这里有三个层级，理论上有对应的三张ShadowMap，但是合成了一张ShadowMap,
也就是说如果ShadowMap的分辨率是1024X1024，则以x为平铺方向递增，合成ShadowMap分辨率为3072X1024，
因此可以看到计算ShadowMap采样坐标的代
*/
//jingz 此处为纹理集分解坐标偏移的操作
void ComputeCoordinatesTransform(in int iCascadeIndex, in float4 InterpolatedPosition,
	in out float4 vShadowTexCoord, in out float4 vShadowTexCoordViewSpace)
{
	// Now that we know the correct map,we can transform the world space position of the current fragment
	if(SELECT_CASCADE_BY_INTERVAL_FLAG)
	{
		vShadowTexCoord = vShadowTexCoordViewSpace*m_vCascadeScale[iCascadeIndex];
		vShadowTexCoord += m_vCascadeOffset[iCascadeIndex];
	}
	
	//计算当前x在texture中的int型像素位置
	vShadowTexCoord.x *= m_fShadowTexPartitionPerLevel;// precomputed (float) iCascadeIndex/(float)CASCADE_COUNT_FLAG
	//累加iCascadeIndex 所在的偏移基址：m_fShadowTexPartitionPerLevel*(float)iCascadeIndex
	vShadowTexCoord.x += (m_fShadowTexPartitionPerLevel*(float)iCascadeIndex);
};

//----------------------------
// This function calculates the screen space depth for shadow space texels
// -------
void CalculateRightAndUpTexelDepthDeltas(in float3 vShadowTexDDX, in float3 vShadowTexDDY,
										out float fUpTexDepthWeight,out float fRightTexDepthWeight)
{
	//we use derivatives in X and Y to create a transformation matrix.Because these derivatives give us the 
	// transformation from screen space to shadow space,we need the inverse matrix to take us from shadow space
	// to screen space.This new matrix will allow us to map shadow map texels to screen space. This will allow
	// us to find the screen space depth of a corresponding depth pixel.
	// This is not a perfect solution as it assumes the underlying geometry of the scene is a plane.A more
	// accurate way of finding the actual depth would be to do a deferred rendering approach and actually sample the depth
	
	// Using an offset,or using variance shadow maps is a better approach to reducing these artifacts in most cases.
	
	float2x2 matScreenToShadow = float2x2(vShadowTexDDX.xy,vShadowTexDDY.xy);
	float fDeterminant = determinant(matScreenToShadow);
	
	float fInvDeterminant = 1.0f/fDeterminant;
	
	float2x2 matShadowToScreen = float2x2(
		matScreenToShadow._22 * fInvDeterminant,matScreenToShadow._12*-fInvDeterminant,
		matScreenToShadow._21 * -fInvDeterminant,matScreenToShadow._11*fInvDeterminant); 
	
	float2 vRightShadowTexelLocation = float2(m_fLogicTexelSizeInX,0.0f);
	float2 vUpShadowTexelLocation = float2(0.0f,m_fLogicTexelSizeInX);
	
	//Transform the right pixel by the shadow space to screen matrix
	float2 vRightTexelDepthRatio = mul(vRightShadowTexelLocation,matShadowToScreen);
	float2 vUpTexelDepthRatio = mul(vUpShadowTexelLocation,matShadowToScreen);
	
	//We can now calculate how much depth changes when you move up or right in the shadow map.
	// We use the ratio of change in x and y times the derivative in X and Y of the screen space
	// depth to calculate this change
	fUpTexDepthWeight = vUpTexelDepthRatio.x*vShadowTexDDX.z+vUpTexelDepthRatio.y*vShadowTexDDY.z;
	fRightTexDepthWeight = vRightTexelDepthRatio.x* vShadowTexDDX.z + vRightTexelDepthRatio.y*vShadowTexDDY.z;
	
	
};


//
// Use PCF to sample the depth map and return a percent lit value
//
void CalculatePCFPercentLit(in float4 vShadowTexCoord,in float fRightTexelDepthDelta,in float fUpTexelDepthDelta,
						in float fBlurRowSize,out float fPercentLit)
{
	fPercentLit = 0.0f;//jingz 阴影系数，计算满足阴影深度值的点累计平均的系数，最后用来做全阴影和全光照之间插值的系数

	//This loop could be unrolled,and texture immediate offsets could be used if the kernel size were fixed/
	// This would be performance improvement.
	for(int x = m_iPCFBlurForLoopStart;x<m_iPCFBlurForLoopEnd;++x)
	{
		for(int y = m_iPCFBlurForLoopStart;y<m_iPCFBlurForLoopEnd;++y)
		{
			float depthCompare = vShadowTexCoord.z;
		
			// A very simple solution to the depth bias problems of PCF is to use an offset.
			// Unfortunately,too much offset can lead to Peter-panning(Shadows near the base of object disappear)
			// Too little offset can lead to shadow acne(objects that should not be in shadow are partially self shadowed).
			depthCompare -= m_fShadowBiasFromGUI;
			
			if(USE_DERIVATIVES_FOR_DEPTH_OFFSET_FLAG)
			{
				//Add in derivative computed depth scale based on the x and y pixel/
				depthCompare += fRightTexelDepthDelta * ((float)x) + fUpTexelDepthDelta * ((float)y);
			}
			// Compare the transformed pixel depth to the depth read from the map.
			fPercentLit += g_txShadow.SampleCmpLevelZero(g_SamShadow,
			float2(vShadowTexCoord.x + ((float)x)*m_fCascadedShadowMapTexelSizeInX,
					vShadowTexCoord.y + ((float)y)*m_fLogicTexelSizeInX),
			depthCompare);
		}
		
	}//for
	
	fPercentLit /= (float)fBlurRowSize;

}

//
// Calculate amount to blend between two cascades and band where blending will occure.
//
void CalculateBlendAmountForInterval(in int iCurrentCascadeIndex,
in out float fPixelDepth,in out float fCurrentPixelsBlendBandLocation,
out float fBlendBetweenCascadeAmount)
{
	//We need to calculate the band of the current shadow map where it will be fade into the next cascade.
	// We can then early out of the expensive PCF for loop
	//
	float fBlendInterval = m_fCascadeFrustumsEyeSpaceDepthsFloat4[iCurrentCascadeIndex].x;
	//if(iNextCascadeIndex>1)
	int fBlendIntervalBelowIndex = min(0,iCurrentCascadeIndex-1);
	fPixelDepth -= m_fCascadeFrustumsEyeSpaceDepthsFloat4[fBlendIntervalBelowIndex].x;
	fBlendInterval -= m_fCascadeFrustumsEyeSpaceDepthsFloat4[fBlendIntervalBelowIndex].x;
	
	// The current pixel's blend band location will be used to determine when we need to blend and by how much.
	fCurrentPixelsBlendBandLocation = fPixelDepth / fBlendInterval;
	fCurrentPixelsBlendBandLocation = 1.0f - fCurrentPixelsBlendBandLocation;
	// The fBlendBetweenCascadeAmount is our location in the blend band.
	fBlendBetweenCascadeAmount = fCurrentPixelsBlendBandLocation/m_fCascadeBlendArea;
}

//------------------------
// Calculate amount to blend between two cascades and the band where blending will occure.
//--------------------------
void CalculateBlendAmountForMap(in float4 vShadowMapTexCoord,in out float fCurrentPixelsBlendBandLocation,out float fBlendBetweenCascadeAmount)
{
	//Calculate the blend band for the map based selection.
	float2 distanceToOne = float2(1.0f-vShadowMapTexCoord.x,1.0f-vShadowMapTexCoord.y);
	fCurrentPixelsBlendBandLocation = min(vShadowMapTexCoord.x,vShadowMapTexCoord.y);
	float fCurrentPixelsBlendBandLocation2 = min(distanceToOne.x,distanceToOne.y);
	fCurrentPixelsBlendBandLocation = min(fCurrentPixelsBlendBandLocation,fCurrentPixelsBlendBandLocation2);
	fBlendBetweenCascadeAmount = fCurrentPixelsBlendBandLocation/m_fCascadeBlendArea;
}

//---------------------------------
// Calculate the Shadow based on several options and render the scene
//------------------------------------------------
float4 PSMain(VS_OUTPUT Input):SV_TARGET
{
	float4 vDiffuse = g_txDiffuse.Sample(g_SamLinear,Input.vTexCoord);
	
	float4 vShadowMapTexCoord = float4(0.0f,0.0f,0.0f,0.0f);
	float4 vShadowMapTexCoord_Blend = float4(0.0f,0.f,0.f,0.f);
	
	float4 vVisualizeCascadeColor = float4(0.0f,0.0f,0.0f,1.0f);
	
	float fPercentLit = 0.0f;
	float fPercentLit_Blend = 0.0f;
	
	float fUpTexDepthWeight = 0;
	float fRightTexDepthWeight = 0;
	float fUpTexDepthWeight_Blend = 0;
	float fRightTexDepthWeight_Blend = 0;
	
	int iBlurRowSize = m_iPCFBlurForLoopEnd-m_iPCFBlurForLoopStart;

	float fBlurRowSize = (float)(iBlurRowSize*iBlurRowSize);
	
	int iCascadeFound = 0;
	int iNextCascadeIndex = 1;
	
	float fCurrentPixelDepthInView;
	
	//The interval based selection technique compares the pixel's depth against the frustum's cascade divisions.
	fCurrentPixelDepthInView = Input.fDepthInView;
	
	// This for loop is not necessary when the frustum is uniformly divided and interval based selection is used.
	// In this case fCurrentPixelDepthInView could be used as an array lookup into the correct frustum.
	int iCurrentCascadeIndex;
	
	float4 vPosInShadowView = Input.vPosInShadowView;
	if (SELECT_CASCADE_BY_INTERVAL_FLAG)//jingz
	{
		iCurrentCascadeIndex = 0;
		if(CASCADE_COUNT_FLAG > 1)
		{
			float4 vCurrentPixelDepth = float4(Input.fDepthInView, Input.fDepthInView, Input.fDepthInView, Input.fDepthInView);

			float4 fComparison = (vCurrentPixelDepth>m_fCascadeFrustumsEyeSpaceDepths[0]);
			float4 fComparison2 = (vCurrentPixelDepth > m_fCascadeFrustumsEyeSpaceDepths[1]);
			float fIndex = dot(
							float4(CASCADE_COUNT_FLAG>0,CASCADE_COUNT_FLAG>1,CASCADE_COUNT_FLAG>2,CASCADE_COUNT_FLAG>3),
							fComparison)
							+dot(
							float4(CASCADE_COUNT_FLAG > 4,CASCADE_COUNT_FLAG > 5,CASCADE_COUNT_FLAG>6,CASCADE_COUNT_FLAG>7),
							fComparison2
							);
							
			fIndex = min(fIndex,CASCADE_COUNT_FLAG-1);
			iCurrentCascadeIndex = (int)fIndex;
		}
	}

	if(!SELECT_CASCADE_BY_INTERVAL_FLAG)
	{
		iCurrentCascadeIndex = 0;
		if(CASCADE_COUNT_FLAG == 1)
		{
			vShadowMapTexCoord = vPosInShadowView * m_vCascadeScale[0];
			vShadowMapTexCoord += m_vCascadeOffset[0];
		}
		if(CASCADE_COUNT_FLAG > 1)
		{
			for(int iCascadeIndex = 0;iCascadeIndex<CASCADE_COUNT_FLAG&& iCascadeFound ==0;++iCascadeIndex)
			{
				vShadowMapTexCoord = vPosInShadowView * m_vCascadeScale[iCascadeIndex];
				vShadowMapTexCoord += m_vCascadeOffset[iCascadeIndex];
				
				if( min(vShadowMapTexCoord.x,vShadowMapTexCoord.y) > m_fMinBorderPadding
					&& max(vShadowMapTexCoord.x,vShadowMapTexCoord.y) < m_fMaxBorderPadding)
				{
					iCurrentCascadeIndex = iCascadeIndex;//jingz 最先找到的满足于层级xy裁剪的下标
					iCascadeFound = 1;
				}
			}
		}
		
	}//if_!SELECT_CASCADE_BY_INTERVAL_FLAG
	
	float4 color = 0;
	
	if(BLEND_BETWEEN_CASCADE_LAYERS_FLAG)
	{
		//Repeat texture coordinate calculations for the next cascade
		// the next cascade index is used for blurring between maps.
		iNextCascadeIndex = min(CASCADE_COUNT_FLAG - 1,iCurrentCascadeIndex+1);
	}
	
	float fBlendBetweenCascadeAmount = 1.0f;
	float fCurrentPixelsBlendBandLocation=1.0f;
	
	if(SELECT_CASCADE_BY_INTERVAL_FLAG)
	{
		if(BLEND_BETWEEN_CASCADE_LAYERS_FLAG && CASCADE_COUNT_FLAG > 1)
		{
			CalculateBlendAmountForInterval(iCurrentCascadeIndex,fCurrentPixelDepthInView,fCurrentPixelsBlendBandLocation,fBlendBetweenCascadeAmount);
		}
	}
	else
	{
		if(BLEND_BETWEEN_CASCADE_LAYERS_FLAG)
		{
			CalculateBlendAmountForMap(vShadowMapTexCoord,fCurrentPixelsBlendBandLocation,fBlendBetweenCascadeAmount);
		}
	}
	
	float3 vShadowMapTexCoordDDX;
	float3 vShadowMapTexCoordDDY;
	// The derivative are used to find the slope of the current plane
	// The derivative calculation has to be inside of the loop in order to prevent divergent flow control artifacts.
	if(USE_DERIVATIVES_FOR_DEPTH_OFFSET_FLAG)
	{
		vShadowMapTexCoordDDX = ddx(vPosInShadowView);
		vShadowMapTexCoordDDY = ddy(vPosInShadowView);
		
		vShadowMapTexCoordDDX *= m_vCascadeScale[iCurrentCascadeIndex];
		vShadowMapTexCoordDDY *= m_vCascadeScale[iCurrentCascadeIndex];
	}
	
	ComputeCoordinatesTransform(iCurrentCascadeIndex,Input.vInterpPos,vShadowMapTexCoord,vPosInShadowView);
	
	vVisualizeCascadeColor = vCascadeColorsMultiplier[iCurrentCascadeIndex];
	
	if(USE_DERIVATIVES_FOR_DEPTH_OFFSET_FLAG)
	{
		CalculateRightAndUpTexelDepthDeltas(vShadowMapTexCoordDDX,vShadowMapTexCoordDDY,fUpTexDepthWeight,fBlurRowSize);
	}
	
	CalculatePCFPercentLit(vShadowMapTexCoord,fRightTexDepthWeight,fUpTexDepthWeight,fBlurRowSize,fPercentLit);
	
	if(BLEND_BETWEEN_CASCADE_LAYERS_FLAG && CASCADE_COUNT_FLAG > 1)
	{
		if(fCurrentPixelsBlendBandLocation < m_fCascadeBlendArea)
		{
			//the current pixel is within the blend band.
			
			//Repeat Texture coordinate calculations for the next cascade.
			// The next cascade index is used for blurring between maps.
			if(!SELECT_CASCADE_BY_INTERVAL_FLAG)
			{
				vShadowMapTexCoord_Blend = vPosInShadowView * m_vCascadeScale[iNextCascadeIndex];
				vShadowMapTexCoord_Blend += m_vCascadeOffset[iNextCascadeIndex];
			}
			
			ComputeCoordinatesTransform(iNextCascadeIndex,Input.vInterpPos,vShadowMapTexCoord_Blend,vPosInShadowView);
			
			// We repeat the calculation for the next cascade layer,when blending between maps.
			if(fCurrentPixelsBlendBandLocation < m_fCascadeBlendArea)
			{
				// The current pixel is within the blend band.
				if(USE_DERIVATIVES_FOR_DEPTH_OFFSET_FLAG)
				{
					CalculateRightAndUpTexelDepthDeltas(vShadowMapTexCoordDDX,vShadowMapTexCoordDDY,fUpTexDepthWeight_Blend,fRightTexDepthWeight_Blend);
				}
				
				CalculatePCFPercentLit(vShadowMapTexCoord_Blend,fRightTexDepthWeight_Blend,fUpTexDepthWeight_Blend,fBlurRowSize,fPercentLit_Blend);
				// Blend the two calculated shadows bu the blend amount
				fPercentLit = lerp(fPercentLit_Blend,fPercentLit,fBlendBetweenCascadeAmount);
				
			}
			
		}
	}
	
	if(!m_iVisualizeCascades)
	{
		vVisualizeCascadeColor = float4(1.0f,1.0f,1.0f,1.0f);
	}
	
	float3 vLightDir1 = float3(-1.0f,1.0f,-1.0f);
	float3 vLightDir2 = float3(1.0f,1.0f,-1.0f);
	float3 vLightDir3 = float3(0.0f,-1.0f,0.0f);
	float3 vLightDir4 = float3(1.0f,1.0f,1.0f);
	
	// Some ambient-like lighting.
	float fLighting = 
		saturate( dot(vLightDir1,Input.vNormal))*0.05f+
		saturate( dot(vLightDir2,Input.vNormal))*0.05f+
		saturate( dot(vLightDir3,Input.vNormal))*0.05f+
		saturate( dot(vLightDir4,Input.vNormal))*0.05f;
	
	
	float4 vShadowLighting = fLighting * 0.5f;
	fLighting += saturate(dot(m_vLightDir,Input.vNormal));
	fLighting = lerp(vShadowLighting,fLighting,fPercentLit);
	
	return fLighting* vVisualizeCascadeColor*vDiffuse;
	
};


