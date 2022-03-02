#include "precomp.h"
#include <unordered_map>
#define SIMPLECULL

using namespace Tmpl8;

int create_vertex(unordered_map<int, int>& map, vector<int3>& vertices, int3 v)
{
	int hash = v.x + v.z * GRIDWIDTH * BRICKDIM + v.y * GRIDWIDTH * BRICKDIM * GRIDDEPTH * BRICKDIM;
	auto search = map.find(hash);
	if (search != map.end()) 
	{
		return search->second;
	}
	int index = vertices.size();
	map.insert({ hash, index });
	vertices.push_back(v);
	return index;
}

void Tmpl8::WorldToOBJ(Tmpl8::World* world, std::string filename)
{
	int sizex = GRIDWIDTH * BRICKDIM;
	int sizey = GRIDHEIGHT * BRICKDIM;
	int sizez = GRIDDEPTH * BRICKDIM;

	unordered_map<int, int> vertex_indices;
	vector<int3> _vertices;
	vector<int3> indices;

	for (uint y = 0; y < sizey; y++)
	{
		for (uint z = 0; z < sizez; z++)
		{
			for (uint x = 0; x < sizex; x++)
			{
				ushort cv = world->Get(x, y, z);
				if (cv > 0)
				{
#ifdef SIMPLECULL
					ushort cv1 = world->Get(x, y, z - 1);
					ushort cv2 = world->Get(x, y, z + 1);
					ushort cv3 = world->Get(x - 1, y, z);
					ushort cv4 = world->Get(x + 1, y, z);
					ushort cv5 = world->Get(x, y - 1, z);
					ushort cv6 = world->Get(x, y + 1, z);
					if (cv1 > 0 && cv2 > 0 && cv3 > 0 && cv4 > 0 && cv5 > 0 && cv6 > 0)
					{
						continue;
					}
#endif // SIMPLECULL

					int3 v1 = int3(x, y, z);
					int vi1 = create_vertex(vertex_indices, _vertices, v1);
					int3 v2 = int3(x, y, z + 1);
					int vi2 = create_vertex(vertex_indices, _vertices, v2);
					int3 v3 = int3(x + 1, y, z + 1);
					int vi3 = create_vertex(vertex_indices, _vertices, v3);
					int3 v4 = int3(x + 1, y, z);
					int vi4 = create_vertex(vertex_indices, _vertices, v4);
					int3 v5 = int3(x, y + 1, z);
					int vi5 = create_vertex(vertex_indices, _vertices, v5);
					int3 v6 = int3(x, y + 1, z + 1);
					int vi6 = create_vertex(vertex_indices, _vertices, v6);
					int3 v7 = int3(x + 1, y + 1, z + 1);
					int vi7 = create_vertex(vertex_indices, _vertices, v7);
					int3 v8 = int3(x + 1, y + 1, z);
					int vi8 = create_vertex(vertex_indices, _vertices, v8);

					int3 f1 = int3(vi8, vi4, vi1);
					int3 f2 = int3(vi1, vi5, vi8);
					int3 f3 = int3(vi5, vi1, vi2);
					int3 f4 = int3(vi2, vi6, vi5);
					int3 f5 = int3(vi6, vi2, vi3);
					int3 f6 = int3(vi3, vi7, vi6);
					int3 f7 = int3(vi7, vi3, vi4);
					int3 f8 = int3(vi4, vi8, vi7);
					int3 f9 = int3(vi1, vi4, vi3);
					int3 f10 = int3(vi3, vi2, vi1);
					int3 f11 = int3(vi7, vi8, vi5);
					int3 f12 = int3(vi5, vi6, vi7);

					indices.push_back(f1);
					indices.push_back(f2);
					indices.push_back(f3);
					indices.push_back(f4);
					indices.push_back(f5);
					indices.push_back(f6);
					indices.push_back(f7);
					indices.push_back(f8);
					indices.push_back(f9);
					indices.push_back(f10);
					indices.push_back(f11);
					indices.push_back(f12);
				}
			}
		}
	}

	printf("%d vertices with %d faces\r\n", int(_vertices.size()), int(indices.size()));

	FILE* file = fopen(filename.c_str(), "w");

	if (!file)
	{
		printf("write_obj: can't write data file \"%s\".\n", filename.c_str());
		return;
	}

	fprintf(file, "# %d vertices %d faces\n", int(_vertices.size()), int(indices.size()));
	for (int i = 0; i < _vertices.size();i ++)
	{
		int3 v = _vertices[i];
		fprintf(file, "v %d %d %d\n", v.x, v.y, v.z); //more compact: remove trailing zeros
	}
	for (int i = 0; i < indices.size(); i++)
	{
		int3 tri = indices[i];
		fprintf(file, "f %d %d %d\n", tri.x, tri.y, tri.z);
	}
	fclose(file);
}