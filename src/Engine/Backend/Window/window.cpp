#include "Engine/Backend/Window/window.hpp"

#include <stdexcept>
#include <utility>

namespace lve {

  LveWindow::LveWindow(const WindowConfig &config)
    : width{config.width}
    , height{config.height}
    , clientApi{config.clientApi}
    , windowName{config.title} {
    initWindow();
  }

  LveWindow::LveWindow(int w, int h, std::string name, WindowClientApi clientApi)
    : width{w}
    , height{h}
    , clientApi{clientApi}
    , windowName{std::move(name)} {
    initWindow();
  }

  LveWindow::~LveWindow() {
    if (window) {
      glfwDestroyWindow(window);
      window = nullptr;
    }
    glfwTerminate();
  }

  void LveWindow::initWindow() {
    if (!glfwInit()) {
      throw std::runtime_error("Failed to initialize GLFW");
    }

    if (clientApi == WindowClientApi::Vulkan) {
      glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    } else {
      glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    }
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window = glfwCreateWindow(width, height, windowName.c_str(), nullptr, nullptr);
    if (!window) {
      glfwTerminate();
      throw std::runtime_error("Failed to create GLFW window");
    }

    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
  }

  void LveWindow::pollEvents() { glfwPollEvents(); }

  void LveWindow::waitEvents() { glfwWaitEvents(); }

  std::vector<const char *> LveWindow::getRequiredInstanceExtensions() const {
    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    if (!glfwExtensions || glfwExtensionCount == 0) {
      return {};
    }
    return std::vector<const char *>(glfwExtensions, glfwExtensions + glfwExtensionCount);
  }

  void LveWindow::createWindowSurface(VkInstance instance, VkSurfaceKHR *surface) {
    if (glfwCreateWindowSurface(instance, window, nullptr, surface) != VK_SUCCESS) {
      throw std::runtime_error("failed to create window surface");
    }
  }

  void LveWindow::framebufferResizeCallback(GLFWwindow *window, int width, int height) {
    auto lveWindow = reinterpret_cast<LveWindow *>(glfwGetWindowUserPointer(window));
    lveWindow->framebufferResized = true;
    lveWindow->width = width;
    lveWindow->height = height;
  }

} // namespace lve
