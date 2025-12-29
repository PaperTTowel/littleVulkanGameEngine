#pragma once

#include "Backend/window.hpp"
#include "Backend/device.hpp"
#include "Rendering/renderer.hpp"
#include "Rendering/render_context.hpp"
#include "Editor/editor_system.hpp"
#include "Engine/scene_system.hpp"

// std
#include <memory>

namespace lve {
  class EngineLoop {
  public:
    static constexpr int WIDTH = 800;
    static constexpr int HEIGHT = 600;

    EngineLoop();
    ~EngineLoop();

    EngineLoop(const LveWindow &) = delete;
    EngineLoop &operator=(const LveWindow &) = delete;

    void run();

  private:
    LveWindow lveWindow{WIDTH, HEIGHT, "Hello Vulkan!"};
    LveDevice lveDevice{lveWindow};
    LveRenderer lveRenderer{lveWindow, lveDevice};
    RenderContext renderContext{lveDevice, lveRenderer};
    LveGameObject::id_t viewerId;
    bool useOrthoCamera{false};
    bool wireframeEnabled{false};
    bool normalViewEnabled{false};
    EditorSystem editorSystem{lveWindow, lveDevice};
    editor::ResourceBrowserState resourceBrowserState{};
    SceneSystem sceneSystem{lveDevice};
  };
} // namespace lve

