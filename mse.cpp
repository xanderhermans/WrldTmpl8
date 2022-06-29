#include "precomp.h"
#include "mse.h"
#include <filesystem>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
//#include "lib/stb_image.h"
#include "lib/stb_image_write.h"


Game* CreateGame() { return new MSE(); }

static vector<Image*> imagebuffer;
static int imageindex = 0;
static float currentImageEnergy = 0;
static int cursor_x = 0;
static int cursor_y = 0;
static int sample_radius = 0;
static int __count = 0;
static int lastImageIndex = 0;
static ViewerParams viewerParams;

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

float CalculateMSE(const float4* gt, const float4* data, int width, int height)
{
	double total = 0;
	int N = width * height;
	for (int i = 0; i < N; i++)
	{
		float3 _gt = make_float3(gt[i]);
		float3 _data = make_float3(data[i]);

		//float Y = (gt[i].x + gt[i].y + gt[i].z) / 3.0f;
		//float Y_hat = (data[i].x + data[i].y + data[i].z) / 3.0f;
		float3 y = (_gt - _data) * (_gt - _data);
		total += y.x + y.y + y.z;
	}
	return total / (N * 3);
}

float CalculateRMAE(const float4* gt, const float4* data, int width, int height)
{
	double total = 0;
	int N = width * height;
	for (int i = 0; i < N; i++)
	{
		float3 _gt = make_float3(gt[i]);
		float3 _data = make_float3(data[i]);

		//float Y = (gt[i].x + gt[i].y + gt[i].z) / 3.0f;
		//float Y_hat = (data[i].x + data[i].y + data[i].z) / 3.0f;
		float3 y = fabs(_gt - _data);
		total += y.x + y.y + y.z;
	}
	return total / (N * 3);
}

void stitchImage(std::filesystem::path p, vector<UBImage*> ubimages, int start, int segmentWidth)
{
	using namespace filesystem;
	int widthSegment = segmentWidth;
	int topLeft = start;
	int bottomLeft = segmentWidth / 2 + start;
	unsigned char* buf = new unsigned char[ubimages[0]->width * ubimages[0]->height * 3];
	int w = ubimages[0]->width; int h = ubimages[0]->height;
	for (int i = 0; i < w * h * 3; i++)
	{
		buf[i] = ubimages[0]->buffer[i];
	}

	const int numberOfImages = ubimages.size();
	float gradient = float(bottomLeft - topLeft) / h;
	for (int j = 0; j < numberOfImages - 1; j++)
	{
		int _topLeft = topLeft + widthSegment * j;
		UBImage& imageToRead = *ubimages[j + 1];
		for (int y = 0; y < h; y++)
		{
			for (int x = 0; x < widthSegment; x++)
			{
				int _x = x + _topLeft + gradient * y;
				if (_x >= w) break;
				int i = _x + y * w;
				if (i * 3 >= w * h * 3) printf("%d %d %d\n", _x, y, i);
				buf[i * 3] = imageToRead.buffer[i * 3];
				buf[i * 3 + 1] = imageToRead.buffer[i * 3 + 1];
				buf[i * 3 + 2] = imageToRead.buffer[i * 3 + 2];
			}
		}
	}

	for (int y = 0; y < h; y++)
	{
		int _topLeft = topLeft + widthSegment * (numberOfImages - 1);
		UBImage& imageToRead = *ubimages[numberOfImages - 1];
		for (int x = 0; x < widthSegment; x++)
		{
			int _x = x + _topLeft + gradient * y;
			if (_x >= w) break;
			int i = _x + y * w;
			if (i * 3 >= w * h * 3) printf("%d %d %d\n", _x, y, i);
			buf[i * 3] = imageToRead.buffer[i * 3];
			buf[i * 3 + 1] = imageToRead.buffer[i * 3 + 1];
			buf[i * 3 + 2] = imageToRead.buffer[i * 3 + 2];
		}
	}

	path filepath = p / path(ubimages[0]->name).filename();
	path ext = ".tga";
	stbi_write_tga(filepath.replace_extension(ext).string().c_str(), w, h, 3, buf);
	delete[] buf;
	printf("wrote to %s\n", filepath.replace_extension(ext).string().c_str());
}

void doCreateResultImages()
{
	using namespace filesystem;
	path templ8 = "screenbufferdata/tmpl8/results_restir";
	path gfxexp = "screenbufferdata/gfxexp/results/sa32";
	path groundTruthPath = "screenbufferdata/tmpl8/results_pathtraced";
	const int numberOfRasters = 8;
	path groundTruthFiles[numberOfRasters] = { "mountain1", "mountain2", "letters1", "letters2", "flyingapartments1", "flyingapartments2", "cornell", "cornell" };
	path restir[numberOfRasters] = { "mountain1", "mountain2", "letters1", "letters2", "flyingapartments1", "flyingapartments2", "cornellbricks", "cornellvoxels" };
	path gfxExpRestir[numberOfRasters] = { "mountain1", "mountain2", "letters1", "letters2", "flyingapartments1", "flyingapartments2", "cornellvoxels", "cornellvoxels" };
	path sa32 = "sa32";
	path sa16 = "sa16";
	path sa8 = "sa8";
	path sa1 = "sa1";
	path points = "points";
	path ext = ".tga";

	UBImage* image;

	vector<UBImage*> gtImages;
	const int numberOfComparisons = 6;
	vector<UBImage*> ToCompare[numberOfComparisons];

	for (int i = 0; i < numberOfRasters; i++)
	{
		image = new UBImage();
		UBImage::Load(groundTruthPath / groundTruthFiles[i].replace_extension(ext), *image);
		gtImages.push_back(image);
	}

	int toCompareIndex = 0;
	for (int i = 0; i < numberOfRasters; i++)
	{
		image = new UBImage();
		UBImage::Load(templ8 / sa32 / restir[i].replace_extension(ext), *image);
		ToCompare[toCompareIndex].push_back(image);
	}

	toCompareIndex += 1;
	for (int i = 0; i < numberOfRasters; i++)
	{
		image = new UBImage();
		UBImage::Load(templ8 / sa16 / restir[i].replace_extension(ext), *image);
		ToCompare[toCompareIndex].push_back(image);
	}

	toCompareIndex += 1;
	for (int i = 0; i < numberOfRasters; i++)
	{
		image = new UBImage();
		UBImage::Load(templ8 / sa8 / restir[i].replace_extension(ext), *image);
		ToCompare[toCompareIndex].push_back(image);
	}

	toCompareIndex += 1;
	for (int i = 0; i < numberOfRasters; i++)
	{
		image = new UBImage();
		UBImage::Load(templ8 / sa1 / restir[i].replace_extension(ext), *image);
		ToCompare[toCompareIndex].push_back(image);
	}

	toCompareIndex += 1;
	for (int i = 0; i < numberOfRasters; i++)
	{
		image = new UBImage();
		UBImage::Load(templ8 / points / restir[i].replace_extension(ext), *image);
		ToCompare[toCompareIndex].push_back(image);
	}

	toCompareIndex += 1;
	for (int i = 0; i < numberOfRasters; i++)
	{
		image = new UBImage();
		UBImage::Load(gfxexp / gfxExpRestir[i].replace_extension(ext), *image);
		ToCompare[toCompareIndex].push_back(image);
	}

	//{
	//	path p = "screenbufferdata/results/point_sa_gt";
	//	for (int i = 0; i < numberOfRasters; i++)
	//	{
	//		vector<UBImage*> _images;
	//		_images.push_back(ToCompare[toCompareIndex - 1][i]);
	//		_images.push_back(ToCompare[0][i]);
	//		_images.push_back(gtImages[i]);
	//		int w = _images[0]->width;
	//		int segmentWidth = w / _images.size();
	//		int start = 0.75f * segmentWidth;
	//		stitchImage(p, _images, start, segmentWidth);
	//	}
	//}

	//{
	//	path p = "screenbufferdata/results/gfxexp_sa_gt";
	//	for (int i = 0; i < numberOfRasters; i++)
	//	{
	//		vector<UBImage*> _images;
	//		_images.push_back(ToCompare[toCompareIndex][i]);
	//		_images.push_back(ToCompare[0][i]);
	//		_images.push_back(gtImages[i]);
	//		int w = _images[0]->width;
	//		int segmentWidth = w / _images.size();
	//		int start = 0.75f * segmentWidth;
	//		stitchImage(p, _images, start, segmentWidth);
	//	}
	//}

	//{
	//	path p = "screenbufferdata/results/sa_32_16_8_1";
	//	for (int i = 0; i < numberOfRasters; i++)
	//	{
	//		vector<UBImage*> _images;
	//		_images.push_back(ToCompare[0][i]);
	//		_images.push_back(ToCompare[1][i]);
	//		_images.push_back(ToCompare[2][i]);
	//		_images.push_back(ToCompare[3][i]);
	//		int w = _images[0]->width;
	//		int segmentWidth = w / _images.size();
	//		int start = 0.75f * segmentWidth;
	//		stitchImage(p, _images, start, segmentWidth);
	//	}
	//}
}

// -----------------------------------------------------------
// Initialize the application
// -----------------------------------------------------------
void MSE::Init()
{
	// switch between OpenCL rendering (true) and Surface rendering (false)
	Game::autoRendering = true;
	Image* image;

	using namespace filesystem;
	path templ8 = "screenbufferdata/tmpl8/results_restir";
	path gfxexp = "screenbufferdata/gfxexp/results/sa32";
	path groundTruthPath = "screenbufferdata/tmpl8/results_pathtraced";
	const int numberOfRasters = 8;
	path groundTruthFiles[numberOfRasters] = { "mountain1", "mountain2", "letters1", "letters2", "flyingapartments1", "flyingapartments2", "cornell", "cornell" };
	path restir[numberOfRasters] = { "mountain1", "mountain2", "letters1", "letters2", "flyingapartments1", "flyingapartments2", "cornellbricks", "cornellvoxels" };
	path gfxExpRestir[numberOfRasters] = { "mountain1", "mountain2", "letters1", "letters2", "flyingapartments1", "flyingapartments2", "cornellvoxels", "cornellvoxels" };
	path sa32 = "sa32";
	path sa16 = "sa16";
	path sa8 = "sa8";
	path sa1 = "sa1";
	path points = "points";
	path ext = ".dat";

	vector<Image*> gtImages;
	const int numberOfComparisons = 6;
	vector<Image*> ToCompare[numberOfComparisons];

	for (int i = 0; i < numberOfRasters; i++)
	{
		image = new Image();
		Image::Load(groundTruthPath / groundTruthFiles[i].replace_extension(ext), *image);
		imagebuffer.push_back(image);
		gtImages.push_back(image);
	}

	int toCompareIndex = 0;
	for (int i = 0; i < numberOfRasters; i++)
	{
		image = new Image();
		Image::Load(templ8 / sa32 / restir[i].replace_extension(ext), *image);
		imagebuffer.push_back(image);
		ToCompare[toCompareIndex].push_back(image);
	}

	toCompareIndex += 1;
	for (int i = 0; i < numberOfRasters; i++)
	{
		image = new Image();
		Image::Load(templ8 / sa16 / restir[i].replace_extension(ext), *image);
		imagebuffer.push_back(image);
		ToCompare[toCompareIndex].push_back(image);
	}

	toCompareIndex += 1;
	for (int i = 0; i < numberOfRasters; i++)
	{
		image = new Image();
		Image::Load(templ8 / sa8 / restir[i].replace_extension(ext), *image);
		imagebuffer.push_back(image);
		ToCompare[toCompareIndex].push_back(image);
	}

	toCompareIndex += 1;
	for (int i = 0; i < numberOfRasters; i++)
	{
		image = new Image();
		Image::Load(templ8 / sa1 / restir[i].replace_extension(ext), *image);
		imagebuffer.push_back(image);
		ToCompare[toCompareIndex].push_back(image);
	}

	toCompareIndex += 1;
	for (int i = 0; i < numberOfRasters; i++)
	{
		image = new Image();
		Image::Load(templ8 / points / restir[i].replace_extension(ext), *image);
		imagebuffer.push_back(image);
		ToCompare[toCompareIndex].push_back(image);
	}

	toCompareIndex += 1;
	for (int i = 0; i < numberOfRasters; i++)
	{
		image = new Image();
		Image::Load(gfxexp / gfxExpRestir[i].replace_extension(ext), *image);
		imagebuffer.push_back(image);
		ToCompare[toCompareIndex].push_back(image);
	}

	for (int j = 0; j < numberOfComparisons; j++)
	{
		for (int i = 0; i < numberOfRasters; i++)
		{
			Image* gtImage = gtImages[i];
			Image* dataImage = ToCompare[j][i];
			if (gtImage->width != dataImage->width || gtImage->height != dataImage->height)
			{
				printf("Mismatch in dimensions w%d w%d h%d h%d for %s %s\n", gtImage->width, dataImage->width, gtImage->height, dataImage->height, gtImage->name.c_str(), dataImage->name.c_str());
			}
			float mse = CalculateMSE(gtImage->buffer, dataImage->buffer, gtImage->width, gtImage->height);
			float rmae = CalculateRMAE(gtImage->buffer, dataImage->buffer, gtImage->width, gtImage->height);
			printf("pt vs %s MSE %f RMAE %f\n", dataImage->name.c_str(), mse, rmae);
		}
	}

	doCreateResultImages();

	//path path1 = "screenbufferdata/gfxexp";
	//path path2 = "screenbufferdata/tmpl8";
	//for (auto const& dir_entry : std::filesystem::directory_iterator{ path1 })
	//{
	//	if (dir_entry.is_regular_file() && dir_entry.path().extension() == ".dat")
	//	{
	//		image = new Image();
	//		Image::Load(dir_entry.path(), *image);
	//		imagebuffer.push_back(image);
	//	}
	//}

	//for (auto const& dir_entry : std::filesystem::directory_iterator{ path2 })
	//{
	//	if (dir_entry.is_regular_file() && dir_entry.path().extension() == ".dat")
	//	{
	//		image = new Image();
	//		Image::Load(dir_entry.path(), *image);
	//		imagebuffer.push_back(image);
	//	}
	//}

	//int i = 0;
	//for (auto img : imagebuffer)
	//{
	//	float energy = energyInRaster(img->buffer, img->width, img->height);
	//	cout << i++ << " " << img->name << " " << energy << endl;
	//}

	imageindex = imagebuffer.size() - 1;
	Buffer* viewerParamsBuffer = new Buffer(sizeof(ViewerParams) / 4, 0, &viewerParams);
	GetWorld()->SetViewerParamBuffer(viewerParamsBuffer);
	GetWorld()->SetViewerPixelBuffer(imagebuffer[imageindex]->pixelBuffer);
	GetWorld()->SetIsViewer(true);
}

void MSE::PreRender()
{
	GetWorld()->SetViewerPixelBuffer(imagebuffer[imageindex]->pixelBuffer);
	viewerParams.selectedX = cursor_x;
	viewerParams.selectedY = cursor_y;
}

// Only called when Game::autoRendering = false otherwise rendering is handled by OpenCL kernels
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
				uint32_t c = ((std::min<uint32_t>(static_cast<uint32_t>(val.z * 256), 255) << 0) |
					(std::min<uint32_t>(static_cast<uint32_t>(val.y * 256), 255) << 8) |
					(std::min<uint32_t>(static_cast<uint32_t>(val.x * 256), 255) << 16) |
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

KeyHandler qhandler = { 0, 'Q' };
KeyHandler ehandler = { 0, 'E' };
KeyHandler radiusp = { 0, 'X' };
KeyHandler radiusm = { 0, 'Z' };
KeyHandler rhandler = { 0, 'R' };
KeyHandler thandler = { 0, 'T' };
KeyHandler shandler = { 0, 'S' };
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

	if (shandler.IsTyped())
	{
		using namespace filesystem;
		stbi_flip_vertically_on_write(1);
		Image& img = *imagebuffer[imageindex];
		int w = img.width,
			h = img.height;
		unsigned char* data = new unsigned char[w * h * 3];
		glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, data);
		path ext = ".tga";
		path filepath = img.name;
		stbi_write_tga(filepath.replace_extension(ext).string().c_str(), img.width, img.height, 3, data);
		delete[] data;
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
		if (j >= 0 && j < width && i >= 0 && i < height)
		{
			float4 e = buff[j + i * width];
			energy_x += e.x;
			energy_y += e.y;
			energy_z += e.z;
		}
	}


	for (int i = y - radius; i < y + radius; i++)
	{
		for (int j = x; (j - x) * (j - x) + (i - y) * (i - y) <= radius * radius; j--)
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
	printf("                                                                                            \r");
	printf("image name:%s i:%d d:%d %d energy:%.3f cursor:%d %d:%.4f %.4f %.4f\r", imagebuffer[imageindex]->name.c_str(), imageindex, sample_radius * 2 + 1, __count, currentImageEnergy, cursor_x, cursor_y, energy.x, energy.y, energy.z);
	//printf("image_i:%d energy:%.3f cursor:%d %d\r", imageindex, currentImageEnergy, cursor_x, cursor_y);

}

void UBImage::Load(std::filesystem::path path, UBImage& image)
{
	int width; int height;
	int channels;
	image.buffer = stbi_load(path.string().c_str(), &width, &height, &channels, 3);
	image.width = width;
	image.height = height;
	image.name = path.string();
}

void Image::Load(std::filesystem::path path, Image& image)
{
	ifstream rf(path.string().c_str(), ios::in | ios::binary);
	if (!rf)
	{
		printf("Error opening file %s\n", path.string().c_str());
		return;
	}

	image.name = (path.parent_path() / path.filename()).string();

	uint32_t width, height;
	rf.read((char*)&width, sizeof(uint32_t));
	rf.read((char*)&height, sizeof(uint32_t));

	image.width = width;
	image.height = height;
	if (width != SCRWIDTH || height != SCRHEIGHT)
	{
		printf("ATTEMPTING TO LOAD IMAGE THAT HAS DIFFERENT SIZE THAN THE WINDOW!\n");
	}
	if (image.buffer == nullptr)
	{
		image.pixelBuffer = new Buffer(4 * width * height, 0, new uint[4 * width * height]);
		image.buffer = reinterpret_cast<float4*>(image.pixelBuffer->hostBuffer);
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

	image.pixelBuffer->CopyToDevice();

	rf.close();
	if (!rf.good()) {
		printf("Error closing file %s\n", path.string().c_str());
	}
}