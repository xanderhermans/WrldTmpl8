#include "precomp.h"
#include "mygame.h"
#include "mygamescene.h"

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
static bool skyDomeSampling = true;

static unordered_map<string, void*> commands;
static unordered_map<string, function<void(MyGame&, string)>> functionCommands;

static float3 Ds[10] = {
	float3(-0.67, -0.18, -0.72), float3(0.40, -0.38, 0.83),
	float3(0.11, -0.22, 0.97), float3(-0.48, -0.28, -0.83),
	float3(-0.55, -0.26, 0.79), float3(0.45, -0.39, -0.80),
	float3(0.11, -0.22, 0.97), float3(0.11, -0.22, 0.97),
	float3(0.11, -0.22, 0.97), float3(0.11, -0.22, 0.97)
};
static float3 Os[10] = {
	float3(954.84, 23.43, 955.18), float3(353.51, 234.30, 509.94),
	float3(152.32, 59.82, -49.48), float3(195.35, 84.76, 264.97),
	float3(193.53, 54.54, 85.59), float3(87.03, 86.38, 247.10),
	float3(81.18, 196.95, -284.07), float3(81.18, 196.95, -284.07),
	float3(107.85, 60.81, -48.86), float3(107.85, 60.81, -48.86)
};

static int world_index = 2;
static int angle_offset_index = 0;

// -----------------------------------------------------------
// Initialize the application
// -----------------------------------------------------------
void MyGame::Init()
{
	string import_filename = "/grid.vx";
	string export_dir = "/scene";

	string worlds[5] = {"mountain","letters","scattered","cornellbox",""};
	string world_dir = worlds[world_index];
	string import_path = "scene_export/" + world_dir + import_filename;
	string export_path = "scene_export/" + world_dir + export_dir;

	World& world = *GetWorld();

	int loadedworld = MyGameScene::LoadWorld(world, import_path);
	//int loadedworld = -1;
	if (loadedworld < 0)
	{
		switch (world_index)
		{
		case 0:
			MyGameScene::CreateMountainWorld(world);
			break;
		case 1:
			MyGameScene::CreateLetterWorld(world);
			break;
		case 2:
			MyGameScene::CreateScatteredWorld(world);
			break;
		case 3:
			MyGameScene::CreateCornellBoxWorld(world);
			break;
		default:
			MyGameScene::CreateWorld(world);
			break;
		}
		aabb& aabb = world.GetBounds();
		uint sizeX = aabb.bmax[0];
		uint sizeY = aabb.bmax[1];
		uint sizeZ = aabb.bmax[2];
		MyGameScene::SaveWorld(import_path, sizeX + 4, sizeY + 4, sizeZ + 4);
	}

	if (!filesystem::exists(export_path + ".obj"))
	{
		WorldToOBJ(&world, export_path);
	}

	_D = Ds[world_index * 2 + angle_offset_index];
	_O = Os[world_index * 2 + angle_offset_index];
	D = _D;
	O = _O;

	RenderParams& params = world.GetRenderParams();
	params.numberOfLights = 0;
	params.accumulate = false;
	params.spatial = useSpatialResampling;
	params.temporal = useTemporalResampling;
	params.spatialTaps = SPATIALTAPS;
	params.spatialRadius = SPATIALRADIUS;
	params.numberOfCandidates = NUMBEROFCANDIDATES;
	params.numberOfMaxTemporalImportance = TEMPORALMAXIMPORTANCE;
	params.skyDomeSampling = skyDomeSampling;
	world.GetDebugInfo().counter = 0;

	commands.insert({ "spatialtaps", &params.spatialTaps });
	commands.insert({ "spatialradius", &params.spatialRadius });
	commands.insert({ "numberofcandidates", &params.numberOfCandidates });
	commands.insert({ "temporalimportance", &params.numberOfMaxTemporalImportance });
	commands.insert({ "spatial", &params.spatial });
	commands.insert({ "temporal", &params.temporal });
	commands.insert({ "skydome", &params.skyDomeSampling });
	functionCommands.insert({ "addlights", [](MyGame& _1, string _2) {IntArgFunction([](MyGame& g, int a) {g.GetLightManager().AddRandomLights(a); }, _1, _2, 2500); }});
	functionCommands.insert({ "removelights", [](MyGame& _1, string _2) {IntArgFunction([](MyGame& g, int a) {g.GetLightManager().RemoveRandomLights(a); }, _1, _2, 2500); } });
	functionCommands.insert({ "movelightcount", [](MyGame& _1, string _2) {IntArgFunction([](MyGame& g, int a) {g.GetLightManager().SetUpMovingLights(a); }, _1, _2, 2500); } });

	//world.SetBrick(8 * BRICKDIM, 1 * BRICKDIM, 8 * BRICKDIM, WHITE | (1 << 12));
	//world.SetBrick(8 * BRICKDIM, 0 * BRICKDIM, 16 * BRICKDIM, WHITE | (1 << 12));
	//world.SetBrick(8 * BRICKDIM, 1 * BRICKDIM, 24 * BRICKDIM, WHITE | (1 << 12));
	//world.Set(12 * BRICKDIM, 1 * BRICKDIM, 8 * BRICKDIM, WHITE | (1 << 12));
	//world.Set(12 * BRICKDIM, 0 * BRICKDIM + 1, 16 * BRICKDIM, WHITE | (1 << 12));
	//world.Set(12 * BRICKDIM, 1 * BRICKDIM, 24 * BRICKDIM, WHITE | (1 << 12));

	//world.SetBrick(24 * BRICKDIM, 1 * BRICKDIM, 8 * BRICKDIM, WHITE | (1 << 12));
	//world.SetBrick(24 * BRICKDIM, 0 * BRICKDIM, 16 * BRICKDIM, WHITE | (1 << 12));
	//world.SetBrick(24 * BRICKDIM, 1 * BRICKDIM, 24 * BRICKDIM, WHITE | (1 << 12));
	//world.Set(20 * BRICKDIM, 1 * BRICKDIM, 8 * BRICKDIM, WHITE | (1 << 12));
	//world.Set(20 * BRICKDIM, 0 * BRICKDIM + 1, 16 * BRICKDIM, WHITE | (1 << 12));
	//world.Set(20 * BRICKDIM, 1 * BRICKDIM, 24 * BRICKDIM, WHITE | (1 << 12));

	vector<Light> ls;
	world.OptimizeBricks(); //important to recognize bricks
	lightManager.FindLightsInWorld(ls);
	lightManager.SetupBuffer(ls);
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
	if (GetAsyncKeyState(VK_LCONTROL))
	{
		speed *= 0.01;
	}
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
		lightManager.lightsAreMoving = !lightManager.lightsAreMoving;
		if (lightManager.MovingLightCount() < 1)
		{
			lightManager.SetUpMovingLights(100);
		}
	}
	if (oHandler.IsTyped())
	{
		lightManager.poppingLights = !lightManager.poppingLights;
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
		renderparams.accumulate = (int)(!(bool)(renderparams.accumulate & 1));
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
	World& world = *GetWorld();

	printf("                                                                                \r");
	printf("-cam-pos %.2f %.2f %.2f -cam-pitch %.2f -cam-yaw %.2f -cam-roll %.2f fov %.4f\r", -O.x, O.y, O.z, angles.x, angles.y, angles.z, fov);
	//printf("Camera at _D(%.2f, %.2f, %.2f), _O(%.2f, %.2f, %.2f); fov %.2f\r", D.x, D.y, D.z, O.x, O.y, O.z, fov);

#if RIS == 1
	clWaitForEvents(1, &world.renderDone);
	clWaitForEvents(1, &world.albedoRenderDone);
	clWaitForEvents(1, &world.candidateAndTemporalResamplingDone);
	clWaitForEvents(1, &world.spatialResamplingDone);
	clWaitForEvents(1, &world.shadingDone);

	const int numberOfFrames = 100;
	static int frameCounter = -1;
	static float totalRenderTimes[numberOfFrames] = { };
	static float albedoRenderTimes[numberOfFrames] = { };
	static float candidateRenderTimes[numberOfFrames] = { };
	static float spatialRenderTimes[numberOfFrames] = { };
	static float shadingRenderTimes[numberOfFrames] = { };
	static float finalizingRenderTimes[numberOfFrames] = { };
	if (frameCounter < 0) //first frame
	{
		for (int i = 0; i < numberOfFrames; i++) { totalRenderTimes[i] = -1; }
		for (int i = 0; i < numberOfFrames; i++) { albedoRenderTimes[i] = -1; }
		for (int i = 0; i < numberOfFrames; i++) { candidateRenderTimes[i] = -1; }
		for (int i = 0; i < numberOfFrames; i++) { spatialRenderTimes[i] = -1; }
		for (int i = 0; i < numberOfFrames; i++) { shadingRenderTimes[i] = -1; }
		for (int i = 0; i < numberOfFrames; i++) { finalizingRenderTimes[i] = -1; }
		frameCounter = 0;
	}

	float totalRenderTime = 0;
	float albedoRenderTime = 0;
	float candidateRenderTime = 0;
	float spatialRenderTime = 0;
	float shadingRenderTime = 0;
	float finalizingRenderTime = 0;

	totalRenderTimes[frameCounter] = world.GetRenderTime();
	albedoRenderTimes[frameCounter] = world.GetAlbedoTime();
	candidateRenderTimes[frameCounter] = world.GetCandidateTime();
	spatialRenderTimes[frameCounter] = world.GetSpatialTime();
	shadingRenderTimes[frameCounter] = world.GetShadingTime();
	finalizingRenderTimes[frameCounter] = world.GetFinalizingTime();

	for (int i = 0; i < numberOfFrames + 1; i++) { float time = totalRenderTimes[i]; if (time < 0 || i == numberOfFrames) { totalRenderTime /= i; break; } totalRenderTime += time; }
	for (int i = 0; i < numberOfFrames + 1; i++) { float time = albedoRenderTimes[i];  if (time < 0 || i == numberOfFrames) { albedoRenderTime /= i; break; } albedoRenderTime += time; }
	for (int i = 0; i < numberOfFrames + 1; i++) { float time = candidateRenderTimes[i];  if (time < 0 || i == numberOfFrames) { candidateRenderTime /= i; break; } candidateRenderTime += time; }
	for (int i = 0; i < numberOfFrames + 1; i++) { float time = spatialRenderTimes[i];  if (time < 0 || i == numberOfFrames) { spatialRenderTime /= i; break; } spatialRenderTime += time; }
	for (int i = 0; i < numberOfFrames + 1; i++) { float time = shadingRenderTimes[i];  if (time < 0 || i == numberOfFrames) { shadingRenderTime /= i; break; } shadingRenderTime += time; }
	for (int i = 0; i < numberOfFrames + 1; i++) { float time = finalizingRenderTimes[i];  if (time < 0 || i == numberOfFrames) { finalizingRenderTime /= i; break; } finalizingRenderTime += time; }

	//printf("                                                                                                                                 \r");
	printf("%d total %f, albedo %f, candidate %f, spatial %f, shading %f\n", frameCounter, totalRenderTime * 1000, albedoRenderTime * 1000, candidateRenderTime * 1000, spatialRenderTime * 1000, (shadingRenderTime + finalizingRenderTime) * 1000);
	//printf("%d total %f, albedo %f, candidate %f, spatial %f, shading %f, finalizing %f\n", frameCounter, world.GetRenderTime(), world.GetAlbedoTime(), world.GetCandidateTime(), world.GetSpatialTime(), world.GetShadingTime(), world.GetFinalizingTime());

	frameCounter = (frameCounter + 1) % numberOfFrames;
#endif
}

void MyGame::PrintDebug()
{
	clWaitForEvents(1, &GetWorld()->GetRenderDoneEventHandle());
	GetWorld()->GetDebugBuffer()->CopyFromDevice();
	DebugInfo* debugInfo = reinterpret_cast<DebugInfo*>(GetWorld()->GetDebugBuffer()->GetHostPtr());
	Reservoir res = debugInfo->res; Reservoir res1 = debugInfo->res1; Reservoir res2 = debugInfo->res2; Reservoir res3 = debugInfo->res3;
	float4 f1 = debugInfo->f1; float4 f2 = debugInfo->f2; float4 f3 = debugInfo->f3; float4 f4 = debugInfo->f4;
	RenderParams params = GetWorld()->GetRenderParams();
	printf("res %f %d %d %f %f %f %f %f\n", res.sumOfWeights, res.streamLength, res.lightIndex, res.adjustedWeight, res.positionOnVoxel.x, res.positionOnVoxel.y, res.positionOnVoxel.z, res.invPositionProbability);
	printf("res1 %f %d %d %f %f %f %f %f\n", res1.sumOfWeights, res1.streamLength, res1.lightIndex, res1.adjustedWeight, res1.positionOnVoxel.x, res1.positionOnVoxel.y, res1.positionOnVoxel.z, res1.invPositionProbability);
	printf("res2 %f %d %d %f %f %f %f %f\n", res2.sumOfWeights, res2.streamLength, res2.lightIndex, res2.adjustedWeight, res2.positionOnVoxel.x, res2.positionOnVoxel.y, res2.positionOnVoxel.z, res2.invPositionProbability);
	printf("res3 %f %d %d %f %f %f %f %f\n", res3.sumOfWeights, res3.streamLength, res3.lightIndex, res3.adjustedWeight, res3.positionOnVoxel.x, res3.positionOnVoxel.y, res3.positionOnVoxel.z, res3.invPositionProbability);
	//if (f1.x != f2.x || f1.y != f2.y || f1.z != f2.z)
	{
		printf("res %f %f %f %f\n", f1.x, f1.y, f1.z, f1.w);
		printf("res %f %f %f %f\n", f2.x, f2.y, f2.z, f2.w);
	}
	//printf("res %f %f %f %f\n", f1.x, f1.y, f1.z, f1.w);
	//printf("res %f %f %f %f\n", f2.x, f2.y, f2.z, f2.w);
	printf("res %f %f %f %f\n", f3.x, f3.y, f3.z, f3.w);
	printf("res %f %f %f %f\n", f4.x, f4.y, f4.z, f4.w);
	//if (f1.x != f1.y) printf("res %f %f %f %f\n", f1.x, f1.y, f1.z, f1.w);
	printf("%d\n", debugInfo->counter);
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
	World& world = *GetWorld();
	RenderParams& renderparams = GetWorld()->GetRenderParams();
	// clear line
	//printf("                                                            \r");
	//printf("temporal frames %d\r", renderparams.numberOfMaxTemporalImportance);
	//printf("Frame: %d acc:%d sp:%d coord x:%.2f y:%.2f z:%.2f\r", GetWorld()->GetRenderParams().framecount, GetWorld()->GetRenderParams().accumulate, useSpatialResampling, O.x, O.y, O.z);

	//PrintDebug();
	//PrintStats();
	DumpScreenBuffer();
	if (lightManager.lightsAreMoving)
	{
		lightManager.MoveLights();
	}
	if (lightManager.poppingLights)
	{
		lightManager.PopLights(deltaTime);
	}
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

void MyGame::IntArgFunction(function<void(MyGame&, int)> fn, MyGame& g, string s, int defaultarg)
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