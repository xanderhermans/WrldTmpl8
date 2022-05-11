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
static int sample_radius = 0;
static int __count = 0;
static int lastImageIndex = 0;

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
		cout << i++ << " " << img->name << " " << energy << endl;
	}
}

void MSE::Render(Surface* surface)
{
    static Image* plottedImage = 0;
    if (imagebuffer.size() > 0 /*&& plottedImage != imagebuffer[imageindex]*/)
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

	uint cursor_color = 0xff00ff;
	__count = 1;
	{
		int j = cursor_x;
		int i = cursor_y;
		surface->Plot(j, i, cursor_color);
	}
	for (int i = cursor_y - sample_radius; i < cursor_y + sample_radius; i++) {
		for (int j = cursor_x; (j - cursor_x) * (j - cursor_x) + (i - cursor_y) * (i - cursor_y) <= sample_radius * sample_radius; j--) {
			if (i >= 0 && i < SCRHEIGHT && j >= 0 && j < SCRWIDTH)
			{
				surface->Plot(j, i, cursor_color);
				__count++;
			}
		}
		for (int j = cursor_x + 1; (j - cursor_x) * (j - cursor_x) + (i - cursor_y) * (i - cursor_y) <= sample_radius * sample_radius; j++) {
			if (i >= 0 && i < SCRHEIGHT && j >= 0 && j < SCRWIDTH)
			{
				surface->Plot(j, i, cursor_color);
				__count++;
			}
		}
	}
}

KeyHandler qhandler = {0, 'Q'};
KeyHandler ehandler = {0, 'E'};
KeyHandler radiusp = { 0, 'X' };
KeyHandler radiusm = { 0, 'Z' };
KeyHandler rhandler = { 0, 'R' };
KeyHandler thandler = { 0, 'T' };
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
	if (!isFocused) return; // ignore controls if window doesnt have focus
	if (ehandler.IsTyped())
	{
		imageindex = imageindex + 1;
	}
	if (qhandler.IsTyped())
	{
		imageindex = imageindex - 1;
	}
	if (thandler.IsTyped())
	{
		lastImageIndex = imageindex;
	}
	if (rhandler.IsTyped())
	{
		imageindex = lastImageIndex;
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
		sample_radius = max(0, sample_radius - 1);
	}
}

float3 energyAroundCoord(int x, int y, int radius, float4* buff, int width, int height)
{
	double energy_x = 0;
	double energy_y = 0;
	double energy_z = 0;
	int count = 1;
	{
		int i = y;
		int j = x;
		float4 e = buff[j + i * width];
		energy_x += e.x;
		energy_y += e.y;
		energy_z += e.z;
	}

	for (int i = y - radius; i < y + radius; i++) 
	{
		for (int j = x; (j - x) * (j - x) + (i - y) * (i - y) <= radius * radius; j--)
		{
			if (i >= 0 && i < SCRHEIGHT && j >=0 && j < SCRWIDTH)
			{
				float4 e = buff[j + i * width];
				energy_x += e.x;
				energy_y += e.y;
				energy_z += e.z;
				count++;
			}
		}
		for (int j = x + 1; (j - x) * (j - x) + (i - y) * (i - y) <= radius * radius; j++) 
		{
			if (i >= 0 && i < SCRHEIGHT && j >= 0 && j < SCRWIDTH)
			{
				float4 e = buff[j + i * width];
				energy_x += e.x;
				energy_y += e.y;
				energy_z += e.z;
				count++;
			}
		}
	}

	float3 energy = make_float3(
		energy_x / count,
		energy_y / count,
		energy_z / count
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
	printf("image name:%s i:%d d:%d %d energy:%.3f cursor:%d %d:%.4f %.4f %.4f\r", imagebuffer[imageindex]->name.c_str(), imageindex, sample_radius * 2 + 1, __count, currentImageEnergy, cursor_x, cursor_y, energy.x, energy.y, energy.z);
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

	image.name = path.filename().string();

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