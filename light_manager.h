#pragma once

namespace Tmpl8
{
	class LightManager
	{
		// <grid coord, light index>
		unordered_map<uint, uint> positionToLightsIndices;
		unordered_map<uint, uint> defaultVoxel;
		unordered_map<uint, uint4> movinglights;
	public:
		static uint3 PositionFromIteration(int3 center, uint& iteration, uint radius);
		uint NumberOfLights() { return GetWorld()->GetRenderParams().numberOfLights; }
		void SetNumberOfLights(uint numberOfLights) { GetWorld()->GetRenderParams().numberOfLights = numberOfLights; }
		uint MovingLightCount() { return movinglights.size(); }
		unordered_map<uint, uint>& GetPositionToLightIndices() { return positionToLightsIndices; }
		Light* GetLights() { return reinterpret_cast<Light*>(GetWorld()->GetLightsBuffer()->hostBuffer); }
		void FindLightsInWorld(vector<Light>& ls);
		void SetupBuffer(vector<Light>& ls, int position = 0);
		void AddLight(uint3 pos, uint3 size = make_uint3(1), uint color = WHITE | (1 << 12));
		void RemoveLight(uint3 pos);
		void AddRandomLights(int numberOfLights);
		void RemoveRandomLights(int numberOfLights);

		bool lightsAreMoving = false;
		bool poppingLights = false;
		void SetUpMovingLights(int numberOfLights);
		void MoveLights();
		void PopLights(float deltaTime);
	};
}

