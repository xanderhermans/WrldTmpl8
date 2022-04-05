#include "precomp.h"
#include "mygame.h"
#include "mygamescene.h"
#include <filesystem>
#include "mse.h"

Game* CreateGame() { return new MSE(); }

static vector<Image*> imagebuffer;
static int imageindex = 0;
static float currentImageEnergy = 0;
static int cursor_x = 0;
static int cursor_y = 0;
static int sample_radius = 1;

float energyInRaster(const float4* data, int width, int height)
{
	double total = 0;
	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			const float4 val = data[x + y * width];
			total += (val.x + val.y + val.z) / 3.0f;
		}
	}
	return total / width / height;
}

// -----------------------------------------------------------
// Initialize the application
// -----------------------------------------------------------
void MSE::Init()
{
	Game::autoRendering = false;
	Image* image;

	using namespace filesystem;
	path path1 = "screenbufferdata/gfxexp";
	path path2 = "screenbufferdata/tmpl8";

	for (auto const& dir_entry : std::filesystem::directory_iterator{ path1 })
	{
		if (dir_entry.is_regular_file() && dir_entry.path().extension() == ".dat")
		{
			image = new Image();
			Image::Load(dir_entry.path(), *image);
			imagebuffer.push_back(image);
		}
	}

	for (auto const& dir_entry : std::filesystem::directory_iterator{ path2 })
	{
		if (dir_entry.is_regular_file() && dir_entry.path().extension() == ".dat")
		{
			image = new Image();
			Image::Load(dir_entry.path(), *image);
			imagebuffer.push_back(image);
		}
	}

	int i = 0;
	for (auto img : imagebuffer)
	{
		float energy = energyInRaster(img->buffer, img->width, img->height);
		cout << i++ << " " << energy << endl;
	}
}

void MSE::Render(Surface* surface)
{
    static Image* plottedImage = 0;
    if (imagebuffer.size() > 0 && plottedImage != imagebuffer[imageindex])
    {
		plottedImage = imagebuffer[imageindex];
		currentImageEnergy = energyInRaster(plottedImage->buffer, plottedImage->width, plottedImage->height);
		if (plottedImage->width > surface->width || plottedImage->height > surface->height)
		{
			printf("Image is larger than surface. Implicit cropping happening.\n");
		}

        for (int y = 0; y < plottedImage->height; y++)
        {
            for (int x = 0; x < plottedImage->width; x++)
            {
                float4 val = plottedImage->buffer[x + y * plottedImage->width];
				uint32_t c = ((std::min<uint32_t>(static_cast<uint32_t>(val.z * 255), 255) << 0) |
					(std::min<uint32_t>(static_cast<uint32_t>(val.y * 255), 255) << 8) |
					(std::min<uint32_t>(static_cast<uint32_t>(val.x * 255), 255) << 16) |
					255 << 24);
					//(std::min<uint32_t>(static_cast<uint32_t>(val.w * 255), 255) << 24));
                surface->Plot(x, y, c);
            }
        }
    }
}

struct KeyHandler
{
	SHORT last = 0;
	char key = 0;

	bool IsTyped()
	{
		SHORT state = GetAsyncKeyState(key);
		SHORT _last = last;
		last = state;
		return state == 0 && _last != state;
	}
};

KeyHandler qhandler = {0, 'Q'};
KeyHandler ehandler = {0, 'E'};
KeyHandler radiusp = { 0, 'X' };
KeyHandler radiusm = { 0, 'Z' };
KeyHandler numhandlers[10] = {
	{0, '0'},
	{0, '1'},
	{0, '2'},
	{0, '3'},
	{0, '4'},
	{0, '5'},
	{0, '6'},
	{0, '7'},
	{0, '8'}, 
	{0, '9'}, 
};
void MSE::HandleControls(float deltaTime)
{
	if (ehandler.IsTyped())
	{
		imageindex = imageindex + 1;
	}
	if (qhandler.IsTyped())
	{
		imageindex = imageindex - 1;
	}

	for (int i = 0; i < 10; i++)
	{
		if (numhandlers[i].IsTyped())
		{
			imageindex = i;
		}
	}

	if (imageindex < 0)
	{
		imageindex = imagebuffer.size() - 1;
	}
	else if (imageindex >= imagebuffer.size())
	{
		imageindex = 0;
	}

	float2 pos = GetCursorPosition();
	cursor_x = pos.x;
	cursor_y = pos.y;

	cursor_x = max(min(cursor_x, SCRWIDTH), 0);
	cursor_y = max(min(cursor_y, SCRHEIGHT), 0);

	if (radiusp.IsTyped())
	{
		sample_radius++;
	}

	if (radiusm.IsTyped())
	{
		sample_radius = max(1, sample_radius - 1);
	}
}

float3 energyAroundCoord(int x, int y, int radius, float4* buff, int width, int height)
{
	double energy_x = 0;
	double energy_y = 0;
	double energy_z = 0;
	int sample_count = 0;
	for (int _y = max(y - radius + 1, 0); _y < min(height, y + radius); _y++)
	{
		for (int _x = max(x - radius + 1, 0); _x < min(width, x + radius); _x++)
		{
			float4 e = buff[_x + _y * width];
			energy_x += e.x;
			energy_y += e.y;
			energy_z += e.z;
			sample_count++;
		}
	}
	sample_count = max(1, sample_count);
	float3 energy = make_float3(
		energy_x / sample_count,
		energy_y / sample_count,
		energy_z / sample_count
	);
	return energy;
}

// -----------------------------------------------------------
// Main application tick function
// -----------------------------------------------------------
void MSE::Tick(float deltaTime)
{
	HandleControls(deltaTime);

	float3 energy = energyAroundCoord(cursor_x, cursor_y, sample_radius, imagebuffer[imageindex]->buffer, imagebuffer[imageindex]->width, imagebuffer[imageindex]->height);
	printf("                                                            \r");
	printf("image_i:%d r:%d energy:%.3f cursor:%d %d:%.4f %.4f %.4f\r", imageindex, sample_radius, currentImageEnergy, cursor_x, cursor_y, energy.x, energy.y, energy.z);
	//printf("image_i:%d energy:%.3f cursor:%d %d\r", imageindex, currentImageEnergy, cursor_x, cursor_y);
}

void Image::Load(std::filesystem::path path, Image& image)
{
	ifstream rf(path.string().c_str(), ios::in | ios::binary);
	if (!rf)
	{
		printf("Error opening file %s\n", path.string().c_str());
		return;
	}

	uint32_t width, height;
	rf.read((char*)&width, sizeof(uint32_t));
	rf.read((char*)&height, sizeof(uint32_t));

	image.width = width;
	image.height = height;
	if (image.buffer == nullptr)
	{
		image.buffer = new float4[width * height];
	}

	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++)
		{
			float4 val;
			rf.read((char*)&val, sizeof(float4));
			image.buffer[x + y * width] = val;
		}
	}

	rf.close();
	if (!rf.good()) {
		printf("Error closing file %s\n", path.string().c_str());
	}
}