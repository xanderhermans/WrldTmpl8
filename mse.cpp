#include "precomp.h"
#include "mygame.h"
#include "mygamescene.h"
#include <filesystem>
#include "mse.h"

Game* CreateGame() { return new MSE(); }

static vector<Image*> imagebuffer;
static int imageindex = 0;

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
		printf("Swapping image %d energy: %f\n", imageindex, energyInRaster(plottedImage->buffer, plottedImage->width, plottedImage->height));
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
					(std::min<uint32_t>(static_cast<uint32_t>(val.w * 255), 255) << 24));
                surface->Plot(x, y, c);
            }
        }
    }
}

SHORT lastQstate = 0;
SHORT lastEstate = 0;
void MSE::HandleControls(float deltaTime)
{
	SHORT qState = GetAsyncKeyState('Q');
	SHORT eState = GetAsyncKeyState('E');
	if (qState == 0 && lastQstate != qState)
	{
		imageindex = (imageindex + 1) % imagebuffer.size();
	}
	if (eState == 0 && lastEstate != eState)
	{
		imageindex = (imageindex - 1) % imagebuffer.size();
	}
	lastQstate = eState;
	lastQstate = qState;
}

// -----------------------------------------------------------
// Main application tick function
// -----------------------------------------------------------
void MSE::Tick(float deltaTime)
{
	HandleControls(deltaTime);
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