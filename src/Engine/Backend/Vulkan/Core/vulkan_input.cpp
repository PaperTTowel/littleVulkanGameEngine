#include "Engine/Backend/Vulkan/Core/vulkan_input.hpp"

namespace lve::backend {

  int VulkanInputProvider::toGlfwKey(KeyCode code) {
    switch (code) {
      case KeyCode::A: return GLFW_KEY_A;
      case KeyCode::D: return GLFW_KEY_D;
      case KeyCode::W: return GLFW_KEY_W;
      case KeyCode::S: return GLFW_KEY_S;
      case KeyCode::H: return GLFW_KEY_H;
      case KeyCode::J: return GLFW_KEY_J;
      case KeyCode::K: return GLFW_KEY_K;
      case KeyCode::U: return GLFW_KEY_U;
      case KeyCode::I: return GLFW_KEY_I;
      case KeyCode::Y: return GLFW_KEY_Y;
      case KeyCode::Left: return GLFW_KEY_LEFT;
      case KeyCode::Right: return GLFW_KEY_RIGHT;
      case KeyCode::Up: return GLFW_KEY_UP;
      case KeyCode::Down: return GLFW_KEY_DOWN;
    }
    return GLFW_KEY_UNKNOWN;
  }

  bool VulkanInputProvider::isKeyPressed(KeyCode code) const {
    const int key = toGlfwKey(code);
    if (key == GLFW_KEY_UNKNOWN) {
      return false;
    }
    return glfwGetKey(window.getGLFWwindow(), key) == GLFW_PRESS;
  }

} // namespace lve::backend
