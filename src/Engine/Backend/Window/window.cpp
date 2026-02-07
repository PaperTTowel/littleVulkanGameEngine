#include "Engine/Backend/Window/window.hpp"
#include "Engine/path_utils.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifndef GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#endif
#include <GLFW/glfw3native.h>
#include <array>
#include <filesystem>
#endif

#include <iostream>
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
#ifdef _WIN32
    if (windowIconLarge) {
      DestroyIcon(reinterpret_cast<HICON>(windowIconLarge));
      windowIconLarge = nullptr;
    }
    if (windowIconSmall) {
      DestroyIcon(reinterpret_cast<HICON>(windowIconSmall));
      windowIconSmall = nullptr;
    }
#endif

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

#ifdef _WIN32
    setWindowIconWin32();
#endif

    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    glfwSetScrollCallback(window, scrollCallback);
  }

#ifdef _WIN32
  void LveWindow::setWindowIconWin32() {
    if (!window) {
      return;
    }

    namespace fs = std::filesystem;
    const std::array<fs::path, 8> candidates{{
      fs::path{"Assets/textures/icon.ico"},
      fs::path{"../Assets/textures/icon.ico"},
      fs::path{"../../Assets/textures/icon.ico"},
      fs::path{"../../../Assets/textures/icon.ico"},
      fs::path{"../../../../Assets/textures/icon.ico"},
      fs::path{"src/Assets/textures/icon.ico"},
      fs::path{"../src/Assets/textures/icon.ico"},
      fs::path{"icon.ico"},
    }};

    fs::path iconPath{};
    for (const auto &candidate : candidates) {
      if (fs::exists(candidate)) {
        iconPath = candidate;
        break;
      }
    }
    if (iconPath.empty()) {
      std::cerr << "Window icon not found (expected Assets/textures/icon.ico).\n";
      return;
    }

    const int iconW = GetSystemMetrics(SM_CXICON);
    const int iconH = GetSystemMetrics(SM_CYICON);
    const int smallW = GetSystemMetrics(SM_CXSMICON);
    const int smallH = GetSystemMetrics(SM_CYSMICON);
    const std::wstring iconPathWide = iconPath.wstring();

    HICON hIconLarge = static_cast<HICON>(LoadImageW(
      nullptr,
      iconPathWide.c_str(),
      IMAGE_ICON,
      iconW,
      iconH,
      LR_LOADFROMFILE));
    HICON hIconSmall = static_cast<HICON>(LoadImageW(
      nullptr,
      iconPathWide.c_str(),
      IMAGE_ICON,
      smallW,
      smallH,
      LR_LOADFROMFILE));

    if (!hIconLarge && !hIconSmall) {
      std::cerr << "Failed to load window icon: " << pathutil::toUtf8(iconPath) << "\n";
      return;
    }

    HWND hwnd = glfwGetWin32Window(window);
    if (!hwnd) {
      if (hIconLarge) DestroyIcon(hIconLarge);
      if (hIconSmall) DestroyIcon(hIconSmall);
      return;
    }

    if (hIconLarge) {
      SendMessage(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hIconLarge));
      windowIconLarge = hIconLarge;
    }
    if (hIconSmall) {
      SendMessage(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hIconSmall));
      windowIconSmall = hIconSmall;
    }
  }
#endif

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
    if (!lveWindow) {
      return;
    }
    lveWindow->framebufferResized = true;
    lveWindow->width = width;
    lveWindow->height = height;
  }

  void LveWindow::scrollCallback(GLFWwindow *window, double xoffset, double yoffset) {
    auto lveWindow = reinterpret_cast<LveWindow *>(glfwGetWindowUserPointer(window));
    if (!lveWindow) {
      return;
    }
    (void)xoffset;
    lveWindow->scrollDeltaY += static_cast<float>(yoffset);
  }

} // namespace lve
