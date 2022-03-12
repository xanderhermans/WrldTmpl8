#pragma once

namespace Tmpl8
{
	void WorldToOBJ(Tmpl8::World* world, std::string filename);
	float3 ToFloatRGB(const uint v);
	uint ToUint(const float3 c);
	float EmitStrength(const uint v);

	struct Tri
	{
		int3 f;
		ushort col;

		Tri(int i1, int i2, int i3, ushort _col) { f = int3(i1, i2, i3); col = _col; }
	};

	struct UnboundQuad
	{
		uint col = 0;
		int3 v1 = make_int3(0), v2 = make_int3(0), v3 = make_int3(0), v4 = make_int3(0);
		UnboundQuad(int3 v1, int3 v2, int3 v3, int3 v4) : v1(v1), v2(v2), v3(v3), v4(v4) {}
	};

	struct Mesh
	{
		vector<int3> vertices;
		vector<Tri> tris;
	};

	void QuadsForAxis(uint* grid, vector<UnboundQuad>& quads, int axis_i, int layerside);

	class GreeyMeshJob : public Job
	{
	public:
		void Main()
		{
			QuadsForAxis(grid, quads, i, layerside);
			printf("Axis %d side done %d\r\n", i, layerside);
		}
		uint* grid;
		int i, layerside;
		vector<UnboundQuad> quads;
	};
}