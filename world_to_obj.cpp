#include "precomp.h"
#include <unordered_map>
#include <unordered_set>
#include <filesystem>
#define CULL_DUPE_VERTICES
#define CULL_OCCLUDED_FACES
#define LEFTHANDED_COORDINATE_SYSTEM
#define USE_GREEDY_MESHING

using namespace Tmpl8;

bool IsEmitter(const uint v)
{
	return (v >> 12 & 15) > 0;
}

float Tmpl8::EmitStrength(const uint v)
{
	return (float)((v & 0xf000) >> 12);
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
						mesh.tris.push_back(f1);
						mesh.tris.push_back(f2);
					}

#ifdef CULL_OCCLUDED_FACES
					if (!left_occluded)
#endif // SIMPLECULL
					{
						Tri f3 = Tri(vi5, vi1, vi2, cv);
						Tri f4 = Tri(vi2, vi6, vi5, cv);
						mesh.tris.push_back(f3);
						mesh.tris.push_back(f4);
					}

#ifdef CULL_OCCLUDED_FACES
					if (!back_occluded)
#endif // SIMPLECULL
					{
						Tri f5 = Tri(vi6, vi2, vi3, cv);
						Tri f6 = Tri(vi3, vi7, vi6, cv);
						mesh.tris.push_back(f5);
						mesh.tris.push_back(f6);
					}

#ifdef CULL_OCCLUDED_FACES
					if (!right_occluded)
#endif // SIMPLECULL
					{
						Tri f7 = Tri(vi7, vi3, vi4, cv);
						Tri f8 = Tri(vi4, vi8, vi7, cv);
						mesh.tris.push_back(f7);
						mesh.tris.push_back(f8);
					}

#ifdef CULL_OCCLUDED_FACES
					if (!bottom_occluded)
#endif // SIMPLECULL
					{
						Tri f9 = Tri(vi1, vi4, vi3, cv);
						Tri f10 = Tri(vi3, vi2, vi1, cv);
						mesh.tris.push_back(f9);
						mesh.tris.push_back(f10);
					}

#ifdef CULL_OCCLUDED_FACES
					if (!top_occluded)
#endif // SIMPLECULL
					{
						Tri f11 = Tri(vi7, vi8, vi5, cv);
						Tri f12 = Tri(vi5, vi6, vi7, cv);
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
			float strength = EmitStrength(col);
			float3 ke = kd * strength;
			fprintf(matfile, "Ke %f %f %f\n", ke.x, ke.y, ke.z);
		}

		fprintf(matfile, "\n");
	}
	fclose(matfile);

	filesystem::path objfilename = path.replace_extension(".obj");
	FILE* objfile = fopen(objfilename.string().c_str(), "w");

	if (!objfile)
	{
		printf("write_obj: can't write data file \"%s\".\n", objfilename.string().c_str());
		return;
	}

	fprintf(objfile, "# %d vertices %d faces\n", int(mesh.vertices.size()), int(mesh.tris.size()));
	fprintf(objfile, "mtllib %s\n", matfilename.filename().string().c_str());
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

void printvis(bool* vis)
{
	uint sizex = GRIDWIDTH * BRICKDIM;
	uint sizey = GRIDHEIGHT * BRICKDIM;
	uint sizez = GRIDDEPTH * BRICKDIM;

	for (uint z = 0; z < 8; z++)
	{
		for (uint x = 0; x < 8; x++)
		{
			bool res = vis[x + z * sizex];
			printf("%d ", res);
		}
		printf("\r\n");
	}
}

inline void ToAxis(const int lc1, const int lc2, const int layer, int& x, int& y, int& z, int axis)
{
	int data[3] = { lc1, lc2, layer };
	x = data[axis % 3];
	y = data[(axis + 1) % 3];
	z = data[(axis + 2) % 3];
}

inline uint GetFromGrid(uint* grid, const uint lc1, const uint lc2, const int layer, int axis)
{
	if (layer < 0) return 0;
	static uint __sizex = GRIDWIDTH * BRICKDIM;
	static uint __sizey = GRIDHEIGHT * BRICKDIM;
	static uint __sizez = GRIDDEPTH * BRICKDIM;

	int x, y, z;
	ToAxis(lc1, lc2, layer, x, y, z, axis);

	if (x < __sizex && y < __sizey && z < __sizez)
	{
		// bx + bz * GRIDWIDTH + by * GRIDWIDTH * GRIDDEPTH from world.h
		return grid[x + z * __sizex + y * __sizex * __sizez];
	}
	return 0;
}

void Tmpl8::QuadsForAxis(uint* grid, vector<UnboundQuad>& quads, int axis_i, int layerside)
{
	if (axis_i > 2)
	{
		throw runtime_error("axis index exceed 2");
	}

	static uint __sizex = GRIDWIDTH * BRICKDIM;
	static uint __sizey = GRIDHEIGHT * BRICKDIM;
	static uint __sizez = GRIDDEPTH * BRICKDIM;
	static uint localdims1[3] = { __sizex, __sizez, __sizey };
	static uint localdims2[3] = { __sizey, __sizex, __sizez };
	static uint layerdims[3] = { __sizez, __sizey, __sizex };
	static int sideoffsets[2] = { -1, 1 };

	bool* visited = nullptr;
	uint layerdim = layerdims[axis_i];
	uint localdim1 = localdims1[axis_i];
	uint localdim2 = localdims2[axis_i];
	int sideoffset = sideoffsets[layerside];

	for (uint layer = 0; layer < layerdim; layer++)
	{
		if (visited != nullptr) { for (uint z = 0; z < localdim2; z++) { for (uint x = 0; x < localdim1; x++) { visited[x + z * localdim1] = false; } } }
		else { visited = new bool[localdim1 * localdim2]; for (uint z = 0; z < localdim2; z++) { for (uint x = 0; x < localdim1; x++) { visited[x + z * localdim1] = false; } } }

		for (uint localcount2 = 0; localcount2 < localdim2; localcount2++)
		{
			uint runxstart = localdim1;
			uint lastvoxel = 0;
			uint localcount1 = 0;
			while (localcount1 < localdim1)
			{
				bool runStarted = runxstart != localdim1;

				if (runStarted)
				{
					uint voxel = GetFromGrid(grid, localcount1, localcount2, layer, axis_i);
					uint voxel1 = GetFromGrid(grid, localcount1, localcount2, layer + sideoffset, axis_i);
					bool neighbourOccluded = voxel1 != 0;
					if (visited[localcount1 + localcount2 * localdim1] || voxel != lastvoxel || neighbourOccluded)
					{
						// not the same voxel, we have work to do
						uint _localcount2;
						for (_localcount2 = localcount2 + 1; _localcount2 < localdim2; _localcount2++)
						{
							for (uint _localcount1 = runxstart; _localcount1 < localcount1; _localcount1++)
							{
								uint voxel = GetFromGrid(grid, _localcount1, _localcount2, layer, axis_i);
								uint voxel1 = GetFromGrid(grid, _localcount1, _localcount2, layer + sideoffset, axis_i);
								bool neighbourOccluded = voxel1 != 0;
								if (voxel != lastvoxel || visited[_localcount1 + _localcount2 * localdim1] || neighbourOccluded)
								{
									_localcount2--;
									goto makequad;
								}
							}
							for (uint _localcount1 = runxstart; _localcount1 < localcount1; _localcount1++)
							{
								visited[_localcount1 + _localcount2 * localdim1] = true;
							}
						}
					makequad:
						{
							int runystart = localcount2;
							int runxend = localcount1;
							int runyend = _localcount2 + 1;
							int runheight = layer + layerside;
							int x0, y0, z0, x1, y1, z1;
							ToAxis(runxstart, runystart, runheight, x0, y0, z0, axis_i);
							ToAxis(runxend, runyend, runheight, x1, y1, z1, axis_i);

#ifdef LEFTHANDED_COORDINATE_SYSTEM
							x0 *= -1;
							x1 *= -1;
#endif
							int3 v1 = make_int3(x0, y0, z1);
							int3 v2 = make_int3(x1, y0, z0);
							int3 v3 = make_int3(x1, y1, z1);
							int3 v4 = make_int3(x0, y1, z0);

#if 0
							static int3 _test = make_int3(0);
							int3 test1 = v1 - v2;
							int3 test2 = v1 - v3;
							int3 test3 = v1 - v4;
							int3 test4 = v2 - v3;
							int3 test5 = v2 - v4;
							int3 test6 = v3 - v4;

							bool test = _test == test1 || _test == test2 || _test == test3 || _test == test4 || _test == test5 || _test == test6;

							auto pp = [](int3 v1, int3 v2) {printf("v %d %d %d %d %d %d\n", v1.x, v1.y, v1.z, v2.x, v2.y, v2.z); };
							if (test)
							{
								if (_test == test1)
								{
									pp(v1, v2);
								}
								if (_test == test2)
								{
									pp(v1, v3);
								}
								if (_test == test3)
								{
									pp(v1, v4);
								}
								if (_test == test4)
								{
									pp(v2, v3);
								}
								if (_test == test5)
								{
									pp(v2, v4);
								}
								if (_test == test6)
								{
									pp(v3, v4);
								}
							}

							if (runxstart == runxend || runystart == runyend || test)
							{
								printf("Duplicate vertex position x:%d y:%d z:%d x:%d y:%d z:%d lc1:%d lc2:%d layer:%d\n", x0, y0, z0, x1, y1, z1, localcount1, _localcount2 + 1, layer);
							}
#endif

#ifdef PRINTINFO
							printf("face: ");
							printf("lc1:%d lc2:%d ", localcount1, localcount2);
							printf("%d %d %d %d\r\n", x0, z0, x1, z1);
							printvis(visited);
#endif

							UnboundQuad quad = UnboundQuad(v1, v2, v3, v4);
							quad.col = (lastvoxel & 0xffff) | ((axis_i & 0xf) << 16) | ((layerside & 0xf) << 20);
							quads.push_back(quad);
						}

						runxstart = localdim1;
						lastvoxel = 0;
						continue;
					}
				}
				else
				{
#if 0
					if (localcount2 * localdim1 + localcount1 > localdim1 * localdim2)
					{
						printf("TRYING TO READ OUTSIDE ARRAY %d %d %d i:%d size:%d", localcount1, localcount2, localdim2, localcount2 * localdim1 + localcount1, localdim1 * localdim2);
						return;
					}
#endif

					if (!visited[localcount1 + (int)(localcount2 * localdim1)]) // havent visited this
					{
						uint voxel = GetFromGrid(grid, localcount1, localcount2, layer, axis_i);
						uint voxel1 = GetFromGrid(grid, localcount1, localcount2, layer + sideoffset, axis_i);
						bool neighbourOccluded = voxel1 != 0;
						if (voxel != 0 && !neighbourOccluded) // not empty space
						{
							runxstart = localcount1;
							lastvoxel = voxel;
						}
					}
				}

				visited[localcount1 + localcount2 * localdim1] = true;
				localcount1++;
			}
		}

#ifdef PRINTINFO	
		if (layer < 8) printf("\r\n");
#endif
	}

	if (visited != nullptr) delete[] visited;
}

void QuadsToMesh(vector<UnboundQuad>& quads, Mesh& mesh)
{
	mesh.tris.clear();
	mesh.vertices.clear();

	unordered_map<int, int> vertex_indices;
	vector<int3>& vertices = mesh.vertices;

	for (UnboundQuad& quad : quads)
	{
		ushort col = quad.col & 0xffff;
		ushort side = (quad.col & 0xf0000) >> 16;
		ushort layerside = (quad.col & 0xf00000) >> 20;
		int3 _v1 = quad.v1;
		int3 _v2 = quad.v2;
		int3 _v3 = quad.v3;
		int3 _v4 = quad.v4;

		int v1 = createVertex(vertex_indices, vertices, _v1);
		int v2 = createVertex(vertex_indices, vertices, _v2);
		int v3 = createVertex(vertex_indices, vertices, _v3);
		int v4 = createVertex(vertex_indices, vertices, _v4);

		int vs[4] = { v1,v2,v3,v4 };

		int orderingtri1[24] = {
			0,1,2,3,
			0,2,1,3,
			0,1,3,2,

			3,2,1,0,
			3,1,2,0,
			2,3,1,0
		};

#if 0
		if (v1 == v2 || v1 == v4 || v2 == v4)
		{
			printf("Ill-formed tri %d %d %d\r\n", v1, v2, v4);
		}
		if (v2 == v3 || v2 == v4 || v3 == v4)
		{
			printf("Ill-formed tri %d %d %d\r\n", v2, v3, v4);
		}
#endif

		int offset = (side * 4) + layerside * 12;
		if (offset + 3 > 23) printf("Wrong offset, unimplemented cube sides?\n");

		Tri tri1 = Tri(vs[orderingtri1[0 + offset]], vs[orderingtri1[1 + offset]], vs[orderingtri1[2 + offset]], col);
		Tri tri2 = Tri(vs[orderingtri1[0 + offset]], vs[orderingtri1[2 + offset]], vs[orderingtri1[3 + offset]], col);

		mesh.tris.push_back(tri1);
		mesh.tris.push_back(tri2);
	}
}

void WorldToQuadMesh(World& world, Mesh& mesh)
{
	printf("Pre-allocating grid\r\n");
	static uint __sizex = GRIDWIDTH * BRICKDIM;
	static uint __sizey = GRIDHEIGHT * BRICKDIM;
	static uint __sizez = GRIDDEPTH * BRICKDIM;
	uint* grid = new uint[__sizex * __sizey * __sizez];
	{
		for (uint y = 0; y < __sizey; y++)
		{
			for (uint z = 0; z < __sizez; z++)
			{
				for (uint x = 0; x < __sizex; x++)
				{
					grid[x + z * __sizex + y * __sizex * __sizez] = world.Get(x, y, z);
				}
			}
		}
	}
	printf("Grid filled\r\n");

	GreeyMeshJob greedymeshjobs[6];
	JobManager* jm = JobManager::GetJobManager();

	for (int i = 0, j = 0; i < 3; i++)
	{
		for (int layerside = 0; layerside < 2; layerside++) 
		{
			GreeyMeshJob& job = greedymeshjobs[j];
			job.grid = grid;
			job.i = i;
			job.layerside = layerside;

			jm->AddJob2(&job);
			j++;
		}
	}

	jm->RunJobs();
	vector<UnboundQuad> quads;
	for (int i = 0; i < 6; i++)
	{
		GreeyMeshJob& job = greedymeshjobs[i];
		for (UnboundQuad& quad : job.quads)
		{
			quads.push_back(quad);
		}
	}

	printf("Creating mesh\r\n");
	QuadsToMesh(quads, mesh);

	printf("quads: %d \r\n", (int)quads.size());

	delete[] grid;
}

// NOTE: I think there is an issue with worlds with size 1024. Try slightly smaller worlds.
void Tmpl8::WorldToOBJ(Tmpl8::World* world, std::string filename)
{
	printf("Exporting world to obj!\r\n");
	Mesh mesh;

#ifdef USE_GREEDY_MESHING
	WorldToQuadMesh(*world, mesh);
#else
	worldToMesh(world, mesh);
#endif // USE_GREEDY_MESHING

	printf("%d vertices with %d faces\r\n", int(mesh.vertices.size()), int(mesh.tris.size()));

	SaveMeshToObj(mesh, filename);
}