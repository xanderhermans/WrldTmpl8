#include "precomp.h"
#include <unordered_map>
#include <unordered_set>
#include <filesystem>
#define CULL_DUPE_VERTICES
#define CULL_OCCLUDED_FACES

using namespace Tmpl8;

struct Tri
{
	int3 f;
	ushort col;
	ushort side;

	Tri(int i1, int i2, int i3, ushort _col) { f = int3(i1, i2, i3); col = _col; }
};

struct Mesh
{
	vector<int3> vertices;
	vector<Tri> tris;
};

bool IsEmitter(const uint v)
{
	return (v >> 12 & 15) > 0;
}

// convert a voxel color to floating point rgb // from tools.cl
float3 Tmpl8::ToFloatRGB(const uint v)
{
#if PAYLOADSIZE == 1
	return float3((v >> 5) * (1.0f / 7.0f), ((v >> 2) & 7) * (1.0f / 7.0f), (v & 3) * (1.0f / 3.0f));
#else
	return float3(((v >> 8) & 15) * (1.0f / 15.0f), ((v >> 4) & 15) * (1.0f / 15.0f), (v & 15) * (1.0f / 15.0f));
#endif
}

uint Tmpl8::ToUint(float3 c)
{
	c = clamp(c, float3(0, 0, 0), float3(1, 1, 1));
#if PAYLOADSIZE == 1
	return (uint(c.x * 7.0f) & 7) << 5 | (uint(c.y * 7.0f) & 7) << 2 | (uint(c.z * 3.0) & 3);
#else
	return (uint(c.x * 15.0f) & 15) << 8 | (uint(c.y * 15.0f) & 15) << 4 | (uint(c.z * 15.0f) & 15);
#endif
}



int createVertex(unordered_map<int, int>& map, vector<int3>& vertices, int3 v)
{
	int hash = v.x + v.z * GRIDWIDTH * BRICKDIM + v.y * GRIDWIDTH * BRICKDIM * GRIDDEPTH * BRICKDIM;
	auto search = map.find(hash);
	if (search != map.end())
	{
		return search->second;
	}
	int index = int(vertices.size());
	map.insert({ hash, index });
	vertices.push_back(v);
	return index;
}

void worldToMesh(Tmpl8::World* world, Mesh& mesh)
{
	uint sizex = GRIDWIDTH * BRICKDIM;
	uint sizey = GRIDHEIGHT * BRICKDIM;
	uint sizez = GRIDDEPTH * BRICKDIM;

	unordered_map<int, int> vertex_indices;

	for (uint y = 0; y < sizey; y++)
	{
		for (uint z = 0; z < sizez; z++)
		{
			for (uint x = 0; x < sizex; x++)
			{
				ushort cv = world->Get(x, y, z);
				if (cv > 0)
				{
#ifdef CULL_DUPE_VERTICES
					ushort cv1 = world->Get(x, y, z - 1);
					ushort cv2 = world->Get(x, y, z + 1);
					ushort cv3 = world->Get(x - 1, y, z);
					ushort cv4 = world->Get(x + 1, y, z);
					ushort cv5 = world->Get(x, y - 1, z);
					ushort cv6 = world->Get(x, y + 1, z);
					bool front_occluded = cv1 > 0;
					bool back_occluded = cv2 > 0;
					bool left_occluded = cv3 > 0;
					bool right_occluded = cv4 > 0;
					bool bottom_occluded = cv5 > 0;
					bool top_occluded = cv6 > 0;
					if (back_occluded && front_occluded && left_occluded && right_occluded && bottom_occluded && top_occluded)
					{
						continue;
					}
#endif // SIMPLECULL

					int3 v1 = int3(x, y, z);
					int vi1 = createVertex(vertex_indices, mesh.vertices, v1);
					int3 v2 = int3(x, y, z + 1);
					int vi2 = createVertex(vertex_indices, mesh.vertices, v2);
					int3 v3 = int3(x + 1, y, z + 1);
					int vi3 = createVertex(vertex_indices, mesh.vertices, v3);
					int3 v4 = int3(x + 1, y, z);
					int vi4 = createVertex(vertex_indices, mesh.vertices, v4);
					int3 v5 = int3(x, y + 1, z);
					int vi5 = createVertex(vertex_indices, mesh.vertices, v5);
					int3 v6 = int3(x, y + 1, z + 1);
					int vi6 = createVertex(vertex_indices, mesh.vertices, v6);
					int3 v7 = int3(x + 1, y + 1, z + 1);
					int vi7 = createVertex(vertex_indices, mesh.vertices, v7);
					int3 v8 = int3(x + 1, y + 1, z);
					int vi8 = createVertex(vertex_indices, mesh.vertices, v8);

#ifdef CULL_OCCLUDED_FACES
					if (!front_occluded)
#endif // SIMPLECULL
					{
						Tri f1 = Tri(vi8, vi4, vi1, cv);
						Tri f2 = Tri(vi1, vi5, vi8, cv);
						f1.side = 1;
						f2.side = 1;
						mesh.tris.push_back(f1);
						mesh.tris.push_back(f2);
					}

#ifdef CULL_OCCLUDED_FACES
					if (!left_occluded)
#endif // SIMPLECULL
					{
						Tri f3 = Tri(vi5, vi1, vi2, cv);
						Tri f4 = Tri(vi2, vi6, vi5, cv);
						f3.side = 2;
						f4.side = 2;
						mesh.tris.push_back(f3);
						mesh.tris.push_back(f4);
					}

#ifdef CULL_OCCLUDED_FACES
					if (!back_occluded)
#endif // SIMPLECULL
					{
						Tri f5 = Tri(vi6, vi2, vi3, cv);
						Tri f6 = Tri(vi3, vi7, vi6, cv);
						f5.side = 3;
						f6.side = 3;
						mesh.tris.push_back(f5);
						mesh.tris.push_back(f6);
					}

#ifdef CULL_OCCLUDED_FACES
					if (!right_occluded)
#endif // SIMPLECULL
					{
						Tri f7 = Tri(vi7, vi3, vi4, cv);
						Tri f8 = Tri(vi4, vi8, vi7, cv);
						f7.side = 4;
						f8.side = 4;
						mesh.tris.push_back(f7);
						mesh.tris.push_back(f8);
					}

#ifdef CULL_OCCLUDED_FACES
					if (!bottom_occluded)
#endif // SIMPLECULL
					{
						Tri f9 = Tri(vi1, vi4, vi3, cv);
						Tri f10 = Tri(vi3, vi2, vi1, cv);
						f9.side = 5;
						f10.side = 5;
						mesh.tris.push_back(f9);
						mesh.tris.push_back(f10);
					}

#ifdef CULL_OCCLUDED_FACES
					if (!top_occluded)
#endif // SIMPLECULL
					{
						Tri f11 = Tri(vi7, vi8, vi5, cv);
						Tri f12 = Tri(vi5, vi6, vi7, cv);
						f11.side = 6;
						f12.side = 6;
						mesh.tris.push_back(f11);
						mesh.tris.push_back(f12);
					}
				}
			}
		}
	}
}

void SaveMeshToObj(Mesh& mesh, string filename)
{
	filesystem::path path = filename;
	if (path.has_parent_path())
	{
		filesystem::create_directories(path.parent_path());
	}
	printf("Start writing to file %s\r\n", path.string().c_str());

	// We could switch materials for every face but that would add a lot of unnecessary lines to the obj files so we sort the faces per material.
	unordered_map<ushort, vector<int3>*> tris_per_mat;
	for (Tri& f : mesh.tris)
	{
		auto search = tris_per_mat.find(f.col);
		vector<int3>* _tris;
		if (search != tris_per_mat.end())
		{
			_tris = search->second;
		}
		else
		{
			_tris = new vector<int3>();
			tris_per_mat.insert({ f.col, _tris });
		}

		_tris->push_back(f.f);
	}

	filesystem::path matfilename = path.replace_extension(".mtl");
	FILE* matfile = fopen(matfilename.string().c_str(), "w");

	if (!matfile)
	{
		printf("write_mtl: can't write data file \"%s\".\n", matfilename.string().c_str());
		return;
	}

	fprintf(matfile, "#materials\n");
	for (auto& entry : tris_per_mat)
	{
		ushort col = entry.first;
		fprintf(matfile, "newmtl %d\n", col);

		float3 kd = ToFloatRGB(col);
		fprintf(matfile, "Kd %f %f %f\n", kd.x, kd.y, kd.z);
		if (IsEmitter(col))
		{
			fprintf(matfile, "Ke %f %f %f\n", kd.x, kd.y, kd.z);
		}

		fprintf(matfile, "\n");
	}

	filesystem::path objfilename = path.replace_extension(".obj");
	FILE* objfile = fopen(objfilename.string().c_str(), "w");

	if (!objfile)
	{
		printf("write_obj: can't write data file \"%s\".\n", objfilename.string().c_str());
		return;
	}

	fprintf(objfile, "# %d vertices %d faces\n", int(mesh.vertices.size()), int(mesh.tris.size()));
	for (int3& v : mesh.vertices)
	{
		fprintf(objfile, "v %d %d %d\n", v.x, v.y, v.z); //more compact: remove trailing zeros
	}

	for (auto& entry : tris_per_mat)
	{
		ushort col = entry.first;
		fprintf(objfile, "usemtl %d\n", col);

		vector<int3>* tris = entry.second;
		for (int3& f : *tris)
		{
			// v+1 because vertex indexing starts at 1 in obj file format
			fprintf(objfile, "f %d %d %d\n", f.x + 1, f.y + 1, f.z + 1);
		}
	}

	fclose(objfile);

	printf("Done! Written to file %s\r\n", objfilename.string().c_str());
}

void Tmpl8::WorldToOBJ(Tmpl8::World* world, std::string filename)
{
	Mesh mesh;
	worldToMesh(world, mesh);

	printf("%d vertices with %d faces\r\n", int(mesh.vertices.size()), int(mesh.tris.size()));

	SaveMeshToObj(mesh, filename);
}