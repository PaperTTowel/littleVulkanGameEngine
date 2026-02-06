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
    float consumeScrollDeltaY() {
      const float delta = scrollDeltaY;
      scrollDeltaY = 0.f;
      return delta;
    }
    GLFWwindow *getGLFWwindow() const { return window; }

    void pollEvents();
    void waitEvents();
    std::vector<const char *> getRequiredInstanceExtensions() const;
    void createWindowSurface(VkInstance instance, VkSurfaceKHR *surface);

  private:
    static void framebufferResizeCallback(GLFWwindow *window, int width, int height);
    static void scrollCallback(GLFWwindow *window, double xoffset, double yoffset);
    void initWindow();
#ifdef _WIN32
    void setWindowIconWin32();
#endif

    int width;
    int height;
    bool framebufferResized{false};
    float scrollDeltaY{0.f};
    WindowClientApi clientApi{WindowClientApi::Vulkan};

    std::string windowName;
    GLFWwindow *window{nullptr};
#ifdef _WIN32
    void *windowIconLarge{nullptr};
    void *windowIconSmall{nullptr};
#endif
  };
} // namespace lve
