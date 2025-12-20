#pragma once

#include "utils/game_object.hpp"

#include <string>
#include <vector>

namespace lve::editor {

  struct ResourceBrowserState {
    std::vector<std::string> meshFiles{};
    std::vector<std::string> spriteMetaFiles{};
    int selectedMesh{-1};
    int selectedSpriteMeta{-1};
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
    const LveGameObject *selected);

} // namespace lve::editor
