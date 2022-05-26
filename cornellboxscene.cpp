#include "precomp.h"
#include "mygamescene.h"
#include <filesystem>

using namespace MyGameScene;

string small_box_path = "scene_export/cornellbox/cornell-box.vox";

#define BRICKLIGHTS 1

void MyGameScene::CreateCornellBoxWorld(World& world)
{
	world.Clear();

	int idSmall = LoadSprite(small_box_path.c_str());
	StampSpriteTo(idSmall, 0, 0, 0);

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
		world.SetBrick(x, _ym, z, WHITE | (4 << 12));
	}
#endif
}