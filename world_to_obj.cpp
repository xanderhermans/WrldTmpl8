#include "precomp.h"
#define SIMPLECULL

using namespace Tmpl8;

void WorldToOBJ(Tmpl8::World* world, std::string filename)
{
	int sizex = GRIDWIDTH * BRICKDIM;
	int sizey = GRIDHEIGHT * BRICKDIM;
	int sizez = GRIDDEPTH * BRICKDIM;

	vector<int3> vertices;
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

					int vi = vertices.size() - 1;
					int vi1 = ++vi;
					int vi2 = ++vi;
					int vi3 = ++vi;
					int vi4 = ++vi;
					int vi5 = ++vi;
					int vi6 = ++vi;
					int vi7 = ++vi;
					int vi8 = ++vi;

					int3 v1 = int3(x, y, z);
					int3 v2 = int3(x, y, z + 1);
					int3 v3 = int3(x + 1, y, z + 1);
					int3 v4 = int3(x + 1, y, z);
					int3 v5 = int3(x, y + 1, z);
					int3 v6 = int3(x, y + 1, z + 1);
					int3 v7 = int3(x + 1, y + 1, z + 1);
					int3 v8 = int3(x + 1, y + 1, z);

					vertices.push_back(v1);
					vertices.push_back(v2);
					vertices.push_back(v3);
					vertices.push_back(v4);
					vertices.push_back(v5);
					vertices.push_back(v6);
					vertices.push_back(v7);
					vertices.push_back(v8);

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

	printf("%d vertices with %d faces\r\n", int(vertices.size()), int(indices.size()));

	ofstream myfile;
	myfile.open(filename);
	myfile << "# " << vertices.size() << " vetices " << indices.size() << " faces" << endl;
	myfile << "# vertices" << endl;
	for (int3& v : vertices)
	{
		myfile << "v " << v.x / 10.0f << " " << v.y / 10.0f << " " << v.z / 10.0f << endl;
	}
	myfile << "# faces" << endl;
	for (int3& f : indices)
	{
		myfile << "f " << f.x + 1 << " " << f.y + 1 << " " << f.z + 1 << endl;
	}
	myfile.close();
}