Texture2D<float> tDepth:register(t0);
Texture2D<float> tCircleOfConfusion:register(t0);//debug
RWTexture2D<float> uCircleOfConfusion:register(u0);
RWTexture2D<float4> uDebugVisCircleOfConfusion:register(u0);


cbuffer CalcDOFParams
{
	uint2 ScreenParams;
	float zNear;
	float zFar;
	float focusDistance;//对焦平面到镜头的距离，俗称对焦距离
	float ratioFocalLengthToLensDiameter;//fStop
	float focalLength;//焦距
	float maxRadius;
	float forceCircleOfConfusion;

	float CircleOfConfusionScale;
	float CircleOfConfusionBias;

};


////focalLength distanceToLense focusDistance这几个变量有什么区别,
//float CircleOfConfusionFromDepth(float sceneDepth, float focusDistance, float ratioFocalLengthToLensDiameter, float focalLength)
//{
//	const float lensDiameter = focalLength / ratioFocalLengthToLensDiameter;
//	const float circleOfConfusionScale = focalLength*lensDiameter / focusDistance;// move to canstant buffer
//	const float objectDepthGreaterThanFocalLength = sceneDepth - focalLength;
//	float circleOfConfusion = (objectDepthGreaterThanFocalLength > 0.0f) ? (circleOfConfusionScale*(sceneDepth - focusDistance) / (sceneDepth - focalLength)) : 0.0;
//
//	circleOfConfusion = clamp(circleOfConfusion* float(ScreenParams.x)*0.5, -maxRadius, maxRadius);
//
//	return circleOfConfusion;
//}


float CircleOfConfusionFromDepth(float sceneDepth, float focusDistance, float ratioFocalLengthToLensDiameter, float focalLength)
{
	float DCoC = sceneDepth * CircleOfConfusionScale + CircleOfConfusionBias;

	DCoC = clamp(DCoC* float(ScreenParams.x)*0.5, -maxRadius, maxRadius);

	return DCoC;
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

		float circleOfConfusion = clamp(CircleOfConfusionFromDepth(camDepth, focusDistance, ratioFocalLengthToLensDiameter, focalLength), -maxRadius, maxRadius);

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
