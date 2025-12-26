#pragma once

#include "utils/game_object.hpp"

#include <string>
#include <vector>

namespace lve::editor {

  struct ResourceBrowserState {
    std::string rootPath{"Assets"};
    std::string currentPath{"Assets"};
    std::vector<std::string> directories{};
    std::vector<std::string> files{};
    int selectedDir{-1};
    int selectedFile{-1};
    std::string filter{};
    std::string activeMeshPath{"Assets/models/colored_cube.obj"};
    std::string activeSpriteMetaPath{"Assets/textures/characters/player.json"};
    bool pendingRefresh{true};
  };

  struct ResourceBrowserActions {
    bool refreshRequested{false};
    bool setActiveMesh{false};
    bool setActiveSpriteMeta{false};
    bool applyMeshToSelection{false};
    bool applySpriteMetaToSelection{false};
  };

  ResourceBrowserActions BuildResourceBrowserPanel(
    ResourceBrowserState &state,
    const LveGameObject *selected,
    bool *open);

} // namespace lve::editor
