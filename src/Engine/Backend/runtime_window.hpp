#pragma once

#include "Engine/Backend/input.hpp"
#include "Engine/Backend/render_types.hpp"

namespace lve::backend {
  class WindowBackend {
  public:
    virtual ~WindowBackend() = default;

    virtual void pollEvents() = 0;
    virtual bool shouldClose() const = 0;
    virtual RenderExtent getExtent() const = 0;
    virtual InputProvider &input() = 0;
    virtual const InputProvider &input() const = 0;
  };
} // namespace lve::backend
