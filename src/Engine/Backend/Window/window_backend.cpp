#include "Engine/Backend/Window/window_backend.hpp"

namespace lve::backend {

  void GlfwWindowBackend::pollEvents() {
    window.pollEvents();
  }

  bool GlfwWindowBackend::shouldClose() const {
    return window.shouldClose();
  }

  RenderExtent GlfwWindowBackend::getExtent() const {
    return window.getExtent();
  }

} // namespace lve::backend
