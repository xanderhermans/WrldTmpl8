#include "precomp.h"
#include "mygamescene.h"
#include <filesystem>
#include <unordered_map>

using namespace MyGameScene;

string small_box_path = "scene_export/cornellbox/cornell-box.vox";

#define BRICKLIGHTS 1

const vector<pair<float3, float3>> colormapping =
{
	{make_float3(0.000000, 0.333333, 0.000000), make_float3(0.000000, 1.0, 0.000000)},
	{make_float3(0.866667, 0.666667, 0.466667), make_float3(1.0, 1.0, 1.0)},
	{make_float3(0.466667, 0.000000, 0.000000), make_float3(1.0, 0.000000, 0.000000)},
};

bool shouldBeReplaced2(float3 r, float3& replace)
{
	for (auto& key : colormapping)
	{
		bool isx = abs(r.x - key.first.x) < 10e-3;
		bool isy = abs(r.y - key.first.y) < 10e-3;
		bool isz = abs(r.z - key.first.z) < 10e-3;
		if (isx && isy && isz)
		{
			replace = key.second;
			return true;
		}
	}
	return false;
}

void MyGameScene::CreateCornellBoxWorld(World& world)
{
	world.Clear();

	int idSmall = LoadSprite(small_box_path.c_str());
	StampSpriteTo(idSmall, 0, 0, 0);

	uint sizex = MAPWIDTH;
	uint sizey = MAPHEIGHT;
	uint sizez = MAPDEPTH;
	uint* grid = world.GetGrid();
	PAYLOAD* brick = world.GetBrick();
	for (uint by = 0; by < GRIDHEIGHT; by++)
	{
		for (uint bz = 0; bz < GRIDDEPTH; bz++)
		{
			for (uint bx = 0; bx < GRIDWIDTH; bx++)
			{
				ushort vox = 0;
				const uint cellIdx = bx + bz * GRIDWIDTH + by * GRIDWIDTH * GRIDDEPTH;
				const uint g = grid[cellIdx];
				if ((g & 1) != 0)
				{
					for (uint ly = 0; ly < BRICKDIM; ly++)
					{
						for (uint lz = 0; lz < BRICKDIM; lz++)
						{
							for (uint lx = 0; lx < BRICKDIM; lx++)
							{
								uint _index = (g >> 1) * BRICKSIZE + lx + ly * BRICKDIM + lz * BRICKDIM * BRICKDIM;
								vox = brick[_index];
								float3 rgb = ToFloatRGB(vox);
								float3 toReplace;
								if (shouldBeReplaced2(rgb, toReplace))
								{
									uint x = lx + bx * BRICKDIM;
									uint y = ly + by * BRICKDIM;
									uint z = lz + bz * BRICKDIM;
									uint c = ToUint(toReplace);
									world.Set(x, y, z, c);
								}
							}
						}
					}
				}
			}
		}
	}

	int _ym = 256 - 8;
	int _x = 64;
	int _xm = 64 + 128;
	int _z = 64;
	int _zm = 64 + 128;
#if BRICKLIGHTS == 0
	for (int x = _x; x <= _xm; x++) for (int z = _z; z <= _zm; z++)
	{
		//world.Set(x, _y, z, WHITE | (1 << 12));
		world.Set(x, _ym, z, WHITE | (1 << 12));
	}
#else
	for (int x = _x; x < _xm; x+=BRICKDIM) for (int z = _z; z < _zm; z+=BRICKDIM)
	{
		//world.Set(x, _y, z, WHITE | (1 << 12));
		world.SetBrick(x, _ym, z, WHITE | (15 << 12));
	}
#endif
}