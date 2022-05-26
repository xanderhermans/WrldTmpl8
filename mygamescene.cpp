#include "precomp.h"
#include "mygamescene.h"
#include <filesystem>

using namespace MyGameScene;
const int world_width = 256;
const int world_height = 128;
const int world_depth = 288;

void MyGameScene::DrawBox(World& world, uint _x, uint _y, uint _z, uint _xm, uint _ym, uint _zm, uint c)
{
	for (int y = _y; y <= _ym; y++) for (int z = _z; z <= _zm; z++)
	{
		world.Set(_x, y, z, c);
		world.Set(_xm, y, z, c);
	}
	for (int x = _x; x <= _xm; x++) for (int z = _z; z <= _zm; z++)
	{
		world.Set(x, _y, z, c);
		world.Set(x, _ym, z, c);
	}
	for (int x = _x; x <= _xm; x++) for (int y = _y; y <= _ym; y++)
	{
		world.Set(x, y, _zm, c);
	}
}

void DummyWorld(World& world)
{
	int _y = 0;
	int _ym = world_height;
	int _x = 0;
	int _xm = world_width;
	int _z = 0;
	int _zm = world_depth;

	uint EMIT = 1 << 15;
	uint EMIT_WHITE = WHITE | EMIT;
	uint EMIT_GREEN = GREEN | EMIT;
	uint EMIT_RED = RED | EMIT;
	uint EMIT_BLUE = BLUE | EMIT;
	uint EMIT_YELLOW = YELLOW | EMIT;
	uint EMIT_ORANGE = ORANGE | EMIT;

	array color_strenth{0.5f, 0.5f, 0.5f, 1.0f, 1.0f, 1.0f, 2.0f, 2.0f, 8.0f, 15.0f};
	auto get_voxel_color_random = [&](uint emit_color, float probability)
	{
		float randomfloat = RandomFloat();
		if (randomfloat <= probability)
		{
			float3 colr = ToFloatRGB(emit_color);
			randomfloat = RandomFloat() * (color_strenth.size() - 1);
			float strength = 1;// color_strenth[int(randomfloat)];
			uint emit = uint(max(1.0f, strength)) << 12;
			uint emit_col = emit_color | emit;
			return emit_col;
		}
		return (uint)0;
	};

#if 1 // BOX
	DrawBox(world, 0, 0, 0, _xm, _ym, _zm);
#endif

#if 1 //RANDOM SMALL LIGHTS
	const int offset = 10;
	int count = 0;

	for (int y = _y; y <= _ym; y++) for (int z = _z; z <= _zm; z++)
	{
		uint col1 = get_voxel_color_random(WHITE, 0.1);
		if (col1 > 0)
		{
			world.Set(_x + offset, y, z, col1);
			count++;
		}
		uint col2 = get_voxel_color_random(GREEN, 0.1);
		if (col2 > 0)
		{
			world.Set(_xm - offset, y, z, col2);
			count++;
		}
	}
	for (int x = _x; x <= _xm; x++) for (int y = _y; y <= _ym; y++)
	{
		world.Set(x, y, _zm, WHITE);
		uint col1 = get_voxel_color_random(ORANGE, 0.1);
		if (col1 > 0)
		{
			world.Set(x, y, _zm - offset, col1);
			count++;
		}
	}
#endif


#if 0 //TWO AREA LIGHTS
	for (int y = 48; y <= 64; y++) for (int z = 48; z <= 64; z++)
	{
		world.Set(_x, y, z, WHITE | 0x4000);
	}

	for (int x = 48; x <= 64; x++) for (int y = 48; y <= 64; y++)
	{
		world.Set(x, y, _z, WHITE | 0x4000);
	}
#endif
}

void MyGameScene::SaveWorld(std::string filename, uint sizeX, uint sizeY, uint sizeZ)
{
	filesystem::path path = filename;
	if (path.has_parent_path())
	{
		filesystem::create_directories(path.parent_path());
	}

	uint idx = CreateSprite(make_int3(0), make_int3(sizeX, sizeY, sizeZ), 1);
	SaveSprite(idx, path.string().c_str());
}

int MyGameScene::LoadWorld(World& world, std::string filename)
{
	filesystem::path path = filename;
	if (!filesystem::exists(path))
	{
		return -1;
	}
	uint idx = LoadSprite(filename.c_str());

	world.Clear();
	StampSpriteTo(idx, make_int3(0));
	return idx;
}

void MyGameScene::CreateWorld(World& world)
{
	world.Clear();
	DummyWorld(world);

	uint a = LoadSprite("assets/corvette.vx");
	StampSpriteTo(a, 96, 8, 24);
}
