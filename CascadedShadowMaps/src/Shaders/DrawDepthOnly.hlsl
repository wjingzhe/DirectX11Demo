//Globals
cbuffer cbPerObject:register(b0)
{
	matrix g_mWorldViewProject:packoffset(c0);
}


// input/output structures
struct VS_INPUT
{
	float4 vPosition:POSITION;
};
struct VS_OUTPUT
{
	float4 vPosition : SV_POSITION;
};


//Vertex Shader
VS_OUTPUT VSMain(VS_INPUT vin)
{
	VS_OUTPUT vout;

	//There is nothing special here, just tranform and write out the depth.
	vout.vPosition = mul(vin.g_mWorldViewProject);

	return vout;
}

