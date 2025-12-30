#pragma once

#include "Engine/Backend/model_data.hpp"

#include <string>

namespace lve {

  bool loadModelDataFromFile(
    const std::string &path,
    backend::ModelData &outData,
    std::string *outError = nullptr);

} // namespace lve
