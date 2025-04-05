#pragma once

#include "lve_game_object.hpp"
#include "lve_window.hpp"

namespace lve {
  class KeyboardMovementController {

  public:
    struct KeyMappings {
      int moveLeft = GLFW_KEY_H;
      int moveRight = GLFW_KEY_K;
      int moveForward = GLFW_KEY_U;
      int moveBackward = GLFW_KEY_J;
      int moveUp = GLFW_KEY_I;
      int moveDown = GLFW_KEY_Y;
      int lookLeft = GLFW_KEY_LEFT;
      int lookRight = GLFW_KEY_RIGHT;
      int lookUp = GLFW_KEY_UP;
      int lookDown = GLFW_KEY_DOWN;
    };

    void moveInPlaneXZ(GLFWwindow *window, float dt, LveGameObject &gameObject);

    KeyMappings keys{};
    float moveSpeed{3.f};
    float lookSpeed{1.5f};
  };

  class CharacterMovementController {
  public:
    struct KeyMappings {
      int moveLeft = GLFW_KEY_A;
      int moveRight = GLFW_KEY_D;
      int moveForward = GLFW_KEY_W;
      int moveBackward = GLFW_KEY_S;
    };

    void moveInPlaneXZ(GLFWwindow *window, float dt, LveGameObject &gameObject);

    KeyMappings keys{};
    float moveSpeed{2.f};
    float lookSpeed{1.5f};
  };
} // namespace lve