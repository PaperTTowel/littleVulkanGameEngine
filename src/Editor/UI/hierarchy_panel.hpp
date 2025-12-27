#pragma once

#include "utils/game_object.hpp"

#include <optional>

namespace lve::editor {

  enum class HierarchyCreateRequest { None, Sprite, Mesh, PointLight };

  struct HierarchyPanelState {
    std::optional<LveGameObject::id_t> selectedId{};
    int selectedNodeIndex{-1};
  };

  struct HierarchyActions {
    HierarchyCreateRequest createRequest{HierarchyCreateRequest::None};
    bool deleteSelected{false};
  };

  HierarchyActions BuildHierarchyPanel(
    LveGameObjectManager &manager,
    HierarchyPanelState &state,
    LveGameObject::id_t protectedId,
    bool *open);

} // namespace lve::editor
