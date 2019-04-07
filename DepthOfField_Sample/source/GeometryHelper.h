#pragma once
#include <cstdint>
#include <vector>
#include <DirectXMath.h>

class GeometryHelper
{
public:

	using uint16 = std::uint16_t;
	using uint32 = std::uint32_t;

	struct Vertex
	{
		Vertex() {}
		Vertex(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& n, const DirectX::XMFLOAT2& uv,const DirectX::XMFLOAT3& tangentUVW)
			:Position(pos),Normal(n),TextureUV(uv),TangentUVW(tangentUVW)
		{

		}

		Vertex(float px, float py, float pz,
			float nx, float ny, float nz,
			float u, float v,
			float tangentU, float tangentV, float tangentW)
			:Position(px,py,pz),Normal(nx,ny,nz),TextureUV(u,v),TangentUVW(tangentU,tangentV,tangentW)
		{

		}


		DirectX::XMFLOAT3 Position;
		DirectX::XMFLOAT3 Normal;
		DirectX::XMFLOAT2 TextureUV;
		DirectX::XMFLOAT3 TangentUVW;

	};

	struct MeshData
	{
		std::vector<Vertex> Vertices;
		std::vector<uint32> Indices32;

		std::vector<uint16>& GetIndices16()
		{
			if (mIndices16.empty())
			{
				mIndices16.resize(Indices32.size());
				for (size_t i = 0;i<Indices32.size();++i)
				{
					mIndices16[i] = static_cast<uint16>(Indices32[i]);
				}
			}

			return mIndices16;
		}

	private:
		std::vector<uint16> mIndices16;
	};

	static MeshData CreateBox(float width=1.0f,float height=1.0f,float depth=1.0f,uint32 numSubdivision=6,float centerX=0.0f, float centerY=0.0f, float centerZ=0.0f);

	static MeshData CreateScreenQuad();

	static MeshData CreateSphere(float radius, uint32 sliceCount, uint32 stackCount,DirectX::XMFLOAT3 center = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f));

	static void Subdivide(MeshData& meshData);

protected:
private:
};