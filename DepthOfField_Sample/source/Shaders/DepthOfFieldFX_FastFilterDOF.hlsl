//#ifndef DOUBLE_INTEGRATE
//#define DOUBLE_INTEGRATE 1
//#endif

#ifndef CONVERT_TO_SRGB
#define CONVERT_TO_SRGB 1
#endif

#define RWTexToUse RWStructuredBuffer<int4>

Texture2D<float4> TextureColor:register(t0);
Texture2D<float> TextureCircleOfConfusion:register(t1);

sampler PointSampler:register(s0);

RWTexToUse TextureIntermediate : register(u0);
RWTexToUse TextureIntermediate_transposed : register(u1);

RWTexture2D<float4> ResultColor:register(u2);

cbuffer Params:register(b0)
{
	int2 sourceResolution;
	float2 invSourceResolution;
	int2 bufferResolution;
	float scaleFactor;
	int padding;
	int4 bartlettData[9];
	int4 boxBartlettData[4];
};


// Calc offset into buffer from(x,y)
uint GetOffset(int2 addr2d)
{
	return ((addr2d.y*bufferResolution.x) + addr2d.x);
}

//
// Calc offset 
uint GetOffsetTransposed(int2 addr2d)
{
	return ((addr2d.x*bufferResolution.y) + addr2d.y);
}


// Atomic add color to buffer
void InterlockedAddToBuffer(RWTexToUse buffer, int2 addr2d, int4 color)
{
	const int offset = GetOffset(addr2d);

	InterlockedAdd(buffer[offset].r, color.r);
	InterlockedAdd(buffer[offset].g, color.g);
	InterlockedAdd(buffer[offset].b, color.b);
	InterlockedAdd(buffer[offset].a, color.a);

}

// Read result from buffer
int4 ReadFromBuffer(RWTexToUse buffer, int2 addr2d)
{
	const int offset = GetOffset(addr2d);
	int4 result = buffer[offset];
	return result;
}

// write color to buffer
void WriteToBuffer(RWTexToUse buffer, int2 addr2d, int4 color)
{
	const int offset = GetOffset(addr2d);
	buffer[offset] = color;
}

// write color to transposed buffer
void WriteToBufferTransposed(RWTexToUse buffer, int2 addr2d, int4 color)
{
	const int offset = GetOffsetTransposed(addr2d);
	buffer[offset] = color;
}

// Convert Circle of Confusion to blur radius in pixels
int CircleOfConfusionToBlurRadius(const in float fCircleOfField , const in int BlurRadius)
{
	return clamp(abs(int(fCircleOfField)), 0, BlurRadius);
}

//jingz 还是没看懂它的意图
int4 normalizeBlurColor(const in float4 color, const in int BlurRadius)
{
	const float halfWidth = BlurRadius + 1;

	// the weight for the bartlett is half_width^4
	const float weight = (halfWidth * halfWidth*halfWidth*halfWidth);

	float normalization = scaleFactor / weight;

	return int4(round(color*normalization));//jingz 返回最接近的整数
}


//将当前处理的像素以弥散圈为范围，使用权重值增加在其他像素上，弥散圈内的算法可以参照原pdf，下面代码即是实现
void WriteDeltaBartlett(RWTexToUse deltaBuffer, float3 vColor, int BlurRadius, int2 location)
{
	int4 intColor = normalizeBlurColor(float4(vColor, 1.0f), BlurRadius);
	[loop] for (int i = 0; i < 9; ++i)
	{
		const int2 delta = bartlettData[i].xy * (BlurRadius + 1);
		const int deltaValue = bartlettData[i].z;

		// Offset the location by location of the delta and padding
		// Need to offset by(1,1) because the kernel is not centered
		int2 bufLoc = location.xy + delta + padding + uint2(1, 1);

		//Write the delta
		//Use interlocked add to prevent the threads from stepping on each other
		InterlockedAddToBuffer(deltaBuffer, bufLoc, intColor*deltaValue);
	}

}


//Write the bartlett data to the buffer for box filter
void WriteBoxDeltaBartlett(RWTexToUse deltaBuffer, float3 vColor, int BlurRadius, int2 location)
{
	float normalization = scaleFactor / float(BlurRadius * 2 + 1);//jingz
	int4 intColor = int4(round(float4(vColor, 1.0f)*normalization));

	for (int i = 0; i < 4; ++i)
	{
		const int2 delta = boxBartlettData[i].xy*BlurRadius;
		const int2 offset = boxBartlettData[i].xy > int2(0, 0);
		const int deltaValue = boxBartlettData[i].z;

		// Offset the location by location of the delta and padding
		int2 bufLoc = location.xy + delta + padding + offset;

		InterlockedAddToBuffer(deltaBuffer, bufLoc, intColor*deltaValue);

	}
}


[numthreads(8,8,1)]
void FastFilterSetup(uint3 ThreadID:SV_DispatchThreadID)
{
	if (((int)ThreadID.x < sourceResolution.x) && ((int)ThreadID.y < sourceResolution.y))
	{
		//Read the circleOfConfusion the from the coc/Depth buffer
		const float fCircleOfConfusion = TextureCircleOfConfusion.Load(int3(ThreadID.xy, 0));
		const int BlurRadius = CircleOfConfusionToBlurRadius(fCircleOfConfusion, padding);
		float3 vColor = TextureColor.Load(int3(ThreadID.xy, 0)).rgb;

		WriteDeltaBartlett(TextureIntermediate, vColor, BlurRadius, ThreadID.xy);
	}
}


[numthreads(8,8,1)]
void QuaterResFastFilterSetup(uint3 ThreadID:SV_DispatchThreadID)
{
	uint2 location = ThreadID.xy * 2;
	if (int(location.x) < sourceResolution.x && int(location.y) < sourceResolution.y)
	{
		float2 texCoord = (location + 1.0f)*invSourceResolution;

		const int maxRadius = padding;

		const float4 fCircleOfField4 = TextureCircleOfConfusion.Gather(PointSampler, texCoord);

		const float4 focusMask = 2.0f < abs(fCircleOfField4); //jingz ???

		const float weight = dot(focusMask, focusMask);


		const float4 red4 = TextureColor.GatherRed(PointSampler, texCoord);
		const float4 green4 = TextureColor.GatherGreen(PointSampler, texCoord);
		const float4 blue4 = TextureColor.GatherBlue(PointSampler, texCoord);

		if (weight >= 3.9)//jingz ???
		{
			float4 absCircleOfField4 = abs(fCircleOfField4);

			const float fCircleOfField = max(max(absCircleOfField4.x, absCircleOfField4.y), max(absCircleOfField4.z, absCircleOfField4.w));

			const int BlurRadius = CircleOfConfusionToBlurRadius(fCircleOfField, padding);

			float3 vColor;

			vColor.r = dot(red4, focusMask) / weight;
			vColor.g = dot(green4, focusMask) / weight;
			vColor.b = dot(blue4, focusMask) / weight;

			WriteDeltaBartlett(TextureIntermediate, vColor, BlurRadius, location);

		}
		else
		{
			WriteDeltaBartlett(TextureIntermediate, float3(red4.x, green4.x, blue4.x), CircleOfConfusionToBlurRadius(fCircleOfField4.x, padding), location + int2(0, 1));
			WriteDeltaBartlett(TextureIntermediate, float3(red4.y, green4.y, blue4.y), CircleOfConfusionToBlurRadius(fCircleOfField4.x, padding), location + int2(1, 1));
			WriteDeltaBartlett(TextureIntermediate, float3(red4.z, green4.z, blue4.z), CircleOfConfusionToBlurRadius(fCircleOfField4.x, padding), location + int2(1, 0));
			WriteDeltaBartlett(TextureIntermediate, float3(red4.w, green4.w, blue4.w), CircleOfConfusionToBlurRadius(fCircleOfField4.x, padding), location + int2(0, 0));

		}

	}
}


[numthreads(8,8,1)]
void BoxFastFilterSetup(uint3 ThreadID:SV_DispatchThreadID)
{
	if (((int)ThreadID.x < sourceResolution.x) && ((int)ThreadID.y < sourceResolution.y))
	{

		//Read the coc from the coc/depth buffer
		const float fCircleOfConfusion = TextureCircleOfConfusion.Load(int3(ThreadID.xy, 0));
		const int BlurRadius = CircleOfConfusionToBlurRadius(fCircleOfConfusion, padding);
		float3 vColor = TextureColor.Load(int3(ThreadID.xy, 0)).rgb;

		WriteBoxDeltaBartlett(TextureIntermediate, vColor, BlurRadius, ThreadID.xy);
	}
}

// Read and normalize  the results
float4 ReadResult(RWTexToUse buffer, int2 location)
{
	float4 texRead = ReadFromBuffer(buffer, location + padding);

	texRead.rgba /= texRead.a;

	return texRead;
}

[numthreads(64,1,1)]
void VerticalIntegrate(uint3 Tid:SV_DispatchThreadID, uint3 Gid : SV_GroupID)
{
	// To perform double integration in a single step,we must keep two counters delta and color

	if (int(Tid.x) < bufferResolution.x)
	{
		int2 addr = int2(Tid.x, 0);

		//Intialization
		// We want delta and color to be the same
		// their value is just the first element of the domain
		int4 delta = ReadFromBuffer(TextureIntermediate, addr);
		int4 color = delta;


		uint chunkEnd = bufferResolution.y;

		//Actually do the integration
		[loop]
		for (uint i = 1; i < chunkEnd; ++i)
		{
			addr = int2(Tid.x, i);
			// Read from the current integration
			delta += ReadFromBuffer(TextureIntermediate, addr);

#if DOUBLE_INTEGRATE
			//Accumulate to the delta
			color += delta;
#else
			color = delta;
#endif
			//Write the delta integrated value to the output
			WriteToBufferTransposed(TextureIntermediate_transposed, addr, color);

		}
	}
}


float3 LinearToSRGB(float3 linColor)
{
	return pow(abs(linColor), 1.0 / 2.2);
}

[numthreads(8,8,1)]
void ReadFinalResult(uint3 Tid:SV_DispatchThreadID, uint3 Gid : SV_GroupID, uint3 GTid : SV_GroupThreadID)
{
	uint2 texCoord = Tid.xy;

	float4 result = ReadResult(TextureIntermediate, texCoord);

#if CONVERT_TO_SRGB
	result.rgb = LinearToSRGB(result.rgb);
#endif

	ResultColor[texCoord] = float4(result.rgb, 1.0f);
}
