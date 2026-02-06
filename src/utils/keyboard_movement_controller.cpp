#include "keyboard_movement_controller.hpp"

#include "Engine/coords.hpp"

#include <limits>

namespace lve {

  void KeyboardMovementController::moveInPlaneXZ(backend::InputProvider &input, float dt, LveGameObject &gameObject) {
    glm::vec3 rotate{0};
    if (input.isKeyPressed(keys.lookRight)) rotate.y += 1.f;
    if (input.isKeyPressed(keys.lookLeft)) rotate.y -= 1.f;
    if (input.isKeyPressed(keys.lookUp)) rotate.x += 1.f;
    if (input.isKeyPressed(keys.lookDown)) rotate.x -= 1.f;

    if (glm::dot(rotate, rotate) > std::numeric_limits<float>::epsilon()) {
      gameObject.transform.rotation += lookSpeed * dt * glm::normalize(rotate);
    }

    // limit pitch values between about +/- 85ish degrees
    gameObject.transform.rotation.x = glm::clamp(gameObject.transform.rotation.x, -1.5f, 1.5f);
    gameObject.transform.rotation.y = glm::mod(gameObject.transform.rotation.y, glm::two_pi<float>());

    float yaw = gameObject.transform.rotation.y;
    const glm::vec3 forwardDir{sin(yaw), 0.f, cos(yaw)};
    const glm::vec3 rightDir{forwardDir.z, 0.f, -forwardDir.x};
    const glm::vec3 upDir = coords::kUp;

    glm::vec3 moveDir{0.f};
    if (input.isKeyPressed(keys.moveForward)) moveDir += forwardDir;
    if (input.isKeyPressed(keys.moveBackward)) moveDir -= forwardDir;
    if (input.isKeyPressed(keys.moveRight)) moveDir += rightDir;
    if (input.isKeyPressed(keys.moveLeft)) moveDir -= rightDir;
    if (input.isKeyPressed(keys.moveUp)) moveDir += upDir;
    if (input.isKeyPressed(keys.moveDown)) moveDir -= upDir;

    if (glm::dot(moveDir, moveDir) > std::numeric_limits<float>::epsilon()) {
      gameObject.transform.translation += moveSpeed * dt * glm::normalize(moveDir);
    }
  }

  glm::vec3 CharacterMovementController::moveInPlaneXZ(backend::InputProvider &input, float dt, LveGameObject &character) {
    (void)dt;
    const glm::vec3 rightDir = coords::kRight;
    const glm::vec3 upDir = coords::kUp;

    glm::vec3 moveDir{0.f};
    if (input.isKeyPressed(keys.moveForward)) {
      moveDir += upDir;
      character.directions = Direction::UP;
      character.objState = ObjectState::WALKING;
    }
    if (input.isKeyPressed(keys.moveBackward)) {
      moveDir -= upDir;
      character.directions = Direction::DOWN;
      character.objState = ObjectState::WALKING;
    }
    if (input.isKeyPressed(keys.moveRight)) {
      moveDir += rightDir;
      character.directions = Direction::RIGHT;
      character.objState = ObjectState::WALKING;
    }
    if (input.isKeyPressed(keys.moveLeft)) {
      moveDir -= rightDir;
      character.directions = Direction::LEFT;
      character.objState = ObjectState::WALKING;
    }
    if (glm::length(moveDir) == 0.f) {
      character.objState = ObjectState::IDLE;
    }

    return moveDir;
  }

} // namespace lve
