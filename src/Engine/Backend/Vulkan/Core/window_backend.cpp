#include "Engine/Backend/Vulkan/Core/window_backend.hpp"

namespace lve::backend {

  void VulkanWindowBackend::pollEvents() {
    glfwPollEvents();
  }

  bool VulkanWindowBackend::shouldClose() const {
    return window.shouldClose();
  }

  RenderExtent VulkanWindowBackend::getExtent() const {
    const VkExtent2D extent = window.getExtent();
    return RenderExtent{extent.width, extent.height};
  }

} // namespace lve::backend
