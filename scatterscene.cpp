#include "precomp.h"
#include "mygamescene.h"
#include <filesystem>

using namespace MyGameScene;

const vector<pair<float3, float4>> colormapping = 
{ 
	{make_float3(0.067), make_float4(1.0, 0.8, 0.6, 3.0)},
	{make_float3(0.867, 0.467, 0.067), make_float4(1.0, 1.0, 1.0, 15.0)},
	{make_float3(0.467, 0.933, 0.867), make_float4(1.0, 1.0, 1.0, 15.0)},
	{make_float3(0.267, 0.133, 0.200), make_float4(0.0, 0.5, 1.0, 4.0)}
};

bool shouldBeReplaced1(float3 r, float4& replace)
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

void MyGameScene::CreateScatteredWorld(World& world)
{
	world.Clear();

	const int world_width = 512 - 1;
	const int world_height = 256 - 1;
	const int world_depth = 512 - 1;

	uint a = LoadSprite("assets/flyingapts.vox");
	StampSpriteTo(a, 0, 0, 0);

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
								float4 toReplace;
								if (shouldBeReplaced1(rgb, toReplace))
								{
									uint x = lx + bx * BRICKDIM;
									uint y = ly + by * BRICKDIM;
									uint z = lz + bz * BRICKDIM;
									uint c = ToUint(make_float3(toReplace));
									world.Set(x, y, z, c | (int(toReplace.w) << 12));
								}
							}
						}
					}
				}
			}
		}
	}
}