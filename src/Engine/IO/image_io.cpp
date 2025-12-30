#include "Engine/IO/image_io.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace lve {

  namespace {
    void setError(std::string *outError, const char *message) {
      if (outError) {
        *outError = message ? message : "unknown error";
      }
    }
  } // namespace

  bool loadImageDataFromFile(
    const std::string &path,
    ImageData &outData,
    std::string *outError,
    bool flipVertically) {
    outData = ImageData{};
    if (path.empty()) {
      setError(outError, "empty image path");
      return false;
    }

    stbi_set_flip_vertically_on_load(flipVertically ? 1 : 0);
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc *pixels = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (!pixels) {
      setError(outError, stbi_failure_reason());
      return false;
    }

    const std::size_t pixelCount = static_cast<std::size_t>(width) * height;
    outData.width = width;
    outData.height = height;
    outData.channels = 4;
    outData.pixels.assign(pixels, pixels + pixelCount * 4);
    stbi_image_free(pixels);
    return true;
  }

  bool loadImageDataFromMemory(
    const unsigned char *data,
    std::size_t size,
    ImageData &outData,
    std::string *outError,
    bool flipVertically) {
    outData = ImageData{};
    if (!data || size == 0) {
      setError(outError, "empty image buffer");
      return false;
    }

    stbi_set_flip_vertically_on_load(flipVertically ? 1 : 0);
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc *pixels = stbi_load_from_memory(
      data,
      static_cast<int>(size),
      &width,
      &height,
      &channels,
      STBI_rgb_alpha);
    if (!pixels) {
      setError(outError, stbi_failure_reason());
      return false;
    }

    const std::size_t pixelCount = static_cast<std::size_t>(width) * height;
    outData.width = width;
    outData.height = height;
    outData.channels = 4;
    outData.pixels.assign(pixels, pixels + pixelCount * 4);
    stbi_image_free(pixels);
    return true;
  }

  bool loadImageDataFromRgba(
    const unsigned char *rgbaPixels,
    int width,
    int height,
    ImageData &outData,
    std::string *outError) {
    outData = ImageData{};
    if (!rgbaPixels || width <= 0 || height <= 0) {
      setError(outError, "invalid RGBA pixel data");
      return false;
    }

    const std::size_t pixelCount = static_cast<std::size_t>(width) * height;
    outData.width = width;
    outData.height = height;
    outData.channels = 4;
    outData.pixels.assign(rgbaPixels, rgbaPixels + pixelCount * 4);
    return true;
  }

  bool loadImageDataFromTextureSource(
    const backend::ModelTextureSource &source,
    ImageData &outData,
    std::string *outError) {
    switch (source.kind) {
      case backend::ModelTextureSource::Kind::File:
        return loadImageDataFromFile(source.path, outData, outError, true);
      case backend::ModelTextureSource::Kind::EmbeddedCompressed:
        return loadImageDataFromMemory(source.data.data(), source.data.size(), outData, outError, false);
      case backend::ModelTextureSource::Kind::EmbeddedRaw:
        return loadImageDataFromRgba(source.data.data(), source.width, source.height, outData, outError);
      case backend::ModelTextureSource::Kind::None:
      default:
        setError(outError, "texture source is empty");
        return false;
    }
  }

} // namespace lve
