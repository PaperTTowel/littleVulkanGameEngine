#pragma once

#include <string>

namespace lve {

  struct AssetDefaults {
    std::string rootPath{"Assets"};
    std::string activeMeshPath{"Assets/models/colored_cube.obj"};
    std::string activeSpriteMetaPath{"Assets/textures/characters/player.json"};
    std::string activeMaterialPath{};
  };

} // namespace lve
