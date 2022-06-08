#pragma once

namespace Tmpl8
{
	namespace MyGameScene
	{
		int LoadWorld(World& world, std::string path);
		void CreateWorld(World& world);
		void SaveWorld(std::string path, uint sizeX, uint sizeY, uint sizeZ);

		void CreateMountainWorld(World& world);
		void CreateLetterWorld(World& world);
		void CreateScatteredWorld(World& world);
		void CreateCornellBoxWorld(World& world);
		void DrawBox(World& world, uint x, uint y, uint z, uint max_x, uint max_y, uint max_z, uint c = WHITE);
	};
}

