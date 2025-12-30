#pragma once

#include "Engine/material_data.hpp"

#include <functional>
#include <string>

namespace lve {

  bool saveMaterialToFile(
    const std::string &path,
    const MaterialData &data,
    std::string *outError = nullptr);

  bool loadMaterialDataFromFile(
    const std::string &path,
    MaterialData &outData,
    std::string *outError = nullptr,
    const std::function<std::string(const std::string &)> &pathResolver = {});

} // namespace lve
