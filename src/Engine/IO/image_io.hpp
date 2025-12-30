#pragma once

#include "Engine/Backend/model_data.hpp"
#include "Engine/IO/image_data.hpp"

#include <cstddef>
#include <string>

namespace lve {

  bool loadImageDataFromFile(
    const std::string &path,
    ImageData &outData,
    std::string *outError = nullptr,
    bool flipVertically = true);

  bool loadImageDataFromMemory(
    const unsigned char *data,
    std::size_t size,
    ImageData &outData,
    std::string *outError = nullptr,
    bool flipVertically = false);

  bool loadImageDataFromRgba(
    const unsigned char *rgbaPixels,
    int width,
    int height,
    ImageData &outData,
    std::string *outError = nullptr);

  bool loadImageDataFromTextureSource(
    const backend::ModelTextureSource &source,
    ImageData &outData,
    std::string *outError = nullptr);

} // namespace lve
