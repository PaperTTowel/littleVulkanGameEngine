#pragma once

namespace lve::backend {
  enum class KeyCode {
    A,
    D,
    W,
    S,
    H,
    J,
    K,
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
  };
} // namespace lve::backend
