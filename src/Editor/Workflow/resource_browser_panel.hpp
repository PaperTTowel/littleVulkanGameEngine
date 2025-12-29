#pragma once

#include "utils/game_object.hpp"

#include <string>
#include <vector>

namespace lve::editor {

  struct BrowserState {
    std::string rootPath{"Assets"};
    std::string currentPath{"Assets"};
    std::vector<std::string> directories{};
    std::vector<std::string> files{};
    int selectedDir{-1};
    int selectedFile{-1};
    std::string filter{};
    bool pendingRefresh{true};
    bool restrictToRoot{true};
  };

  struct ResourceBrowserState {
    BrowserState browser{};
    std::string activeMeshPath{"Assets/models/colored_cube.obj"};
    std::string activeSpriteMetaPath{"Assets/textures/characters/player.json"};
  };

  struct FileDialogState {
    BrowserState browser{};
    std::string title{"Import"};
    std::string okLabel{"Open"};
    bool allowDirectories{false};
  };

  struct ResourceBrowserActions {
    bool refreshRequested{false};
    bool setActiveMesh{false};
    bool setActiveSpriteMeta{false};
    bool applyMeshToSelection{false};
    bool applySpriteMetaToSelection{false};
  };

  struct FileDialogActions {
    bool accepted{false};
    bool canceled{false};
    std::string selectedPath{};
  };

  ResourceBrowserActions BuildResourceBrowserPanel(
    ResourceBrowserState &state,
    const LveGameObject *selected,
    bool *open);

  FileDialogActions BuildFileDialogPanel(
    FileDialogState &state,
    bool *open);

} // namespace lve::editor
