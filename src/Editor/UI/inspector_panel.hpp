#pragma once

#include "utils/game_object.hpp"
#include "utils/sprite_animator.hpp"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

namespace lve::editor {

  struct GizmoContext {
    void *drawList{nullptr};
    float x{0.f};
    float y{0.f};
    float width{0.f};
    float height{0.f};
    bool valid{false};
  };

  struct TransformSnapshot {
    glm::vec3 translation{};
    glm::vec3 rotation{};
    glm::vec3 scale{1.f};
  };

  struct InspectorActions {
    bool transformChanged{false};
    bool transformCommitted{false};
    TransformSnapshot beforeTransform{};
    TransformSnapshot afterTransform{};
    bool nameChanged{false};
    std::string beforeName{};
    std::string afterName{};
    bool nodeOverridesChanged{false};
    bool nodeOverridesCommitted{false};
    std::vector<NodeTransformOverride> beforeNodeOverrides{};
    std::vector<NodeTransformOverride> afterNodeOverrides{};
  };

  InspectorActions BuildInspectorPanel(
    LveGameObject *selected,
    SpriteAnimator *animator,
    const glm::mat4 &view,
    const glm::mat4 &projection,
    VkExtent2D viewportExtent,
    bool *open,
    const GizmoContext &gizmoContext,
    int gizmoOperation,
    int gizmoMode,
    int &selectedNodeIndex);

} // namespace lve::editor
