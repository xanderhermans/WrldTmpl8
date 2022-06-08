#include "precomp.h"
#include "mygamescene.h"
#include <filesystem>

using namespace MyGameScene;

void MyGameScene::CreateLetterWorld(World& world)
{
	world.Clear();

	DrawBox(world, 0, 0, 0, 256, 128, 288);
	uint a = LoadSprite("assets/corvette.vx");
	StampSpriteTo(a, 96, 8, 24);

	for (int i = 32; i < 256; i += 32)
	{
		world.Print(to_string(i).c_str(), 256 - 64, 0, i, BLUE + i | (4 << 12));
	}

	static char f[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890+-=/\\*:;()[]{}<>!?.,'\"&#$_%^|";
	for (int j = 16; j < 128; j += 32)
	{
		for (int i = 32; i < 256; i += 32)
		{
			uint color = WHITE - ((i / 32) + 7 * (j / 32)) * 13;
			uint emit = 15 << 12;
			world.Print(string{ f[((i / 32) + 7 * (j / 32))] }.c_str(), i, j, 288, color | emit);
		}
	}

	for (int j = 16; j < 128; j += 32)
	{
		for (int i = 32; i < 256; i += 32)
		{
			uint color = WHITE - ((i / 32) + 7 * (j / 32)) * 13;
			uint emit = 15 << 12;
			world.PrintZ(string{ f[((i / 32) + 7 * (j / 32))] }.c_str(), 256, j, i, color | emit);
		}
	}

	world.PrintZ("Voxel ReSTIR", 16, 32, 96, YELLOW | (8 << 12));
}