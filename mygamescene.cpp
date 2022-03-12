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
	uint EMIT_ORANGE = ORANGE | EMIT;

	array color_strenth{0.5f, 0.5f, 0.5f, 1.0f, 1.0f, 1.0f, 2.0f, 2.0f, 8.0f, 15.0f};
	auto set_voxel_emitter = [&](int x, int y, int z, uint color, uint emit_color, float probability)
	{
		float randomfloat = RandomFloat();
		if (randomfloat <= probability)
		{
			float3 colr = ToFloatRGB(emit_color);
			randomfloat = RandomFloat() * (color_strenth.size() - 1);
			float strength = 4;// color_strenth[int(randomfloat)];
			uint emit = uint(max(1.0f, strength)) << 12;
			uint emit_col = emit_color | emit;
			world.Set(x, y, z, emit_col);
		}
		else
		{
			world.Set(x, y, z, color);
		}
	};

	for (int y = _y; y <= _ym; y++) for (int z = _z; z <= _zm; z++)
	{
		set_voxel_emitter(_x, y, z, WHITE, WHITE, 0.1);
		set_voxel_emitter(_xm, y, z, WHITE, GREEN, 0.1);
	}
	for (int x = _x; x <= _xm; x++) for (int z = _z; z <= _zm; z++)
	{
		world.Set(x, _y, z, WHITE);
		//world.Set(x, _ym, z, EMIT_BLUE);
	}
	for (int x = _x; x <= _xm; x++) for (int y = _y; y <= _ym; y++)
	{
		//world.Set(x, y, _z, EMIT_YELLOW);
		//world.Set(x, y, _zm, EMIT_ORANGE);
		set_voxel_emitter(x, y, _zm, WHITE, ORANGE, 0.1);
	}
}

void MyGameScene::CreateWorld(World& world)
{
	world.Clear();
	DummyWorld(world);

	uint a = LoadSprite("assets/corvette.vx");
	StampSpriteTo(a, 96, 8, 24);

	//world.Clear();
	//uint sizex = 6;
	//uint sizey = 7;
	//uint sizez = 8;
	//for (uint y = 0; y < sizey; y++)
	//{
	//	for (uint z = 0; z < sizez; z++)
	//	{
	//		for (uint x = 0; x < sizex; x++)
	//		{
	//			world.Set(x, y, z, WHITE | (1 << 15));
	//		}
	//	}
	//}

	//for (int i = 0; i < sizex; i++)
	//{
	//	world.Set(i, 0, 0, RED | (1 << 15));
	//}
	//for (int i = 0; i < sizey; i++)
	//{
	//	world.Set(0, i, 0, GREEN | (1 << 15));
	//}
	//for (int i = 0; i < sizez; i++)
	//{
	//	world.Set(0, 0, i, BLUE | (1 << 15));
	//}
}
