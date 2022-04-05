#include "precomp.h"
#include "mygame.h"
#include "mygamescene.h"
#include <filesystem>


#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "lib/stb_image_write.h"

Game* CreateGame() { return new MyGame(); }

//float3 _D(0, 0, 1), _O(107.85, 60.81, -48.86);
float3 _D(0.11, -0.22, 0.97), _O(107.85, 60.81, -48.86);
float3 D = _D, O = _O;

static bool shouldDumpBuffer = false;
static bool takeScreenshot = false;
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

	GetWorld()->GetRenderParams().accumulate = true;

	SetupLightBuffers();
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
					light.weight = 1;
					lights.push_back(light);
				}
			}
		}
	}

	uint numberOfLights = lights.size();
	Buffer* lightbuffer = world.GetLightsBuffer();
	if (!lightbuffer)
	{
		printf("Light buffer does not exist, creating.\n");
		lightbuffer = new Buffer(sizeof(Light) / 4 * numberOfLights * 2, Buffer::DEFAULT, new Light[numberOfLights * 2]);
		lightbuffer->ownData = true;
	}
	else if (lightbuffer->size < numberOfLights)
	{
		printf("Light buffer too small, resizing.\n");
		delete lightbuffer;
		lightbuffer = new Buffer(sizeof(Light) / 4 * numberOfLights * 2, 0, new Light[numberOfLights * 2]);
		lightbuffer->ownData = true;
	}

	printf("Number of emitting voxels: %d\n", numberOfLights);

	Light* lightsData = (Light*)lightbuffer->hostBuffer;
	for (int i = 0; i < lights.size(); i++)
	{
		auto light = lights[i];
		light.weight /= (float)lights.size();
		lightsData[i] = light;
	}

	world.GetRenderParams().numberOfLights = numberOfLights;
	lightbuffer->CopyToDevice();
	world.SetLightsBuffer(lightbuffer);
}

SHORT lastEstate = 0;
SHORT lastQstate = 0;
SHORT lastSpaceState = 0;
void MyGame::HandleControls(float deltaTime)
{
	if (!isFocused) return; // ignore controls if window doesnt have focus
	// free cam controls
	float3 tmp(0, 1, 0), right = normalize(cross(tmp, D)), up = cross(D, right);
	float speed = deltaTime * 0.03f;
	bool dirty = false;

	SHORT qState = GetAsyncKeyState('Q');
	SHORT eState = GetAsyncKeyState('E');
	SHORT spaceState = GetAsyncKeyState(VK_SPACE);
	if (qState == 0 && qState != lastEstate) { shouldDumpBuffer = true; printf("Dumping screenbuffer queued.\n"); }
	if (eState == 0 && eState != lastQstate) { takeScreenshot = true; printf("Next dump is screenshot.\n"); }

	if (GetAsyncKeyState('W')) { O += speed * D; dirty = true; }
	else if (GetAsyncKeyState('S')) { O -= speed * D; dirty = true; }
	if (GetAsyncKeyState('A')) { O -= speed * right; dirty = true; }
	else if (GetAsyncKeyState('D')) { O += speed * right; dirty = true; }

	if (GetAsyncKeyState('R')) { O += speed * up; dirty = true; }
	else if (GetAsyncKeyState('F')) { O -= speed * up; dirty = true; }

	if (GetAsyncKeyState(VK_LEFT)) { D = normalize(D - right * 0.025f * speed); dirty = true; }
	else if (GetAsyncKeyState(VK_RIGHT)) { D = normalize(D + right * 0.025f * speed); dirty = true; }
	if (GetAsyncKeyState(VK_UP)) { D = normalize(D - up * 0.025f * speed); dirty = true; }
	else if (GetAsyncKeyState(VK_DOWN)) { D = normalize(D + up * 0.025f * speed); dirty = true; }
	
	if (GetAsyncKeyState('Z')) { D = _D; O = _O; dirty = true; }

	if (GetAsyncKeyState('L')) { PrintStats(); };
	
	LookAt(O, O + D);

	if (spaceState == 0 && spaceState != lastSpaceState)
	{
		GetWorld()->GetRenderParams().accumulate = !GetWorld()->GetRenderParams().accumulate;
		dirty = true;
	}
	if (GetAsyncKeyState('X')) dirty = true;
	if (dirty)
	{
		GetWorld()->GetRenderParams().framecount = 0;
		GetWorld()->GetRenderParams().frame = 0;
	}

	lastEstate = qState;
	lastQstate = eState;
	lastSpaceState = spaceState;
}

void MyGame::PreRender()
{
	SubSampleLightSamples();
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

const int numberOfInitialSamples = 1000;
const int numberOfCandidates = 32;

void MyGame::SubSampleLightSamples()
{
	World& world = *GetWorld();

	Buffer* reservoirbuffer = world.GetReservoirsBuffer();
	if (!reservoirbuffer)
	{
		reservoirbuffer = new Buffer(sizeof(Reservoir) / 4 * SCRWIDTH * SCRHEIGHT, 0, new Reservoir[SCRWIDTH * SCRHEIGHT]);
		reservoirbuffer->CopyToDevice();
		world.SetReservoirBuffer(reservoirbuffer);
	}

	Buffer* initialSamplingBuffer = world.GetInitialSamplingBuffer();
	if (!initialSamplingBuffer)
	{
		initialSamplingBuffer = new Buffer(sizeof(Reservoir) / 4 * numberOfInitialSamples, 0, new Reservoir[numberOfInitialSamples]);
		world.GetRenderParams().numberOfInitialSamples = numberOfInitialSamples;
		world.SetInitialSamplingBuffer(initialSamplingBuffer);
	}

	Light* lightsData = (Light*)world.GetLightsBuffer()->hostBuffer;
	if (!world.GetLightsBuffer() || !world.GetLightsBuffer()->hostBuffer)
	{
		printf("Light buffer not set up yet. No initial sampling\n"); 
		return;
	}
	Reservoir* initialSamplings = (Reservoir*)initialSamplingBuffer->hostBuffer;
	const int numberOfLights = world.GetRenderParams().numberOfLights;
	for (int i = 0; i < numberOfInitialSamples; i++)
	{
		Reservoir& res = initialSamplings[i];
		res.light_index = 0;
		res.streamLength = 0;
		res.sumOfWeights = 0;
		res.type = 0;

		for (int j = 0; j < numberOfCandidates; j++)
		{
			int random_index = RandomFloat() * (numberOfLights - 1);
			Light& randomlight = lightsData[random_index];
			UpdateReservoirL(res, randomlight, RandomFloat());
		}
	}

	initialSamplingBuffer->CopyToDevice();
}

// -----------------------------------------------------------
// Main application tick function
// -----------------------------------------------------------
void MyGame::Tick(float deltaTime)
{
	HandleControls(deltaTime);
	//clWaitForEvents(1,&GetWorld()->GetRenderDoneEventHandle());
	//GetWorld()->GetFrameBuffer()->CopyFromDevice();
	//auto buf = GetWorld()->GetFrameBuffer()->GetHostPtr();

	// clear line
	printf("                                                            \r");
	printf("Frame: %d acc: %d coord x:%.2f y:%.2f z:%.2f\r", GetWorld()->GetRenderParams().framecount, GetWorld()->GetRenderParams().accumulate, O.x, O.y, O.z);

	DumpScreenBuffer();
}

union convertor {
	uint asInt;
	float asFloat;
};

void saveScreenBuffer(const std::filesystem::path& filepath, uint32_t width, uint32_t height, const float4* data);
void MyGame::DumpScreenBuffer()
{
#if 1 //stratified
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