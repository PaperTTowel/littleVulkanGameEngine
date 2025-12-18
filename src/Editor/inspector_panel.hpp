#pragma once

#include "utils/game_object.hpp"
#include "utils/sprite_animator.hpp"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

namespace lve::editor {

  void BuildInspectorPanel(
    LveGameObject *selected,
    SpriteAnimator *animator,
    const glm::mat4 &view,
    const glm::mat4 &projection,
    VkExtent2D viewportExtent);

} // namespace lve::editor
