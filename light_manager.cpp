#include "precomp.h"
#include "light_manager.h"

uint3 Tmpl8::LightManager::PositionFromIteration(int3 center, uint& iteration, uint radius)
{
	float angle = (float)iteration / 100 * TWOPI;
	float x1 = cos(angle) * radius;
	float x2 = sin(angle) * radius;
	int3 pos = make_int3(x1 + center.x, center.y, center.z + x2);
	pos.x = max(0, pos.x);
	pos.z = max(0, pos.z);
	return make_uint3(pos);
}

void Tmpl8::LightManager::FindLightsInWorld(vector<Light>& ls)
{
	World& world = *GetWorld();

	uint sizex = MAPWIDTH;
	uint sizey = MAPHEIGHT;
	uint sizez = MAPDEPTH;

	ls.clear();
	uint* grid = world.GetGrid();
	PAYLOAD* brick = world.GetBrick();
	uint numberOfVoxels = 0;
	uint numberOfBricks = 0;
	for (uint by = 0; by < GRIDHEIGHT; by++)
	{
		for (uint bz = 0; bz < GRIDDEPTH; bz++)
		{
			for (uint bx = 0; bx < GRIDWIDTH; bx++)
			{
				ushort vox = 0;
				const uint cellIdx = bx + bz * GRIDWIDTH + by * GRIDWIDTH * GRIDDEPTH;
				const uint g = grid[cellIdx];
				if ((g & 1) == 0) 
				{ 
					vox = g >> 1;
					bool isEmitter = EmitStrength(vox) > 0;
					if (isEmitter)
					{
						Light light;
						light.position = bx * BRICKDIM + bz * BRICKDIM * sizex + by * BRICKDIM * sizex * sizez;
						light.voxel = vox;
						light.size = BRICKDIM;
						ls.push_back(light);
						numberOfBricks++;
					}
				}
				else
				{
					for (uint ly = 0; ly < BRICKDIM; ly++)
					{
						for (uint lz = 0; lz < BRICKDIM; lz++)
						{
							for (uint lx = 0; lx < BRICKDIM; lx++)
							{
								uint _index = (g >> 1) * BRICKSIZE + lx + ly * BRICKDIM + lz * BRICKDIM * BRICKDIM;
								vox = brick[_index];
								bool isEmitter = EmitStrength(vox) > 0;
								if (isEmitter)
								{
									uint x = lx + bx * BRICKDIM;
									uint y = ly + by * BRICKDIM;
									uint z = lz + bz * BRICKDIM;
									Light light;
									light.position = x + z * sizex + y * sizex * sizez;
									light.voxel = vox;
									light.size = 1;
									ls.push_back(light);
									numberOfVoxels++;
								}
							}
						}
					}
				}
			}
		}
	}

	printf("Number of emitting voxels found in world: %d, number of voxels: %d, number of bricks: %d\n", ls.size(), numberOfVoxels, numberOfBricks);
}

void Tmpl8::LightManager::SetupBuffer(vector<Light>& ls, int position)
{
	World& w = *GetWorld();

	uint numberOfLights = ls.size() + NumberOfLights();
	Light* lights;
	if (!w.GetLightsBuffer())
	{
		uint buffersize = max((uint)1, numberOfLights * 2);
		printf("Light buffer does not exist, creating of size %d.\n", buffersize);
		Buffer* buffer = new Buffer(sizeof(Light) / 4 * buffersize, Buffer::DEFAULT, new Light[buffersize]);
		buffer->ownData = true;
		lights = reinterpret_cast<Light*>(buffer->hostBuffer);
		w.SetLightsBuffer(buffer);
	}
	else
	{
		uint lightBufferSize = w.GetLightsBuffer()->size * 4 / sizeof(Light);
		if (lightBufferSize < numberOfLights)
		{
			uint buffersize = max((uint)1, numberOfLights * 2);
			printf("Light buffer too small, resizing to %d.\n", buffersize);

			Buffer* buffer = new Buffer(sizeof(Light) / 4 * buffersize, 0, new Light[buffersize]);
			buffer->ownData = true;

			lights = reinterpret_cast<Light*>(buffer->hostBuffer);
			Light* _lights = GetLights();

			for (int i = 0; i < NumberOfLights(); i++)
			{
				lights[i] = _lights[i];
			}

			delete w.GetLightsBuffer();
			w.SetLightsBuffer(buffer);
		}
		else
		{
			lights = GetLights();
		}
	}

	// add lights
	for (int i = 0; i < ls.size(); i++)
	{
		lights[i + position] = ls[i];
	}

	SetNumberOfLights(numberOfLights);
	w.GetLightsBuffer()->CopyToDevice();
}

void Tmpl8::LightManager::AddLight(uint3 pos, uint3 size, uint color)
{
}

void Tmpl8::LightManager::RemoveLight(uint3 pos)
{
}

void Tmpl8::LightManager::AddRandomLights(int _numberOfLights)
{
	World& w = *GetWorld();

	uint numberOfLights = _numberOfLights + NumberOfLights();

	uint sizex = MAPWIDTH;
	uint sizey = MAPHEIGHT;
	uint sizez = MAPDEPTH;
	const int world_width = 256;
	const int world_height = 128;
	const int world_depth = 288;

	vector<Light> ls;
	for (int i = 0; i < _numberOfLights; i++)
	{
		uint x = RandomFloat() * world_width;
		uint y = RandomFloat() * world_height;
		uint z = RandomFloat() * world_depth;
		uint c = (uint)(RandomFloat() * ((1 << 12) - 2) + 1) | (1 << 12);
		uint index = x + z * sizex + y * sizex * sizez;
		uint prev = w.Get(x, y, z);
		if (EmitStrength(prev) == 0)
		{
			defaultVoxel[index] = prev;
		}
		w.Set(x, y, z, c);
		Light l;
		l.position = index;
		l.voxel = c;
		l.size = 1;
		ls.push_back(l);
	}

	SetupBuffer(ls, NumberOfLights());
	w.GetRenderParams().restirtemporalframe = 0;
}

void Tmpl8::LightManager::RemoveRandomLights(int _numberOfLights)
{
	World& w = *GetWorld();

	uint numberOfLights = max(0, (int)NumberOfLights() - (int)_numberOfLights);
	if (!w.GetLightsBuffer())
	{
		printf("Light buffer does not exist.\n");
		return;
	}
	if (NumberOfLights() < 1)
	{
		printf("No lights to remove.\n");
		return;
	}

	vector<uint> indices;
	for (int i = 0; i < NumberOfLights(); i++) indices.push_back(i);
	unordered_set<uint> toRemove;
	for (int i = 0; i < _numberOfLights; i++)
	{
		if (indices.size() == 0) break;
		int index = RandomFloat() * (indices.size() - 1);
		toRemove.insert(indices[index]);
		indices.erase(indices.begin() + index);
	}

	Light* lights = GetLights();
	uint lightBufferSize = w.GetLightsBuffer()->size * 4 / sizeof(Light);

	Light* newlights;
	uint newLightBufferSize = max((uint)1, numberOfLights * 2);
	Buffer* newLightBuffer;
	if (newLightBufferSize < lightBufferSize / 2)
	{
		printf("Resizing buffer to %d\n", newLightBufferSize);
		Buffer* buffer = new Buffer(sizeof(Light) / 4 * newLightBufferSize, 0, new Light[newLightBufferSize]);
		buffer->ownData = true;
		newlights = reinterpret_cast<Light*>(buffer->hostBuffer);
		newLightBuffer = buffer;
	}
	else
	{
		newLightBuffer = w.GetLightsBuffer();
		newlights = new Light[lightBufferSize];
	}

	uint bufferi = 0;
	for (int i = 0; i < NumberOfLights(); i++)
	{
		if (toRemove.find(i) == toRemove.end())
		{
			newlights[bufferi++] = lights[i];
		}
	}

	if (w.GetLightsBuffer() == newLightBuffer)
	{
		delete[] lights;
		w.GetLightsBuffer()->hostBuffer = reinterpret_cast<uint*>(newlights);
	}
	else
	{
		delete w.GetLightsBuffer();
		w.SetLightsBuffer(newLightBuffer);
	}

	w.GetRenderParams().restirtemporalframe = 0;
	SetNumberOfLights(numberOfLights);
	w.GetLightsBuffer()->CopyToDevice();

	for (const uint i : toRemove)
	{
		uint index = lights[i].position;
		const uint y = index / (MAPWIDTH * MAPDEPTH);
		const uint z = (index / MAPWIDTH) % MAPDEPTH;
		const uint x = index % MAPDEPTH;
		uint color = 0;
		if (defaultVoxel.find(index) != defaultVoxel.end())
		{
			color = defaultVoxel[index];
		}
		w.Set(x, y, z, color);
		if (movinglights.find(i) != movinglights.end())
		{
			movinglights.erase(i);
		}
	}
}

void Tmpl8::LightManager::SetUpMovingLights(int _numberOfMovingLights)
{
	movinglights.clear();
	RenderParams& renderparams = GetWorld()->GetRenderParams();
	int numberOfMovingLights = min((uint)_numberOfMovingLights, renderparams.numberOfLights);

	vector<uint> indices;
	for (int i = 0; i < renderparams.numberOfLights; i++) indices.push_back(i);

	for (int i = 0; i < numberOfMovingLights; i++)
	{
		int index = RandomFloat() * (indices.size() - 1);
		uint lightIndex = indices[index];
		Light light = GetLights()[lightIndex];
		const uint y = light.position / (MAPWIDTH * MAPDEPTH);
		const uint z = (light.position / MAPWIDTH) % MAPDEPTH;
		const uint x = light.position % MAPDEPTH;
		movinglights.insert({ lightIndex, make_uint4(x, y, z, RandomUInt() % 1000) });
		indices.erase(indices.begin() + index);
	}
	printf("%d lights registered for moving\n", movinglights.size());
}

void Tmpl8::LightManager::MoveLights()
{
	World& w = *GetWorld();
	Light* lights = GetLights();

	for (auto& ml : movinglights)
	{
		uint index = ml.first;
		int3 center = make_int3(make_uint3(ml.second));
		uint& iteration = ml.second.w;

		Light& l = lights[index];
		uint3 pos = PositionFromIteration(center, iteration, 15);

		const uint oldy = l.position / (MAPWIDTH * MAPDEPTH);
		const uint oldz = (l.position / MAPWIDTH) % MAPDEPTH;
		const uint oldx = l.position % MAPDEPTH;
		iteration = (iteration + 1) % 1000;
		uint sizex = MAPWIDTH;
		uint sizey = MAPHEIGHT;
		uint sizez = MAPDEPTH;
		uint lightposition = pos.x + pos.z * sizex + pos.y * sizex * sizez;
		uint voxel = w.Get(pos.x, pos.y, pos.z);
		if (EmitStrength(voxel) == 0)
		{
			defaultVoxel[lightposition] = voxel;
		}
		uint color = 0;
		if (defaultVoxel.find(l.position) != defaultVoxel.end())
		{
			color = defaultVoxel[l.position];
		}
		w.Set(oldx, oldy, oldz, color);
		w.Set(pos.x, pos.y, pos.z, l.voxel);
		l.position = lightposition;
	}

	// spatial hides the incorrect temporal albedo artifacts
	if (!w.GetRenderParams().spatial) w.GetRenderParams().restirtemporalframe = 0;
	w.GetLightsBuffer()->CopyToDevice();
}

void Tmpl8::LightManager::PopLights(float deltaTime)
{
	const double frametime = 500;
	static double lasttime = 0;

	if (lasttime > frametime)
	{
		if (GetWorld()->GetRenderParams().numberOfLights >= 500)
		{
			RemoveRandomLights(500);
			AddRandomLights(500);
		}
		else
		{
			AddRandomLights(500);
		}
		lasttime = 0;
	}
	else
	{
		lasttime += deltaTime;
	}
}
