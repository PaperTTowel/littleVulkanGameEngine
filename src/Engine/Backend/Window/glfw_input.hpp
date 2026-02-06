#pragma once

#include "Engine/Backend/input.hpp"
#include "Engine/Backend/Window/window.hpp"

namespace lve::backend {
  class GlfwInputProvider final : public InputProvider {
  public:
    explicit GlfwInputProvider(LveWindow &window) : window{window} {}
    bool isKeyPressed(KeyCode code) const override;
    bool isMouseButtonPressed(MouseButton button) const override;
    float consumeMouseWheelDeltaY() override;

  private:
    static int toGlfwKey(KeyCode code);
    static int toGlfwMouseButton(MouseButton button);
    LveWindow &window;
  };
} // namespace lve::backend
