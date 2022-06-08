#include "precomp.h"
#include "mygamescene.h"
#include <filesystem>

using namespace MyGameScene;

const int stratumSizeX = 18;
const int stratumSizeZ = 18;
const int halfStratumSizeX = stratumSizeX / 2;
const int halfStratumSizeZ = stratumSizeZ / 2;
const int emittance = 15;
const int worldSize = 1024 - 3 * BRICKDIM;

void MyGameScene::CreateMountainWorld(World& world)
{
	// clear world geometry
	ClearWorld();
	// produce landscape
	Surface heights("assets/heightmap.png");
	Surface colours("assets/colours.png");
	for (int x = 0; x < worldSize; x++) for (int z = 0; z < worldSize; z++)
	{
		const int h = (heights.buffer[x + heights.width * z] & 255) + 2;
		const uint c = RGB32to16(colours.buffer[x + colours.width * z]);
		for (int y = 0; y < h - 4; y++) world.Set(x, y, z, WHITE);
		for (int y = max(0, h - 4); y < h; y++) world.Set(x, y, z, c);
		if (x % stratumSizeX == 0 && z % stratumSizeZ == 0)
		{
			float r0 = RandomFloat() * stratumSizeX - halfStratumSizeX;
			float r1 = RandomFloat() * stratumSizeZ - halfStratumSizeZ;
			int _x = clamp((int)(x + r0), 0, worldSize-1);
			int _z = clamp((int)(z + r1), 0, worldSize-1);
			uint _h = (heights.buffer[_x + heights.width * _z] & 255) + 2;
			world.Set(_x, _h, _z, BROWN);
			world.Set(_x, _h  + 1, _z, WHITE | (emittance << 12));
			world.Set(_x, _h + 2, _z, WHITE | (emittance << 12));
			world.Set(_x, _h + 3, _z, WHITE | (emittance << 12));
		}
	}
}