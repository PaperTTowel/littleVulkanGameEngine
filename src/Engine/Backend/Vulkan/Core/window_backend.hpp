#pragma once

#include "Engine/Backend/runtime_window.hpp"
#include "Engine/Backend/Vulkan/Core/vulkan_input.hpp"

namespace lve::backend {
  class VulkanWindowBackend final : public WindowBackend {
  public:
    VulkanWindowBackend(LveWindow &window, VulkanInputProvider &inputProvider)
      : window{window}, inputProvider{inputProvider} {}

    void pollEvents() override;
    bool shouldClose() const override;
    RenderExtent getExtent() const override;
    InputProvider &input() override { return inputProvider; }
    const InputProvider &input() const override { return inputProvider; }

  private:
    LveWindow &window;
    VulkanInputProvider &inputProvider;
  };
} // namespace lve::backend
