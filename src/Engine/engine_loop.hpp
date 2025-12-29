#pragma once

#include "Engine/Backend/runtime_backend.hpp"
#include "Editor/editor_system.hpp"

// std
#include <memory>

namespace lve {
  namespace backend {
    class EditorRenderBackend;
  }

  class EngineLoop {
  public:
    static constexpr int WIDTH = 800;
    static constexpr int HEIGHT = 600;

    EngineLoop();
    ~EngineLoop();

    EngineLoop(const EngineLoop &) = delete;
    EngineLoop &operator=(const EngineLoop &) = delete;

    void run();

  private:
    std::unique_ptr<backend::RuntimeBackend> runtime;
    std::unique_ptr<EditorSystem> editorSystem;
    LveGameObject::id_t viewerId{0};
    bool useOrthoCamera{false};
    bool wireframeEnabled{false};
    bool normalViewEnabled{false};
    editor::ResourceBrowserState resourceBrowserState{};
  };
} // namespace lve



