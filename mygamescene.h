#pragma once

namespace Tmpl8
{
	namespace MyGameScene
	{
		int LoadWorld(World& world, std::string path);
		void CreateWorld(World& world);
		void SaveWorld(std::string path);
		void ScatterLights(World& world, int numberOfLights, int numberOfBrickLights);
	};
}

