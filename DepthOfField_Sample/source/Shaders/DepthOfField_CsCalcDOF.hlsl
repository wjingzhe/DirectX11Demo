Texture2D<float> tDepth:register(t0);
Texture2D<float> tCircleOfConfusion:register(t0);
RWTexture2D<float> uCircleOfConfusion:register(u0);
RWTexture2D<float4> uDebugVisCircleOfConfusion:register(u0);


cbuffer CalcDOFParams
{
	uint2 ScreenParams;
	float zNear;
	float zFar;
	float focusDistance;
	float fStop;
	float focalLength;//焦距
	float maxRadius;
	float forceCircleOfConfusion;
};

//场景深度可能是成像点
//focalLength distanceToLense focusDistance这几个变量有什么区别,
float CircleOfConfusionFromDepth(float sceneDepth, float focusDistance, float fStop, float focalLength)
{
	const float circleOfConfusionScale = (focalLength*focalLength) / fStop;// move to canstant buffer
	const float distanceToLense = sceneDepth - focalLength;
	const float distanceToFocusPlane = distanceToLense - focusDistance;
	float circleOfConfusion = (distanceToLense > 0.0f) ? (circleOfConfusionScale*(distanceToFocusPlane / distanceToLense)) : 0.0;

	circleOfConfusion = clamp(circleOfConfusionScale* float(ScreenParams.x)*0.5, -maxRadius, maxRadius);

	return circleOfConfusion;
}

// Compute camera-space depth for current pixel
float CameraDepth(float depth, float zNear, float zFar)
{
	float invRange = 1.0f / (zFar - zNear);

	return (-zFar*zNear)*invRange / (depth - zFar*invRange);
}

[numthreads(8,8,1)]
void CalcDOF(uint3 threadID:SV_DispatchThreadID)
{
	if ((threadID.x < ScreenParams.x) && (threadID.y < ScreenParams.y))
	{
		const float depth = tDepth.Load(int3(threadID.xy, 0), 0);
		const float camDepth = CameraDepth(depth, zNear, zFar);
		float circleOfConfusion = clamp(CircleOfConfusionFromDepth(camDepth, focusDistance, fStop, focalLength), -maxRadius, maxRadius);

		if (abs(forceCircleOfConfusion) > 0.25)
		{
			circleOfConfusion = -forceCircleOfConfusion;
		}

		uCircleOfConfusion[int2(threadID.xy)] = circleOfConfusion;

	}
}

//完全不知道Debug的意图
[numthreads(8,8,1)]
void DebugVisDOF(uint3 Tid:SV_DispatchThreadID)
{
	float4 colorMap[] =
	{
		{0.0,0.0,0.2,1.0f},
		{0.0,0.4,0.2,1.0f},
		{0.4,0.4,0.0,1.0f},
		{1.0,0.2,0.0,1.0f},
		{1.0,0.0,0.0,1.0f},
	};

	const uint2 texCoord = Tid.xy;
	const float CircleOfConfusion = tCircleOfConfusion.Load(int3(texCoord, 0));
	float value = min(
		(abs(min(CircleOfConfusion,float(maxRadius)))-1)/(float(maxRadius-1)/4.0)
		, 3.99);//jingz 没看懂

	int offset = int(floor(value));
	float t = frac(value);
	float4 result = lerp(colorMap[offset], colorMap[offset + 1], t);

	if (abs(CircleOfConfusion) < 1.0f)
	{
		result = float4(0.0, 0.0, 0.1, 1.0f);
	}

	uDebugVisCircleOfConfusion[texCoord] = result;
}
