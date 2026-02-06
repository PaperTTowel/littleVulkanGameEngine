#pragma once

#include "game_object.hpp"
#include "Engine/Backend/input.hpp"

namespace lve {
  class KeyboardMovementController {

  public:
    struct KeyMappings {
      backend::KeyCode moveLeft = backend::KeyCode::H;
      backend::KeyCode moveRight = backend::KeyCode::K;
      backend::KeyCode moveForward = backend::KeyCode::U;
      backend::KeyCode moveBackward = backend::KeyCode::J;
      backend::KeyCode moveUp = backend::KeyCode::I;
      backend::KeyCode moveDown = backend::KeyCode::Y;
      backend::KeyCode lookLeft = backend::KeyCode::Left;
      backend::KeyCode lookRight = backend::KeyCode::Right;
      backend::KeyCode lookUp = backend::KeyCode::Up;
      backend::KeyCode lookDown = backend::KeyCode::Down;
    };

    void moveInPlaneXZ(backend::InputProvider &input, float dt, LveGameObject &gameObject);

    KeyMappings keys{};
    float moveSpeed{3.f};
    float lookSpeed{1.5f};
  };

  class CharacterMovementController {
  public:
    struct KeyMappings {
      backend::KeyCode moveLeft = backend::KeyCode::A;
      backend::KeyCode moveRight = backend::KeyCode::D;
      backend::KeyCode moveForward = backend::KeyCode::W;
      backend::KeyCode moveBackward = backend::KeyCode::S;
      backend::KeyCode jump = backend::KeyCode::Space;
    };

    glm::vec3 moveInPlaneXZ(backend::InputProvider &input, float dt, LveGameObject &gameObject);

    KeyMappings keys{};
    float moveSpeed{2.f};
    float lookSpeed{1.5f};
  };
} // namespace lve
