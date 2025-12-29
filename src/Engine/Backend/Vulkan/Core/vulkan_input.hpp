#pragma once

#include "Engine/Backend/input.hpp"
#include "Engine/Backend/Vulkan/Core/window.hpp"

namespace lve::backend {
  class VulkanInputProvider final : public InputProvider {
  public:
    explicit VulkanInputProvider(LveWindow &window) : window{window} {}

    bool isKeyPressed(KeyCode code) const override;

  private:
    LveWindow &window;
    static int toGlfwKey(KeyCode code);
  };
} // namespace lve::backend
