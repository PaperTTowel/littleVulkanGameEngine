#include "Game/player_controller.hpp"

#include <cmath>
#include <limits>

namespace lve::game {

  void PlayerController::update(
    backend::InputProvider &input,
    float dt,
    LveGameObject &character,
    tilemap::TilemapSystem *tilemapSystem,
    SpriteAnimator *spriteAnimator) {
    if (!positionInitialized) {
      position = character.transform.translation;
      positionInitialized = true;
    }
    if (!spawnInitialized) {
      spawnPosition = character.transform.translation;
      spawnInitialized = true;
    }
    if (stats.damageTimer > 0.f) {
      stats.damageTimer -= dt;
      if (stats.damageTimer < 0.f) {
        stats.damageTimer = 0.f;
      }
    }

    const glm::vec3 prevPos = position;
    glm::vec3 inputDir = movementController.moveInPlaneXZ(input, dt, character);
    const bool wantsDown = input.isKeyPressed(movementController.keys.moveBackward);
    const bool wantsJump = input.isKeyPressed(movementController.keys.jump);
    const bool onLadder = tilemapSystem ? tilemapSystem->isLadderAtWorld(position) : false;

    if (!onLadder) {
      if (std::abs(inputDir.y) > std::numeric_limits<float>::epsilon()) {
        inputDir.y = 0.f;
        if (std::abs(inputDir.x) <= std::numeric_limits<float>::epsilon()) {
          character.objState = ObjectState::IDLE;
        }
      }
    }

    if (tuning.gravityEnabled && wantsJump && tilemapSystem && !onLadder) {
      const float footOffset = (character.transform.scale.y * 0.5f) + 0.01f + tuning.jumpProbe;
      const glm::vec3 footPos = position + glm::vec3(0.f, footOffset, 0.f);
      if (tilemapSystem->isGroundAtWorld(footPos)) {
        velocity.y = -tuning.jumpSpeed;
      }
    }

    if (glm::dot(inputDir, inputDir) > std::numeric_limits<float>::epsilon()) {
      position += tuning.moveSpeed * dt * glm::normalize(inputDir);
    }

    if (tuning.gravityEnabled) {
      if (onLadder) {
        velocity.y = 0.0f;
      } else {
        velocity.y += tuning.gravity * dt;
        if (velocity.y > tuning.terminalVelocity) {
          velocity.y = tuning.terminalVelocity;
        }
        position.y += velocity.y * dt;
      }
    } else {
      velocity.y = 0.0f;
    }

    bool teleported = false;
    if (tilemapSystem && tilemapSystem->isWaterAtWorld(position)) {
      applyDamage(tuning.waterTouchDamage);
      position = spawnPosition;
      velocity = {};
      teleported = true;
    }

    character.transform.translation = position;
    character.transformDirty = true;

    if (tilemapSystem) {
      if (!teleported) {
        const bool collided = tilemapSystem->resolveCollisions(prevPos, character, wantsDown);
        if (collided) {
          velocity.y = 0.0f;
        }
      }
      tilemapSystem->updateTriggers(position);
      position = character.transform.translation;
    }

    if (tuning.snapEnabled && spriteAnimator) {
      const float ppu = spriteAnimator->getMetadata().pixelsPerUnit;
      if (ppu > 0.f) {
        const float snap = 1.0f / ppu;
        character.transform.translation.x = std::round(position.x / snap) * snap;
        character.transform.translation.y = std::round(position.y / snap) * snap;
        character.transformDirty = true;
      }
    }
  }

  bool PlayerController::applyDamage(float amount) {
    if (amount <= 0.f) {
      return false;
    }
    if (stats.damageTimer > 0.f) {
      return false;
    }
    stats.hp -= amount;
    if (stats.hp < 0.f) {
      stats.hp = 0.f;
    }
    stats.damageTimer = stats.damageCooldown;
    return true;
  }

  void PlayerController::resetForNewRun(const glm::vec3 &spawnPos) {
    stats.hp = stats.maxHp;
    stats.damageTimer = 0.f;
    velocity = glm::vec3{0.f};
    position = spawnPos;
    spawnPosition = spawnPos;
    positionInitialized = true;
    spawnInitialized = true;
  }

} // namespace lve::game
