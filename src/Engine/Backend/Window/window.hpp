#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "Engine/Backend/render_types.hpp"

#include <string>
#include <vector>

namespace lve {

  enum class WindowClientApi {
    Vulkan,
    OpenGL
  };

  struct WindowConfig {
    int width{0};
    int height{0};
    std::string title{};
    WindowClientApi clientApi{WindowClientApi::Vulkan};
  };

  class LveWindow {
  public:
    explicit LveWindow(const WindowConfig &config);
    LveWindow(int w, int h, std::string name, WindowClientApi clientApi = WindowClientApi::Vulkan);
    ~LveWindow();

    LveWindow(const LveWindow &) = delete;
    LveWindow &operator=(const LveWindow &) = delete;

    bool shouldClose() { return glfwWindowShouldClose(window); }
    backend::RenderExtent getExtent() const {
      return backend::RenderExtent{static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)};
    }
    bool wasWindowResized() const { return framebufferResized; }
    void resetWindowResizedFlag() { framebufferResized = false; }
    GLFWwindow *getGLFWwindow() const { return window; }

    void pollEvents();
    void waitEvents();
    std::vector<const char *> getRequiredInstanceExtensions() const;
    void createWindowSurface(VkInstance instance, VkSurfaceKHR *surface);

  private:
    static void framebufferResizeCallback(GLFWwindow *window, int width, int height);
    void initWindow();

    int width;
    int height;
    bool framebufferResized{false};
    WindowClientApi clientApi{WindowClientApi::Vulkan};

    std::string windowName;
    GLFWwindow *window{nullptr};
  };
} // namespace lve
