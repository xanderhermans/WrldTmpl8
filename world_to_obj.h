#pragma once

namespace Tmpl8
{
	void WorldToOBJ(Tmpl8::World* world, std::string filename);
	float3 ToFloatRGB(const uint v);
	uint ToUint(const float3 c);
}