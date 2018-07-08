
//This flag uses the derivative information to map the texels in a shadow map to the view space plane of
// the primitive being rendered.This depth is then used as the comparison depth and reduces self shadowing aliases.
// This technique is expensive and is only valid when objects are planer(such as a ground plane).
#ifndef USE_DERIVATIVES_FOR_DEPTH_OFFSET_FLAG
#define USE_DERIVATIVES_FOR_DEPTH_OFFSET_FLAG 0
#endif

// Thera are two enables for selecting to blend between cascades.This is most useful when the
// the shadow maps are small and artifact can be seen between the various cascade layers.
#ifndef BLEND_BETWEEN_CASCADE_LAYERS_FLAG
#define BLEND_BETWEEN_CASCADE_LAYERS_FLAG 0
#endif

// There are two methods for selecting the proper cascade a fragment lies in. Interval selection
// compares the depth of the fragment against the frustum's depth partition.
// Map based selection compares the texture coordinates against the acutal cascade maps.
// Map based selection gives better coverage.
// Interval based selection is easier to extend and understand.
#ifndef SELECT_CASCADE_BY_INTERVAL_FLAG
#define SELECT_CASCADE_BY_INTERVAL_FLAG 0
#endif


//  The number of cascades
#ifndef CASCADE_COUNT_FLAG
#define CASCADE_COUNT_FLAG 3
#endif

//PCF Percentage-Closer-Filtering

//Most titles will find that 3-4 cascades with BLEND_BETWEEN_CASCADE_LAYERS_FLAG,
// is good for lower end PCs. High end PCs will be able to handle more cascades,and larger blur bands.
//In some cases such as when large PCF kernels are used ,derivative based depth offsets could be used
// with large PCF blur kernels on high end PCs for the ground plane.

cbuffer cbAllShadowData:register(b0)
{
	matrix m_mWorldViewProjection;
	matrix m_mWorld;
	matrix m_mWorldView;
	matrix m_mShadow;
	float4 m_vCascadeOffset[8];
	float4 m_vCascadeScale[8];
	int m_nCascadeLevels;// Number of Cascades
	int m_iVisualizeCascades;// 1 is to visualize the cascades in different colors.O is  to just draw the scene
	int m_iPCFBlurForLoopStart; // For loop begin value,for a 5x5 Kernel this world be -2
	int m_iPCFBlurForLoopEnd; // For lopp end value.For a 5x5 kernel this would be 3.

	// For Map based selection scheme,this keeps the pixels of the valid range.
	// When there is no boarder, these values are 0 and 1 respectively.
	float m_fMinBorderPadding;
	float m_fMaxBorderPadding;
	float m_fShadowBiasOffset; // A shadow map offset to deal with self shadow artifacts.

	float m_fShadowPartionSize;
	float m_fCascadeBlendArea; //Amount to overlap when blending between cascades
	float m_fTexelSize;
	float m_fNativeTexelSizeInx;
	float m_fPaddingForCB3;// Padding variables exist because CBs must be a multiple of 16 bytes.
	float m_fCascadeFrustumEyeSpaceDepthFloat[2]; // The values along Z that separate the cascades.
	float m_fCascadeFrumtumEysSpaceDepthFloat4[8]; // the values along Z that separate the cascades. // Wastefully stored in float4 so they are array indexable

	float3 m_vLightDir;
	float m_fPaddingCB4;


};


Texture2D g_txDiffuse:register(t0);
Texture2D g_txShadow:register(t5);


SamplerState g_samLinear : register(s0);
SamplerState g_SamShadow:register(s5);

// input/output structures
struct VS_INPUT
{
	float4 vPosition : POSITION;
	float3 vNormal ：NORMAL;
	float2 vTexcoord:TEXCOORD0;
};

struct VS_OUTPUT
{
	float4 vPosition:SV_POSITION;
	float3 vNormal :NORMAL;
	float2 vTexcoord:TEXCOORD0;
	float4 vTexShadow:TEXCOORD1;
	float4 vInterpPos:TEXCOORD2;
	float vDepth : TEXCOORD3;
};

// Vertex Shader
VS_OUTPUT VSMain(VS_INPUT vin)
{
	VS_OUTPUT vout;

	vout.vPosition = mul(vin.vPosition, m_mWorldViewProjection);
	vout.vNormal = mul(vin.vNormal, (float3x3)m_mWorld);//jingz There are without scaling;
	vout.vTexcoord = vin.vTexcoord;
	vout.vInterpPos = vin.vPosition;
	vout.vDepth = mul(vin.vPosition, m_mWorldView).z;

	// Transform the shadow texture coordinates for all the cascades.
	vout.vTexShadow = mul(vin.vPosition, m_mShadow);
	return vout;

}


//jingz 这些是怎么用
static const float4 vCascadeColorMultiplier[8] =
{
	float4(1.5f,0.0f,0.0f,1.0f),
	float4(0.0f,1.5f,0.0f,1.0f),
	float4(0.0f,0.0f,5.5f,1.0f),
	float4(1.5f,0.0f,5.5f,1.0f),


	float4(1.5f,1.5f,0.0f,1.0f),
	float4(1.0f,1.0f,1.0f,1.0f),
	float4(0.0f,1.0f,5.5f,1.0f),
	float4(0.5f,3.0f,0.75f,1.0f)

};


void ComputeCoordinateTransform(in int iCascadeIndex,
	in float4 InterpolatedPosition,
	in out float4 vShadowTexCoord,
	int out float4 vShadowTexCoordViewSpace)
{
	// Now that we know the correct map,we can transform the world space position of the current fragment
	if (SELECT_CASCADE_BY_INTERVAL_FLAG)
	{
		vShadowTexCoord = vShadowTexCoordViewSpace * m_vCascadeScale[iCascadeIndex];
		vShadowTexCoord += m_vCascadeOffset[iCascadeIndex];
	}

	vShadowTexCoord.x *= m_fShadowPartitionSize; // Precomputed (float)iCascadeIndex / (float)CASCADE_CNT
	vShadowTexCoord.x += (m_fShadowPartitionSize * (float)iCascadeIndex);

}

// This function calculates the screen space depth for shadow space texels
void CalculateRightAndUpTexelDepthDeltas(in float3 vShadowTexDDX, in float3 vShadowTexDDY,
	out float fUpTexDepthWeight, out float fRightTextDepthWeight)
{
	// We use the derivatives in X and Y to create a transformation matrix.Because these derivives give us the
	// transformation from screen space tp shadow space,we need to inverse matrix to take us from shadow space
	// to screen space. This new matrix will allow us to map shadow map texels to screen space.This will allow
	// us to find the screen space depth of a corresponding depth pixel.
	// This is not a perfect solution as it assumes the underlying geometry of the scene is a plane.A more
	// accureate way of finding the actual depth would be to do a defferred rendering approach and actually 
	// sample the depth;

	// Using an offset,or using variance shadow maps is better approach to reducing these artificts in most cases

	float2x2 matScreenToScreen = float2x2(vShadowTexDDX.xy,vShadowTexDDY.xy);
	float fDeterminant = determinant(matScreenToScreen);
	float fInvDeterminant = 1.0f / fDeterminant;

	float2x2 matShadowToScreen = float2x2(matScreenToShadow._22* fInvDeterminant, matScreenToShadow._12*-fInvDeterminant,
		matScreenToShadow._21*-fInvDeterminant, matScreenToShadow._11*fInvDeterminant);

	float2 vRightShadowTexelLocation = float2(m_fTexelSize, 0.0f);
	float2 vUpShadowTexelLocation = float2(0.0f, m_fTexelSize);

	//Transform the right pixel by the shadow space to screen space matrix.
	float2 vRightTexelDepthRatio = mul(vRightShadowTexelLocation, matShadowToScreen);
	float2 vUpTexelDepthRatio = mul(vUpShadowTexelLocation, matShadowToScreen);

	// We can now caculate how much depth changes when you move up or right in the shadow map/
	// we use the ratio of change in x and y times the derivite in X and Y of the screen space
	// depth to calculate this change
	fUpTextDepthWeight = vUpTexelDepthRatio, x * vShadowTexDDX.z + vUpTexelDeothRatio.y * vShadowTexDDY.z;
	fRightTextDepthWeight =
		vRightTexelDepthRatio.x * vShadowTexDDX.z
		+ vRightTexelDepthRatio.y * vShadowTexDDY.z;
}