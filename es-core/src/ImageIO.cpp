#include "ImageIO.h"

#include "Log.h"
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_surface.h>
#include <SDL3_image/SDL_image.h>
#include <algorithm>
#include <cstdint>
#include <string.h>
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include <sstream>
#include <fstream>
#include <map>
#include <unordered_map>
#include <mutex>
#include "renderers/Renderer.h"
#include "Paths.h"
#include "math/Vector4f.h"

const MaxSizeInfo MaxSizeInfo::Empty;

namespace
{
	// Area average (box filter) of an RGBA32 image into a smaller one, as FreeImage's FILTER_BOX did
	void downscaleBox(const unsigned char* src, size_t srcPitch, size_t sw, size_t sh, unsigned char* dst, size_t dw, size_t dh)
	{
		for (size_t dy = 0; dy < dh; dy++)
		{
			size_t y0 = dy * sh / dh;
			size_t y1 = std::max(y0 + 1, ((dy + 1) * sh + dh - 1) / dh);

			for (size_t dx = 0; dx < dw; dx++)
			{
				size_t x0 = dx * sw / dw;
				size_t x1 = std::max(x0 + 1, ((dx + 1) * sw + dw - 1) / dw);

				uint64_t sum[4] = { 0, 0, 0, 0 };
				for (size_t y = y0; y < y1 && y < sh; y++)
				{
					const unsigned char* px = src + y * srcPitch + x0 * 4;
					for (size_t x = x0; x < x1 && x < sw; x++, px += 4)
						for (int c = 0; c < 4; c++)
							sum[c] += px[c];
				}

				uint64_t count = (std::min(y1, sh) - y0) * (std::min(x1, sw) - x0);
				unsigned char* out = dst + (dy * dw + dx) * 4;
				for (int c = 0; c < 4; c++)
					out[c] = (unsigned char)((sum[c] + count / 2) / count);
			}
		}
	}
}

// Decodes with SDL_image. Returns RGBA bytes (R, G, B, A in memory), bottom row first as the
// renderer expects, in a buffer to free with delete[]; subImageIndex picks a frame of an animation.
unsigned char* ImageIO::loadFromMemoryRGBA32(const unsigned char * data, const size_t size, size_t & width, size_t & height, MaxSizeInfo* maxSize, Vector2i* baseSize, Vector2i* packedSize, int subImageIndex)
{
	LOG(LogDebug) << "ImageIO::loadFromMemoryRGBA32";

	if (baseSize != nullptr)
		*baseSize = Vector2i(0, 0);

	if (packedSize != nullptr)
		*packedSize = Vector2i(0, 0);

	width = 0;
	height = 0;

	if (data == nullptr || size == 0)
		return nullptr;

	SDL_IOStream* io = SDL_IOFromConstMem(data, size);
	if (io == nullptr)
		return nullptr;

	IMG_Animation* animation = nullptr;
	SDL_Surface* decoded = nullptr;
	SDL_Surface* frame = nullptr; // decoded, or a frame of animation

	if (subImageIndex >= 0)
	{
		animation = IMG_LoadAnimation_IO(io, false);
		if (animation != nullptr && subImageIndex < animation->count)
			frame = animation->frames[subImageIndex];
		else if (animation == nullptr)
			SDL_SeekIO(io, 0, SDL_IO_SEEK_SET);
	}

	if (frame == nullptr && animation == nullptr)
		frame = decoded = IMG_Load_IO(io, false);

	SDL_CloseIO(io);

	SDL_Surface* rgba = frame != nullptr ? SDL_ConvertSurface(frame, SDL_PIXELFORMAT_RGBA32) : nullptr;

	if (decoded != nullptr)
		SDL_DestroySurface(decoded);
	if (animation != nullptr)
		IMG_FreeAnimation(animation);

	if (rgba == nullptr)
	{
		LOG(LogError) << "Error - Failed to load image from memory: " << SDL_GetError();
		return nullptr;
	}

	width = rgba->w;
	height = rgba->h;

	if (baseSize != nullptr)
		*baseSize = Vector2i(width, height);

	size_t outWidth = width;
	size_t outHeight = height;

	size_t maxX = maxSize == nullptr ? 0 : (size_t) Math::round(maxSize->x());
	size_t maxY = maxSize == nullptr ? 0 : (size_t) Math::round(maxSize->y());

	if (maxSize != nullptr && maxX > 0 && maxY > 0 && (width > maxX || height > maxY))
	{
		Vector2i sz = adjustPictureSize(Vector2i(width, height), Vector2i(maxX, maxY), maxSize->externalZoom());

		if (sz.x() > Renderer::getScreenWidth() || sz.y() > Renderer::getScreenHeight())
			sz = adjustPictureSize(sz, Vector2i(Renderer::getScreenWidth(), Renderer::getScreenHeight()), false);

		// only ever shrinks: a bigger size here would be the screen limit of an extern zoom
		if (sz.x() > 0 && sz.y() > 0 && (size_t)sz.x() <= width && (size_t)sz.y() <= height && (sz.x() != (int)width || sz.y() != (int)height))
		{
			LOG(LogDebug) << "ImageIO : rescaling image from " << width << "x" << height << " to " << sz.x() << "x" << sz.y();
			outWidth = sz.x();
			outHeight = sz.y();

			if (packedSize != nullptr)
				*packedSize = Vector2i(outWidth, outHeight);
		}
	}

	unsigned char* result = new unsigned char[outWidth * outHeight * 4];
	const unsigned char* pixels = (const unsigned char*)rgba->pixels;

	if (outWidth != width || outHeight != height)
	{
		downscaleBox(pixels, rgba->pitch, width, height, result, outWidth, outHeight);
		flipPixelsVert(result, outWidth, outHeight);
	}
	else
	{
		for (size_t y = 0; y < height; y++)
			memcpy(result + (height - 1 - y) * width * 4, pixels + y * rgba->pitch, width * 4);
	}

	SDL_DestroySurface(rgba);

	width = outWidth;
	height = outHeight;
	return result;
}

void ImageIO::flipPixelsVert(unsigned char* imagePx, const size_t& width, const size_t& height)
{
	unsigned int temp;
	unsigned int* arr = (unsigned int*)imagePx;
	for(size_t y = 0; y < height / 2; y++)
	{
		for(size_t x = 0; x < width; x++)
		{
			temp = arr[x + (y * width)];
			arr[x + (y * width)] = arr[x + (height * width) - ((y + 1) * width)];
			arr[x + (height * width) - ((y + 1) * width)] = temp;
		}
	}
}

Vector2f ImageIO::adjustPictureSizeF(Vector2f imageSize, Vector2f maxSize, bool externSize)
{
	return adjustPictureSizeF(imageSize.x(), imageSize.y(), maxSize.x(), maxSize.y(), externSize);
}

Vector2f ImageIO::adjustPictureSizeF(float cxDIB, float cyDIB, float iMaxX, float iMaxY, bool externSize)
{
	if (externSize)
		return getPictureMinSize(Vector2f(cxDIB, cyDIB), Vector2f(iMaxX, iMaxY));

	if (cxDIB == 0 || cyDIB == 0)
		return Vector2f(cxDIB, cyDIB);

	float xCoef = iMaxX / cxDIB;
	float yCoef = iMaxY / cyDIB;

	cyDIB = cyDIB * std::max(xCoef, yCoef);
	cxDIB = cxDIB * std::max(xCoef, yCoef);

	if (cxDIB > iMaxX)
	{
		cyDIB = cyDIB * iMaxX / cxDIB;
		cxDIB = iMaxX;
	}

	if (cyDIB > iMaxY)
	{
		cxDIB = cxDIB * iMaxY / cyDIB;
		cyDIB = iMaxY;
	}

	return Vector2f(cxDIB, cyDIB);
}

Vector2i ImageIO::adjustPictureSize(Vector2i imageSize, Vector2i maxSize, bool externSize)
{
	if (externSize)
	{
		Vector2f szf = getPictureMinSize(Vector2f(imageSize.x(), imageSize.y()), Vector2f(maxSize.x(), maxSize.y()));
		return Vector2i(szf.x(), szf.y());
	}

	int cxDIB = imageSize.x();
	int cyDIB = imageSize.y();

	if (cxDIB == 0 || cyDIB == 0)
		return imageSize;

	int iMaxX = maxSize.x();
	int iMaxY = maxSize.y();

	double xCoef = (double)iMaxX / (double)cxDIB;
	double yCoef = (double)iMaxY / (double)cyDIB;

	cyDIB = (int)((double)cyDIB * std::max(xCoef, yCoef));
	cxDIB = (int)((double)cxDIB * std::max(xCoef, yCoef));

	if (cxDIB > iMaxX)
	{
		cyDIB = (int)((double)cyDIB * (double)iMaxX / (double)cxDIB);
		cxDIB = iMaxX;
	}

	if (cyDIB > iMaxY)
	{
		cxDIB = (int)((double)cxDIB * (double)iMaxY / (double)cyDIB);
		cyDIB = iMaxY;
	}

	return Vector2i(cxDIB, cyDIB);
}

Vector2f ImageIO::getPictureMinSize(Vector2f imageSize, Vector2f maxSize)
{
	if (imageSize.x() == 0 || imageSize.y() == 0)
		return imageSize;

	float cxDIB = maxSize.x();
	float cyDIB = maxSize.y();

	float xCoef = maxSize.x() / imageSize.x();
	float yCoef = maxSize.y() / imageSize.y();

	if (imageSize.x() * yCoef < maxSize.x())
		cyDIB = imageSize.y() * xCoef;
	else
		cxDIB = imageSize.x() * yCoef;

	return Vector2f(cxDIB, cyDIB);
}

bool ImageIO::getMultiBitmapInformation(const std::string& path, int& totalFrames, int& frameTime)
{
	totalFrames = 1;
	frameTime = 0;

	IMG_Animation* animation = IMG_LoadAnimation(path.c_str());
	if (animation == nullptr)
		return false;

	bool result = false;
	if (animation->count > 1)
	{
		totalFrames = animation->count;
		frameTime = animation->delays != nullptr ? animation->delays[0] : 0;
		if (frameTime <= 0)
			frameTime = 100;

		result = true;
	}

	IMG_FreeAnimation(animation);
	return result;
}
