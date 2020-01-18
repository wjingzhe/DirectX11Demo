#include "GeometryHelper.h"
#include <utility>
#include <algorithm>
using namespace DirectX;



GeometryHelper::MeshData GeometryHelper::CreateBox(float width, float height, float depth, uint32 numSubdivision,float centerX, float centerY, float centerZ)
{
	MeshData meshData;

	Vertex v[24];
	float w2 = 0.5f*width;
	float h2 = 0.5f*height;
	float d2 = 0.5f*depth;

	//jingz how is the tangentUVW defined
	// begin on left,clock counter


	//Fill int the front face vertex face
	v[0] = Vertex(-w2 + centerX, -h2+ centerY, -d2+ centerZ, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f);
	v[1] = Vertex(-w2 + centerX, +h2+ centerY, -d2+ centerZ, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
	v[2] = Vertex(+w2 + centerX, +h2+ centerY, -d2+ centerZ, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f);
	v[3] = Vertex(+w2 + centerX, -h2+ centerY, -d2+ centerZ, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f);

	//Fill in th back face vertex data.
	v[4] = Vertex(+w2 + centerX, -h2+ centerY, +d2+ centerZ, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f);
	v[5] = Vertex(+w2 + centerX, +h2+ centerY, +d2+ centerZ, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f);
	v[6] = Vertex(-w2 + centerX, +h2+ centerY, +d2+ centerZ, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f);
	v[7] = Vertex(-w2 + centerX, -h2+ centerY, +d2+ centerZ, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f);

	//Fill in the top face vertex data
	v[8] = Vertex(-w2 + centerX, +h2+ centerY, -d2+ centerZ, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f);
	v[9] = Vertex(-w2 + centerX, +h2+ centerY, +d2+ centerZ, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
	v[10] = Vertex(+w2+ centerX, +h2+ centerY, +d2+ centerZ, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f);
	v[11] = Vertex(+w2+ centerX, +h2+ centerY, -d2+ centerZ, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f);

	//Fill in bottom face vertex data
	v[12] = Vertex(+w2+ centerX, -h2+ centerY, -d2+ centerZ, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f);
	v[13] = Vertex(+w2+ centerX, -h2+ centerY, +d2+ centerZ, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f);
	v[14] = Vertex(-w2+ centerX, -h2+ centerY, +d2+ centerZ, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f);
	v[15] = Vertex(-w2+ centerX, -h2+ centerY, -d2+ centerZ, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f);

	//Fill in left  face vertex data
	v[16] = Vertex(-w2+ centerX, -h2+ centerY, +d2+ centerZ, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, -1.0f);
	v[17] = Vertex(-w2+ centerX, +h2+ centerY, +d2+ centerZ, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f);
	v[18] = Vertex(-w2+ centerX, +h2+ centerY, -d2+ centerZ, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f);
	v[19] = Vertex(-w2+ centerX, -h2+ centerY, -d2+ centerZ, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, -1.0f);

	//Fill in right face vertex data
	v[20] = Vertex(+w2+ centerX, -h2+ centerY, -d2+ centerZ, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f);
	v[21] = Vertex(+w2+ centerX, +h2+ centerY, -d2+ centerZ, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f);
	v[22] = Vertex(+w2+ centerX, +h2+ centerY, +d2+ centerZ, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f);
	v[23] = Vertex(+w2+ centerX, -h2+ centerY, +d2+ centerZ, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f);


	meshData.Vertices.assign(&v[0], &v[24]);

	//
	// Create the indices
	// 

	uint32 i[36];

	
	// Fill in the front face index data
	i[0] = 0; i[1] = 1; i[2] = 2;
	i[3] = 0; i[4] = 2; i[5] = 3;

	// Fill in the back face index data
	i[6] = 4; i[7] = 5; i[8] = 6;
	i[9] = 4; i[10] = 6; i[11] = 7;

	// Fill in the top face index data
	i[12] = 8; i[13] = 9; i[14] = 10;
	i[15] = 8; i[16] = 10; i[17] = 11;

	// Fill in the bottom face index data
	i[18] = 12; i[19] = 13; i[20] = 14;
	i[21] = 12; i[22] = 14; i[23] = 15;

	// Fill in the left face index data
	i[24] = 16; i[25] = 17; i[26] = 18;
	i[27] = 16; i[28] = 18; i[29] = 19;

	// Fill in the right face index data
	i[30] = 20; i[31] = 21; i[32] = 22;
	i[33] = 20; i[34] = 22; i[35] = 23;

	meshData.Indices32.assign(&i[0], &i[36]);

	//Put a cap on the number of subdivisions
	numSubdivision = std::min<uint32>(numSubdivision, 6u);

	return meshData;
}

GeometryHelper::MeshData GeometryHelper::CreateScreenQuad()
{
	MeshData meshData;

	Vertex v[4];

	//Fill int the front face vertex face
	v[0] = Vertex(-1.0f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
	v[1] = Vertex(1.0f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f);
	v[2] = Vertex(1.0f, -1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f);
	v[3] = Vertex(-1.0f, -1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f);

	meshData.Vertices.assign(&v[0], &v[4]);

	uint32 i[6];
	// Fill in the front face index data
	i[0] = 0; i[1] = 1; i[2] = 2;
	i[3] = 0; i[4] = 2; i[5] = 3;

	meshData.Indices32.assign(&i[0], &i[6]);

	return meshData;
}

GeometryHelper::MeshData GeometryHelper::CreateSphere(float radius, uint32 sliceCount, uint32 stackCount, XMFLOAT3 center)
{
	GeometryHelper::MeshData meshData;
	meshData.Vertices.clear();
	meshData.Indices32.clear();


	//Ô²ÐÄÊÇlocal £¨0,0,0£©

	//
	// Compute the vertices stating at the top pole and moving down the stacks.
	//

	// Poles: note that there will be texture coordinate distortion as there is
	// not a unique point on the texture map to assign to the pole when mapping
	// a rectangular texture onto a sphere.
	Vertex topVertex(center.x, center.y + radius, center.z, 0.0f, +1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	Vertex bottomVertex(center.x, center.y - radius, center.z, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);

	meshData.Vertices.push_back(topVertex);

	float phiStep = XM_PI / stackCount;
	float thetaStep = 2.0f*XM_PI / sliceCount;

	//Compute vertices for each stack ring(do not count the poles as rings).

	for (uint32 i = 1; i < stackCount; ++i)
	{
		float phi = i*phiStep;

		for (uint32 j = 0; j <= sliceCount; ++j)
		{
			float theta = j*thetaStep;

			Vertex v;

			v.Position.x = radius *sinf(phi) * cosf(theta) + center.x;
			v.Position.y = radius *cosf(phi) + center.y;
			v.Position.z = radius *sinf(phi) * sinf(theta) + center.z;

			// Partial derivative of P with respect to theta
			float tX = -radius*sinf(phi)*sinf(theta);
			float tY = 0.0f;
			float tZ = +radius*sinf(phi)*cosf(theta);
			XMVECTOR T = XMLoadFloat3(&XMFLOAT3(tX, tY, tZ));
			XMStoreFloat3(&v.TangentUVW, XMVector3Normalize(T));


			XMVECTOR p = XMLoadFloat3(&v.Position);
			XMStoreFloat3(&v.Normal, XMVector3Normalize(p));

			v.TextureUV.x = theta / XM_2PI;
			v.TextureUV.y = phi / XM_PI;

			meshData.Vertices.push_back(v);
		}
	}
	meshData.Vertices.push_back(bottomVertex);


	//
	// Compute indices for top stack.  The top stack was written first to the vertex buffer
	// and connects the top pole to the first ring.
	//

	for (uint32 i = 1; i <= sliceCount; ++i)
	{
		meshData.Indices32.push_back(0);
		meshData.Indices32.push_back(i + 1);
		meshData.Indices32.push_back(i);
	}

	// Offset the indices to the index of the first vertex in the first ring.
	// This is just skipping the top pole vertex.
	uint32 baseIndex = 1;
	uint32 ringVertexCount = sliceCount + 1;
	for (uint32 i = 0; i < stackCount - 2; ++i)
	{
		for (int j = 0; j < sliceCount; ++j)
		{
			meshData.Indices32.push_back(baseIndex + i*ringVertexCount + j);
			meshData.Indices32.push_back(baseIndex + i*ringVertexCount + j + 1);
			meshData.Indices32.push_back(baseIndex + (i + 1)*ringVertexCount + j);

			meshData.Indices32.push_back(baseIndex + (i + 1)*ringVertexCount + j);
			meshData.Indices32.push_back(baseIndex + i*ringVertexCount + j + 1);
			meshData.Indices32.push_back(baseIndex + (i + 1)*ringVertexCount + j + 1);
		}
	}

	//
	// Compute indices for bottom stack.  The bottom stack was written last to the vertex buffer
	// and connects the bottom pole to the bottom ring.
	//

	// South pole vertex was added last.
	uint32 southPoleIndex = (uint32)meshData.Vertices.size() - 1;

	// Offset the indices to the index of the first vertex in the last ring.
	baseIndex = southPoleIndex - ringVertexCount;

	for (uint32 i = 0; i < sliceCount; ++i)
	{
		meshData.Indices32.push_back(southPoleIndex);
		meshData.Indices32.push_back(baseIndex + i);
		meshData.Indices32.push_back(baseIndex + i + 1);
	}

	return meshData;
}

GeometryHelper::MeshData GeometryHelper::CreateCubePlane(float width, float height, float depth)
{
	GeometryHelper::MeshData meshData;

	float halfWidth = width*0.5f;
	float halfHeight = height*0.5f;
	float halfDepth = depth*0.5f;

	//Vertex v[4];
	//// left plane 
	//v[0] = Vertex(/*Position*/ 1.0f*halfWidth, -1.0f*halfHeight, 1.0f*halfDepth, /* Normal */-1.0f, 0.0f, 0.0f, /* TextureUV */0.0f, 1.0f,/*TangentUVW*/ 1.0f, 0.0f, 0.0f);// bottom-left
	//v[1] = Vertex(/*Position*/1.0f*halfWidth, -1.0f*halfHeight, -1.0f*halfDepth, /* Normal */-1.0f, 0.0f, 0.0f, /* TextureUV */1.0f, 1.0f,/*TangentUVW*/ 1.0f, 0.0f, 0.0f);// bottom-right
	//v[2] = Vertex(/*Position*/ 1.0f*halfWidth, 1.0f*halfHeight, 1.0f*halfDepth, /* Normal */-1.0f, 0.0f, 0.0f, /* TextureUV */0.0f, 0.0f,/*TangentUVW*/ 1.0f, 0.0f, 0.0f);// top-left
	//v[3] = Vertex(/*Position*/1.0f*halfWidth, 1.0f*halfHeight, -1.0f*halfDepth, /* Normal */-1.0f, 0.0f, 0.0f, /* TextureUV */1.0f, 0.0f,/*TangentUVW*/ 1.0f, 0.0f, 0.0f);// top-right
	//meshData.Vertices.assign(&v[0], &v[4]);
	//uint32 i[6];
	//// Fill in the front face index data
	//i[0] = 0; i[1] = 2; i[2] = 3;
	//i[3] = 0; i[4] = 3; i[5] = 1;
	//meshData.Indices32.assign(&i[0], &i[6]);


	Vertex v[24];
	
	 // right plane 
	v[0] = Vertex(/*Position*/ 1.0f*halfWidth, -1.0f*halfHeight, 1.0f*halfDepth, /* Normal */-1.0f, 0.0f, 0.0f, /* TextureUV */0.0f, 1.0f,/*TangentUVW*/ 1.0f, 0.0f, 0.0f);// bottom-left
	v[1] = Vertex(/*Position*/1.0f*halfWidth, -1.0f*halfHeight, -1.0f*halfDepth, /* Normal */-1.0f, 0.0f, 0.0f, /* TextureUV */1.0f, 1.0f,/*TangentUVW*/ 1.0f, 0.0f, 0.0f);// bottom-right
	v[2] = Vertex(/*Position*/ 1.0f*halfWidth, 1.0f*halfHeight, 1.0f*halfDepth, /* Normal */-1.0f, 0.0f, 0.0f, /* TextureUV */0.0f, 0.0f,/*TangentUVW*/ 1.0f, 0.0f, 0.0f);// top-left
	v[3] = Vertex(/*Position*/1.0f*halfWidth, 1.0f*halfHeight, -1.0f*halfDepth, /* Normal */-1.0f, 0.0f, 0.0f, /* TextureUV */1.0f, 0.0f,/*TangentUVW*/ 1.0f, 0.0f, 0.0f);// top-right

	// left plane 
	v[4] = Vertex(/*Position*/ -1.0f*halfWidth, -1.0f*halfHeight, -1.0f*halfDepth, /* Normal */1.0f, 0.0f, 0.0f, /* TextureUV */0.0f, 1.0f,/*TangentUVW*/ 1.0f, 0.0f, 0.0f);// bottom-left
	v[5] = Vertex(/*Position*/-1.0f*halfWidth, -1.0f*halfHeight, 1.0f*halfDepth, /* Normal */1.0f, 0.0f, 0.0f, /* TextureUV */1.0f, 1.0f,/*TangentUVW*/ 1.0f, 0.0f, 0.0f);// bottom-right
	v[6] = Vertex(/*Position*/ -1.0f*halfWidth, 1.0f*halfHeight, -1.0f*halfDepth, /* Normal */1.0f, 0.0f, 0.0f, /* TextureUV */0.0f, 0.0f,/*TangentUVW*/ 1.0f, 0.0f, 0.0f);// top-left
	v[7] = Vertex(/*Position*/-1.0f*halfWidth, 1.0f*halfHeight, 1.0f*halfDepth, /* Normal */1.0f, 0.0f, 0.0f, /* TextureUV */1.0f, 0.0f,/*TangentUVW*/ 1.0f, 0.0f, 0.0f);// top-right

	// top plane 
	v[8] = Vertex(/*Position*/ -1.0f*halfWidth, 1.0f*halfHeight, -1.0f*halfDepth, /* Normal */0.0f, -1.0f, 0.0f, /* TextureUV */0.0f, 1.0f,/*TangentUVW*/ 1.0f, 0.0f, 0.0f);// bottom-left
	v[9] = Vertex(/*Position*/1.0f*halfWidth, 1.0f*halfHeight, -1.0f*halfDepth, /* Normal */0.0f, -1.0f, 0.0f, /* TextureUV */1.0f, 1.0f,/*TangentUVW*/ 1.0f, 0.0f, 0.0f);// bottom-right
	v[10] = Vertex(/*Position*/ -1.0f*halfWidth, 1.0f*halfHeight, 1.0f*halfDepth, /* Normal */0.0f, -1.0f, 0.0f, /* TextureUV */0.0f, 0.0f,/*TangentUVW*/ 1.0f, 0.0f, 0.0f);// top-left
	v[11] = Vertex(/*Position*/1.0f*halfWidth, 1.0f*halfHeight, 1.0f*halfDepth, /* Normal */0.0f, -1.0f, 0.0f, /* TextureUV */1.0f, 0.0f,/*TangentUVW*/ 1.0f, 0.0f, 0.0f);// top-right

	// bottom plane 
	v[12] = Vertex(/*Position*/ -1.0f*halfWidth, -1.0f*halfHeight, 1.0f*halfDepth, /* Normal */0.0f, 1.0f, 0.0f, /* TextureUV */0.0f, 1.0f,/*TangentUVW*/ 1.0f, 0.0f, 0.0f);// bottom-left
	v[13] = Vertex(/*Position*/1.0f*halfWidth, -1.0f*halfHeight, 1.0f*halfDepth, /* Normal */0.0f, 1.0f, 0.0f, /* TextureUV */1.0f, 1.0f,/*TangentUVW*/ 1.0f, 0.0f, 0.0f);// bottom-right
	v[14] = Vertex(/*Position*/ -1.0f*halfWidth, -1.0f*halfHeight, -1.0f*halfDepth, /* Normal */0.0f, 1.0f, 0.0f, /* TextureUV */0.0f, 0.0f,/*TangentUVW*/ 1.0f, 0.0f, 0.0f);// top-left
	v[15] = Vertex(/*Position*/1.0f*halfWidth, -1.0f*halfHeight, -1.0f*halfDepth, /* Normal */0.0f, 1.0f, 0.0f, /* TextureUV */1.0f, 0.0f,/*TangentUVW*/ 1.0f, 0.0f, 0.0f);// top-right

	// far																																								   // far plane from screen to viewer
	v[16] = Vertex(/*Position*/1.0f*halfWidth, -1.0f*halfHeight, 1.0f*halfDepth, /* Normal */0.0f, 0.0f, -1.0f, /* TextureUV */0.0f, 1.0f,/*TangentUVW*/ 1.0f, 0.0f, 0.0f);// bottom-left
	v[17] = Vertex(/*Position*/ -1.0f*halfWidth, -1.0f*halfHeight, 1.0f*halfDepth, /* Normal */0.0f, 0.0f, -1.0f, /* TextureUV */1.0f, 1.0f,/*TangentUVW*/ 1.0f, 0.0f, 0.0f);// bottom-right
	v[18] = Vertex(/*Position*/1.0f*halfWidth, 1.0f*halfHeight, 1.0f*halfDepth, /* Normal */0.0f, 0.0f, -1.0f, /* TextureUV */0.0f, 0.0f,/*TangentUVW*/ 1.0f, 0.0f, 0.0f);// top-left
	v[19] = Vertex(/*Position*/ -1.0f*halfWidth, 1.0f*halfHeight, 1.0f*halfDepth, /* Normal */0.0f, 0.0f, -1.0f, /* TextureUV */1.0f, 0.0f,/*TangentUVW*/ 1.0f, 0.0f, 0.0f);// top-right
	
	
	// near plane from viewer to screen
	v[20] = Vertex(/*Position*/ -1.0f*halfWidth, -1.0f*halfHeight, -1.0f*halfDepth, /* Normal */0.0f, 0.0f, 1.0f, /* TextureUV */0.0f, 1.0f,/*TangentUVW*/ 1.0f, 0.0f, 0.0f);// bottom-left
	v[21] = Vertex(/*Position*/1.0f*halfWidth, -1.0f*halfHeight, -1.0f*halfDepth, /* Normal */0.0f, 0.0f, 1.0f, /* TextureUV */1.0f, 1.0f,/*TangentUVW*/ 1.0f, 0.0f, 0.0f);// bottom-right
	v[22] = Vertex(/*Position*/ -1.0f*halfWidth, 1.0f*halfHeight, -1.0f*halfDepth, /* Normal */0.0f, 0.0f, 1.0f, /* TextureUV */0.0f, 0.0f,/*TangentUVW*/ 1.0f, 0.0f, 0.0f);// top-left
	v[23] = Vertex(/*Position*/1.0f*halfWidth, 1.0f*halfHeight, -1.0f*halfDepth, /* Normal */0.0f, 0.0f, 1.0f, /* TextureUV */1.0f, 1.0f,/*TangentUVW*/ 1.0f, 0.0f, 0.0f);// top-right

	meshData.Vertices.assign(&v[0], &v[24]);

	uint32 i[36];
	// Fill in the front face index data
	i[0] = 0; i[1] = 2; i[2] = 3;
	i[3] = 0; i[4] = 3; i[5] = 1;

	i[6] = 4; i[7] = 6; i[8] = 7;
	i[9] = 4; i[10] = 7; i[11] = 5;

	i[12] = 8; i[13] = 10; i[14] = 11;
	i[15] = 8; i[16] = 11; i[17] = 9;

	i[18] = 12; i[19] = 14; i[20] = 15;
	i[21] = 12; i[22] = 15; i[23] = 13;

	i[24] = 16; i[25] = 18; i[26] = 19;
	i[27] = 16; i[28] = 19; i[29] = 17;

	i[30] = 20; i[31] = 22; i[32] = 23;
	i[33] = 20; i[34] = 23; i[35] = 21;

	meshData.Indices32.assign(&i[0], &i[36]);

	return meshData;
}

//jingz todo
void GeometryHelper::Subdivide(MeshData & meshData)
{
	// Save a copy of the input geometry.
	MeshData inputCopy = meshData;

	meshData.Vertices.resize(0);

}
