#include "precomp.h"
#include "mygamescene.h"

using namespace MyGameScene;

void DummyWorld(World& world)
{
	int width = 256;
	int height = 128;
	int depth = 288;
	int _y = 0;
	int _ym = height;
	int _x = 0;
	int _xm = width;
	int _z = 0;
	int _zm = depth;

	uint EMIT = 1 << 15;
	uint EMIT_WHITE = WHITE | EMIT;
	uint EMIT_GREEN = GREEN | EMIT;
	uint EMIT_RED = RED | EMIT;
	uint EMIT_BLUE = BLUE | EMIT;
	uint EMIT_YELLOW = YELLOW | EMIT;
	uint EMIT_PURPLE = ORANGE | EMIT;

	for (int y = _y; y <= _ym; y++) for (int z = _z; z <= _zm; z++)
	{
		world.Set(_x, y, z, EMIT_WHITE);
		//world.Set(_xm, y, z, EMIT_GREEN);
	}
	for (int x = _x; x <= _xm; x++) for (int z = _z; z <= _zm; z++)
	{
		//world.Set(x, _y, z, EMIT_RED);
		//world.Set(x, _ym, z, EMIT_BLUE);
	}
	for (int x = _x; x <= _xm; x++) for (int y = _y; y <= _ym; y++)
	{
		//world.Set(x, y, _z, EMIT_YELLOW);
		//world.Set(x, y, _zm, EMIT_PURPLE);
	}
	// performance note: these coords prevent exessive brick traversal.
}

void MyGameScene::CreateWorld(World& world)
{
	world.Clear();
	DummyWorld(world);

	uint a = LoadSprite("assets/corvette.vx");
	StampSpriteTo(a, 96, 32, 24);
}
