#include "precomp.h"
#include "mygame.h"
#include "mygamescene.h"
#include <filesystem>
#include <unordered_map>
#include <unordered_set>
#include <functional>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "lib/stb_image_write.h"

Game* CreateGame() { return new MyGame(); }

// -name scene -obj ../scene/scene.obj 1.0 trad -inst scene -cam-pitch 12.70 -cam-yaw -6.47 -cam-roll 0.00 -cam-pos -107.85 60.81 -48.86
// -cam-pitch 12.70 -cam-yaw -6.47 -cam-roll 0.00 -cam-pos -107.85 60.81 -48.86
//float3 _D(0, 0, 1), _O(107.85, 60.81, -48.86);
float3 _D(0.11, -0.22, 0.97), _O(107.85, 60.81, -48.86);
float3 D = _D, O = _O;

static bool shouldDumpBuffer = false;
static bool takeScreenshot = false;
static bool useSpatialResampling = USESPATIAL;
static bool useTemporalResampling = USETEMPORAL;

static unordered_map<string, void*> commands;
static unordered_map<string, function<void(MyGame, string)>> functionCommands;
static unordered_map<uint, uint> lightIndices;
static unordered_map<uint, uint> defaultVoxel;
static unordered_map<uint, uint4> movinglights;
static bool lightsAreMoving = false;
static bool discoLights = false;

uint3 positionFromIteration(int3 center, uint& iteration, uint radius);

// -----------------------------------------------------------
// Initialize the application
// -----------------------------------------------------------
void MyGame::Init()
{
	string import_path = "scene_export/grid.vx";
	string export_path = "scene_export/scene";

	int loadedworld = MyGameScene::LoadWorld(*GetWorld(), import_path);
	if (loadedworld < 0)
	{
		MyGameScene::CreateWorld(*GetWorld());
		MyGameScene::SaveWorld(import_path);
	}

	if (!filesystem::exists(export_path + ".obj"))
	{
		WorldToOBJ(GetWorld(), export_path);
	}

	RenderParams& params = GetWorld()->GetRenderParams();
	params.accumulate = false;
	params.spatial = useSpatialResampling;
	params.temporal = useTemporalResampling;
	params.spatialTaps = SPATIALTAPS;
	params.spatialRadius = SPATIALRADIUS;
	params.numberOfCandidates = NUMBEROFCANDIDATES;
	params.numberOfMaxTemporalImportance = TEMPORALMAXIMPORTANCE;
	GetWorld()->GetDebugInfo().counter = 0;

	commands.insert({ "spatialtaps", &params.spatialTaps });
	commands.insert({ "spatialradius", &params.spatialRadius });
	commands.insert({ "numberofcandidates", &params.numberOfCandidates });
	commands.insert({ "temporalimportance", &params.numberOfMaxTemporalImportance });
	commands.insert({ "spatial", &params.spatial });
	commands.insert({ "temporal", &params.temporal });
	functionCommands.insert({ "addlights", [](MyGame _1, string _2) {IntArgFunction([](MyGame g, int a) {g.AddRandomLights(a); }, _1, _2, 2500); } });
	functionCommands.insert({ "removelights", [](MyGame _1, string _2) {IntArgFunction([](MyGame g, int a) {g.RemoveRandomLights(a); }, _1, _2, 2500); } });
	functionCommands.insert({ "movelightcount", [](MyGame _1, string _2) {IntArgFunction([](MyGame g, int a) {g.SetUpMovingLights(a); }, _1, _2, 2500); } });

	SetupLightBuffers();
	SetupReservoirBuffers();
}

void MyGame::SetupReservoirBuffers()
{
	World& world = *GetWorld();

	Buffer* reservoirbuffer = world.GetReservoirsBuffer()[0];
	const int numberOfReservoirs = SCRWIDTH * SCRHEIGHT;
	if (!reservoirbuffer)
	{
		reservoirbuffer = new Buffer(sizeof(Reservoir) / 4 * numberOfReservoirs, 0, new Reservoir[numberOfReservoirs]);
		world.SetReservoirBuffer(reservoirbuffer, 0);
	}

	Buffer* prevReservoirbuffer = world.GetReservoirsBuffer()[1];
	if (!prevReservoirbuffer)
	{
		prevReservoirbuffer = new Buffer(sizeof(Reservoir) / 4 * numberOfReservoirs, 0, new Reservoir[numberOfReservoirs]);
		world.SetReservoirBuffer(prevReservoirbuffer, 1);
	}
}

void MyGame::SetupLightBuffers()
{
	World& world = *GetWorld();

	uint sizex = MAPWIDTH;
	uint sizey = MAPHEIGHT;
	uint sizez = MAPDEPTH;

	vector<Light> lights;

	for (uint y = 0; y < sizey; y++)
	{
		for (uint z = 0; z < sizez; z++)
		{
			for (uint x = 0; x < sizex; x++)
			{
				ushort c = world.Get(x, y, z);
				bool isEmitter = EmitStrength(c) > 0;
				if (isEmitter)
				{
					Light light;
					light.position = x + z * sizex + y * sizex * sizez;
					light.voxel = c;
					lights.push_back(light);
				}
			}
		}
	}

	uint numberOfLights = lights.size();
	Buffer* lightbuffer = world.GetLightsBuffer();
	uint lightbuffersize = max(numberOfLights, uint(10)) * 2;
	if (!lightbuffer)
	{
		printf("Light buffer does not exist, creating.\n");
		lightbuffer = new Buffer(sizeof(Light) / 4 * lightbuffersize, Buffer::DEFAULT, new Light[lightbuffersize]);
		lightbuffer->ownData = true;
	}
	else if (lightbuffer->size < numberOfLights)
	{
		printf("Light buffer too small, resizing.\n");
		delete lightbuffer;
		lightbuffer = new Buffer(sizeof(Light) / 4 * lightbuffersize, 0, new Light[lightbuffersize]);
		lightbuffer->ownData = true;
	}

	printf("Number of emitting voxels: %d\n", numberOfLights);

	Light* lightsData = (Light*)lightbuffer->hostBuffer;
	for (int i = 0; i < lights.size(); i++)
	{
		auto light = lights[i];
		lightsData[i] = light;
		lightIndices[light.position] = i;
	}

	world.GetRenderParams().numberOfLights = numberOfLights;
	lightbuffer->CopyToDevice();
	world.SetLightsBuffer(lightbuffer);
}

KeyHandler qHandler = { 0, 'Q' };
KeyHandler eHandler = { 0, 'E' };
KeyHandler spaceHandler = { 0, VK_SPACE };
KeyHandler leftHandler = { 0, VK_LEFT };
KeyHandler rightHandler = { 0, VK_RIGHT };
KeyHandler upHandler = { 0, VK_UP };
KeyHandler downHandler = { 0, VK_DOWN };
KeyHandler cHandler = { 0, 'C' };
KeyHandler vHandler = { 0, 'V' };
KeyHandler nHandler = { 0, 'N' };
KeyHandler mHandler = { 0, 'M' };
KeyHandler wHandler = { 0, 'W' };
KeyHandler sHandler = { 0, 'S' };
KeyHandler aHandler = { 0, 'A' };
KeyHandler dHandler = { 0, 'D' };
KeyHandler rHandler = { 0, 'R' };
KeyHandler fHandler = { 0, 'F' };
KeyHandler zHandler = { 0, 'Z' };
KeyHandler lHandler = { 0, 'L' };
KeyHandler xHandler = { 0, 'X' };
KeyHandler inputKeyHandler = { 0, 'I' };
KeyHandler uHandler = { 0, 'U' };
KeyHandler oHandler = { 0, 'O' };
void MyGame::HandleControls(float deltaTime)
{
	// free cam controls
	float3 tmp(0, 1, 0), right = normalize(cross(tmp, D)), up = cross(D, right);
	float speed = deltaTime * 0.03f;
	bool dirty = false;
	RenderParams& renderparams = GetWorld()->GetRenderParams();
	World& w = *GetWorld();

	if (inputKeyHandler.IsTyped() && (ConsoleHasFocus() || isFocused))
	{
		if (isFocused)
		{
			BOOL ret = SetForegroundWindow(GetConsoleWindow());
		}
		printf("input:");
		string input;
		getline(cin, input);
		stringstream ss(input);
		char delim = ' ';
		vector<string> words;
		string word;
		while (getline(ss, word, delim))
		{
			words.push_back(word);
		}
		bool success = false;
		if (words.size() > 0)
		{
			if (words.size() > 1)
			{
				if (commands.find(words[0]) != commands.end())
				{
					int result;
					success = string_to <int>(words[1], result);
					if (success)
					{
						int* address = reinterpret_cast<int*>(commands[words[0]]);
						*address = result;
						//printf("%s %d\n", words[0].c_str(), *address);
					}
				}
				else if (functionCommands.find(words[0]) != functionCommands.end())
				{
					functionCommands[words[0]](*this, words[1]);
					success = true;
				}
			}
			else if (words[0] == "help")
			{
				stringstream ss;
				for (auto& it : commands)
				{
					ss << it.first << ", ";
				}
				for (auto& it : functionCommands)
				{
					ss << it.first << ", ";
				}
				printf("%s\n", ss.str().substr(0, ss.str().size() - 2).c_str());
				success = true;
			}
			else if (functionCommands.find(words[0]) != functionCommands.end())
			{
				functionCommands[words[0]](*this, "");
				success = true;
			}
		}
		if (!success)
		{
			printf("Command not recognized. Write `help` to get a list of commands\n");
		}
		// ignore rest of input
		return;
	}

	if (!isFocused) return; // ignore controls if window doesnt have focus
	if (qHandler.IsTyped()) { shouldDumpBuffer = true; printf("Dumping screenbuffer queued.\n"); }
	if (eHandler.IsTyped()) { takeScreenshot = true; printf("Next dump is screenshot.\n"); }

	if (wHandler.isPressed()) { O += speed * D; dirty = true; }
	else if (sHandler.isPressed()) { O -= speed * D; dirty = true; }
	if (aHandler.isPressed()) { O -= speed * right; dirty = true; }
	else if (dHandler.isPressed()) { O += speed * right; dirty = true; }

	if (rHandler.isPressed()) { O += speed * up; dirty = true; }
	else if (fHandler.isPressed()) { O -= speed * up; dirty = true; }

	if (leftHandler.isPressed()) { D = normalize(D - right * 0.025f * speed); dirty = true; }
	else if (rightHandler.isPressed()) { D = normalize(D + right * 0.025f * speed); dirty = true; }
	if (upHandler.isPressed()) { D = normalize(D - up * 0.025f * speed); dirty = true; }
	else if (downHandler.isPressed()) { D = normalize(D + up * 0.025f * speed); dirty = true; }

	if (zHandler.isPressed()) { D = _D; O = _O; dirty = true; }

	if (uHandler.IsTyped())
	{
		lightsAreMoving = !lightsAreMoving;
		if (movinglights.size() < 1)
		{
			SetUpMovingLights(100);
		}
	}
	if (oHandler.IsTyped())
	{
		discoLights = !discoLights;
	}

	if (lHandler.isPressed()) { PrintStats(); };
	if (cHandler.IsTyped())
	{
		dirty = true;
		useSpatialResampling = !useSpatialResampling;
		renderparams.spatial = useSpatialResampling;
	}
	if (vHandler.IsTyped())
	{
		dirty = true;
		useTemporalResampling = !useTemporalResampling;
		renderparams.temporal = useTemporalResampling;
	}
	if (nHandler.IsTyped())
	{
		dirty = true;
		renderparams.numberOfMaxTemporalImportance = max(0, (int)renderparams.numberOfMaxTemporalImportance - 1);
	}
	if (mHandler.IsTyped())
	{
		dirty = true;
		renderparams.numberOfMaxTemporalImportance = renderparams.numberOfMaxTemporalImportance + 1;
	}

	LookAt(O, O + D);

	if (spaceHandler.IsTyped())
	{
		renderparams.accumulate = !renderparams.accumulate;
		dirty = true;
	}
	if (xHandler.isPressed())
	{
		dirty = true;
	}
	if (dirty)
	{
		renderparams.framecount = 0;
		renderparams.frame = 0;
	}
}

void MyGame::PreRender()
{
}

void MyGame::PrintStats()
{
	// distance to screen is 2.2 and we determine fov w.r.t. height to keep it aspect ratio invariant
	float fov = atanf(1 / 2.2f) * 360 * INVPI;

	mat4& cammat = GetWorld()->GetCameraMatrix();
	float3& angles = cammat.EulerAngles() * 180 * INVPI;

	printf("-cam-pos %.2f %.2f %.2f -cam-pitch %.2f -cam-yaw %.2f -cam-roll %.2f fov %.4f\n", -O.x, O.y, O.z, angles.x, angles.y, angles.z, fov);
	printf("Camera at _D(%.2f, %.2f, %.2f), _O(%.2f, %.2f, %.2f); fov %.2f\n", D.x, D.y, D.z, O.x, O.y, O.z, fov);
}

void MyGame::PrintDebug()
{
	clWaitForEvents(1, &GetWorld()->GetRenderDoneEventHandle());
	GetWorld()->GetDebugBuffer()->CopyFromDevice();
	DebugInfo* debugInfo = reinterpret_cast<DebugInfo*>(GetWorld()->GetDebugBuffer()->GetHostPtr());
	Reservoir res = debugInfo->res; Reservoir res1 = debugInfo->res1; Reservoir res2 = debugInfo->res2; Reservoir res3 = debugInfo->res3;
	float4 f1 = debugInfo->f1; float4 f2 = debugInfo->f2;
	RenderParams params = GetWorld()->GetRenderParams();
	//printf("res %f %d %d %f\n", res.sumOfWeights, res.streamLength, res.lightIndex, res.adjustedWeight);
	//printf("res %f %d %d %f\n", res1.sumOfWeights, res1.streamLength, res1.lightIndex, res1.adjustedWeight);
	//printf("res %f %d %d %f\n", res2.sumOfWeights, res2.streamLength, res2.lightIndex, res2.adjustedWeight);
	//printf("res %f %d %d %f\n", res3.sumOfWeights, res3.streamLength, res3.lightIndex, res3.adjustedWeight);
	//printf("res %f %f %f %f\n", f1.x, f1.y, f1.z, f1.w);
	//if (f1.x != f1.y) printf("res %f %f %f %f\n", f1.x, f1.y, f1.z, f1.w);
	//printf("res %f %f %f %f\n", f2.x, f2.y, f2.z, f2.w);
	//printf("%d\n", debugInfo->counter);
	//printf("%d\n", GetWorld()->GetRenderParams().framecount);
	printf("\n");
	debugInfo->counter = 0;
	GetWorld()->GetDebugBuffer()->CopyToDevice();
}

// -----------------------------------------------------------
// Main application tick function
// -----------------------------------------------------------
void MyGame::Tick(float deltaTime)
{
	HandleControls(deltaTime);
	RenderParams& renderparams = GetWorld()->GetRenderParams();
	// clear line
	//printf("                                                            \r");
	//printf("temporal frames %d\r", renderparams.numberOfMaxTemporalImportance);
	//printf("Frame: %d acc:%d sp:%d coord x:%.2f y:%.2f z:%.2f\r", GetWorld()->GetRenderParams().framecount, GetWorld()->GetRenderParams().accumulate, useSpatialResampling, O.x, O.y, O.z);

	//PrintDebug();
	//PrintStats();
	DumpScreenBuffer();
	if (lightsAreMoving)
	{
		MoveLights();
	}
	if (discoLights)
	{
		const double frametime = 150;
		static double lasttime = 0;

		if (lasttime > frametime)
		{
			if (GetWorld()->GetRenderParams().numberOfLights >= 2500)
			{
				RemoveRandomLights(2500);
				AddRandomLights(2500);
			}
			else
			{
				AddRandomLights(2500);
			}
			lasttime = 0;
		}
		else
		{
			lasttime += deltaTime;
		}
	}
}

void MyGame::SetUpMovingLights(int _numberOfMovingLights)
{
	movinglights.clear();
	RenderParams& renderparams = GetWorld()->GetRenderParams();
	World& w = *GetWorld();
	int numberOfMovingLights = min((uint)_numberOfMovingLights, renderparams.numberOfLights);
	w.GetLightsBuffer()->CopyFromDevice();
	Light* lights = reinterpret_cast<Light*> (w.GetLightsBuffer()->hostBuffer);

	vector<uint> indices;
	for (int i = 0; i < renderparams.numberOfLights; i++) indices.push_back(i);

	for (int i = 0; i < numberOfMovingLights; i++)
	{
		int index = RandomFloat() * (indices.size() - 1);
		uint lightIndex = indices[index];
		Light light = lights[lightIndex];
		const uint y = light.position / (MAPWIDTH * MAPDEPTH);
		const uint z = (light.position / MAPWIDTH) % MAPDEPTH;
		const uint x = light.position % MAPDEPTH;
		movinglights.insert({ lightIndex, make_uint4(x, y, z, RandomUInt() % 1000) });
		indices.erase(indices.begin() + index);
	}
	printf("%d lights registered for moving\n", movinglights.size());
}

void MyGame::MoveLights()
{
	World& w = *GetWorld();
	w.GetLightsBuffer()->CopyFromDevice();
	Light* lights = reinterpret_cast<Light*>(w.GetLightsBuffer()->hostBuffer);

	for (auto& ml : movinglights)
	{
		uint index = ml.first;
		int3 center = make_int3(make_uint3(ml.second));
		uint& iteration = ml.second.w;

		Light& l = lights[index];
		uint3 pos = positionFromIteration(center, iteration, 15);

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

union convertor {
	uint asInt;
	float asFloat;
};

void saveScreenBuffer(const std::filesystem::path& filepath, uint32_t width, uint32_t height, const float4* data);
void MyGame::DumpScreenBuffer()
{
#if STRATIFIEDACCUMULATING && ACCUMULATOR //stratified
	if (GetWorld()->GetRenderParams().framecount % 256 != 0)
	{
		return;
	}
#endif
	if (shouldDumpBuffer)
	{
		printf("Dumping screenbuffer\n");
		shouldDumpBuffer = false;

		clWaitForEvents(1, &GetWorld()->GetRenderDoneEventHandle());
		auto bufferobject = GetWorld()->GetAccumulatorBuffer();
		bufferobject->CopyFromDevice();
		if (bufferobject->size != SCRHEIGHT * SCRWIDTH * 4)
		{
			printf("Incorrect frame buffer size. cannot dump.\n");
			return;
		}
		uint* buf = bufferobject->hostBuffer;

		int width = SCRWIDTH;
		int height = SCRHEIGHT;
		float4* buffer = new float4[SCRWIDTH * SCRHEIGHT];
		uint32_t* _buffer = new uint32_t[SCRWIDTH * SCRHEIGHT];

		for (int y = 0; y < SCRHEIGHT; y++)
		{
			for (int x = 0; x < SCRWIDTH; x++)
			{
				float _c[4];
				for (int i = 0; i < 4; i++)
				{
					convertor c;
					c.asInt = buf[x * 4 + i + y * SCRWIDTH * 4];
					_c[i] = c.asFloat;
				}

				float4 val = make_float4(_c[0], _c[1], _c[2], _c[3]);
				buffer[x + y * SCRWIDTH] = val;

				uint32_t& dst = _buffer[y * width + x];
				dst = ((std::min<uint32_t>(static_cast<uint32_t>(val.x * 255), 255) << 0) |
					(std::min<uint32_t>(static_cast<uint32_t>(val.y * 255), 255) << 8) |
					(std::min<uint32_t>(static_cast<uint32_t>(val.z * 255), 255) << 16) | 255 << 24);
			}
		}

		if (takeScreenshot)
		{
			stbi_write_png("screenbufferdata/tmpl8/output.png", width, height, 4, _buffer,
				width * sizeof(uint32_t));
		}
		saveScreenBuffer("screenbufferdata/tmpl8/output.dat", SCRWIDTH, SCRHEIGHT, buffer);

		delete[] buffer;
		delete[] _buffer;

		takeScreenshot = false;
	}
}

void saveScreenBuffer(const std::filesystem::path& filepath, uint32_t width, uint32_t height, const float4* data)
{
	using namespace std;
	auto t = std::time(nullptr);
	auto tm = *std::localtime(&t);
	ostringstream oss;
	oss << put_time(&tm, "_%Y-%m-%d_%H-%M-%S");
	filesystem::path _filepath = filepath.parent_path().string() + "/" + filepath.stem().string() + oss.str() + filepath.extension().string();
	ofstream wf(_filepath.string().c_str(), ios::out | ios::binary);
	if (!wf)
	{
		printf("Error opening file %s\n", _filepath.string().c_str());
		return;
	}

	wf.write((char*)&width, sizeof(uint32_t));
	wf.write((char*)&height, sizeof(uint32_t));

	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			const float4 val = data[x + y * width];
			wf.write((char*)&val, sizeof(float4));
		}
	}

	wf.close();
	if (!wf.good()) {
		printf("Error closing file %s\n", _filepath.string().c_str());
	}
}

void MyGame::IntArgFunction(function<void(MyGame, int)> fn, MyGame g, string s, int defaultarg)
{
	int result;
	if (s != "" && string_to <int>(s, result))
	{
		fn(g, result);
	}
	else
	{
		fn(g, defaultarg);
	}
}

void MyGame::AddRandomLights(int _numberOfLights)
{
	World& w = *GetWorld();
	RenderParams& params = w.GetRenderParams();

	uint lightBufferSize = w.GetLightsBuffer()->size * 4 / sizeof(Light);
	uint numberOfLights = _numberOfLights + params.numberOfLights;
	Light* lights;
	if (!w.GetLightsBuffer())
	{
		printf("Light buffer does not exist, creating.\n");
		Buffer* buffer = new Buffer(sizeof(Light) / 4 * numberOfLights * 2, Buffer::DEFAULT, new Light[numberOfLights * 2]);
		buffer->ownData = true;
		lights = reinterpret_cast<Light*>(buffer->hostBuffer);
		w.SetLightsBuffer(buffer);
	}
	else if (lightBufferSize < numberOfLights)
	{
		printf("Light buffer too small, resizing to %d.\n", numberOfLights * 2);

		Buffer* buffer = new Buffer(sizeof(Light) / 4 * numberOfLights * 2, 0, new Light[numberOfLights * 2]);
		buffer->ownData = true;

		w.GetLightsBuffer()->CopyFromDevice();
		lights = reinterpret_cast<Light*>(buffer->hostBuffer);
		Light* _lights = reinterpret_cast<Light*>(w.GetLightsBuffer()->hostBuffer);

		for (int i = 0; i < params.numberOfLights; i++)
		{
			lights[i] = _lights[i];
		}

		delete w.GetLightsBuffer();
		w.SetLightsBuffer(buffer);
	}
	else
	{
		w.GetLightsBuffer()->CopyFromDevice();
		lights = reinterpret_cast<Light*>(w.GetLightsBuffer()->hostBuffer);
	}

	uint sizex = MAPWIDTH;
	uint sizey = MAPHEIGHT;
	uint sizez = MAPDEPTH;
	const int world_width = 256;
	const int world_height = 128;
	const int world_depth = 288;

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
		lights[params.numberOfLights + i] = l;
	}

	params.restirtemporalframe = 0;
	params.numberOfLights = numberOfLights;
	w.GetLightsBuffer()->CopyToDevice();
}

void MyGame::RemoveRandomLights(int _numberOfLights)
{
	World& w = *GetWorld();
	RenderParams& params = w.GetRenderParams();

	uint numberOfLights = params.numberOfLights - _numberOfLights;
	if (!w.GetLightsBuffer())
	{
		printf("Light buffer does not exist.\n");
		return;
	}
	if (params.numberOfLights < 1)
	{
		printf("No lights to remove.\n");
		return;
	}

	vector<uint> indices;
	for (int i = 0; i < params.numberOfLights; i++) indices.push_back(i);
	unordered_set<uint> toRemove;
	for (int i = 0; i < _numberOfLights; i++)
	{
		int index = RandomFloat() * (indices.size() - 1);
		toRemove.insert(indices[index]);
		indices.erase(indices.begin() + index);
	}

	w.GetLightsBuffer()->CopyFromDevice();
	Light* lights = reinterpret_cast<Light*>(w.GetLightsBuffer()->hostBuffer);
	uint lightBufferSize = w.GetLightsBuffer()->size * 4 / sizeof(Light);
	Light* newlights = new Light[lightBufferSize];
	int bufferi = 0;
	for (int i = 0; i < params.numberOfLights; i++)
	{
		if (toRemove.find(i) == toRemove.end())
		{
			newlights[bufferi++] = lights[i];
		}
	}

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

	uint newLightBufferSize = (params.numberOfLights - _numberOfLights) * 2;
	if (newLightBufferSize < lightBufferSize / 2)
	{
		printf("Resizing buffer to %d\n", newLightBufferSize);
	}

	w.GetLightsBuffer()->hostBuffer = reinterpret_cast<uint*>(newlights);
	delete lights;

	params.restirtemporalframe = 0;
	params.numberOfLights = numberOfLights;
	w.GetLightsBuffer()->CopyToDevice();
}

uint3 positionFromIteration(int3 center, uint& iteration, uint radius)
{
	float angle = (float)iteration / 100 * TWOPI;
	float x1 = cos(angle) * radius;
	float x2 = sin(angle) * radius;
	int3 pos = make_int3(x1 + center.x, center.y, center.z + x2);
	pos.x = max(0, pos.x);
	pos.z = max(0, pos.z);
	return make_uint3(pos);
}