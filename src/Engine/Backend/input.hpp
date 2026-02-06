#pragma once

namespace lve::backend {
  enum class MouseButton {
    Left,
    Right,
    Middle
  };

  enum class KeyCode {
    A,
    D,
    W,
    S,
    Space,
    E,
    H,
    J,
    K,
    F3,
    U,
    I,
    Y,
    Left,
    Right,
    Up,
    Down
  };

  class InputProvider {
  public:
    virtual ~InputProvider() = default;
    virtual bool isKeyPressed(KeyCode code) const = 0;
    virtual bool isMouseButtonPressed(MouseButton button) const = 0;
    virtual float consumeMouseWheelDeltaY() = 0;
  };
} // namespace lve::backend
