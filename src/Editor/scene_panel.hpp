#pragma once

#include <string>

namespace lve::editor {

  struct ScenePanelState {
    std::string path{"Assets/scene.json"};
  };

  struct ScenePanelActions {
    bool saveRequested{false};
    bool loadRequested{false};
  };

  ScenePanelActions BuildScenePanel(ScenePanelState &state);

} // namespace lve::editor
