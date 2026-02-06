#pragma once

#include "Game/Tilemap/tiled_map.hpp"

#include <string>

namespace lve::tilemap {
  bool loadFromFile(const std::string &path, TiledMap &outMap, std::string *outError);
} // namespace lve::tilemap
