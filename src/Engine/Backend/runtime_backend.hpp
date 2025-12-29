#pragma once

#include "Engine/Backend/editor_render_backend.hpp"
#include "Engine/Backend/render_backend.hpp"
#include "Engine/Backend/runtime_window.hpp"

namespace lve {
  class SceneSystem;
}

namespace lve::backend {
  class RuntimeBackend {
  public:
    virtual ~RuntimeBackend() = default;

    virtual WindowBackend &window() = 0;
    virtual RenderBackend &renderBackend() = 0;
    virtual EditorRenderBackend &editorBackend() = 0;
    virtual SceneSystem &sceneSystem() = 0;
  };
} // namespace lve::backend
