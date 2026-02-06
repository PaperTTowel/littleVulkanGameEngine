#pragma once

#include "Game/Tilemap/tilemap_system.hpp"
#include "utils/keyboard_movement_controller.hpp"
#include "utils/sprite_animator.hpp"

// std
#include <memory>

namespace lve::game {

  struct PlayerTuning {
    float moveSpeed{5.0f};
    float gravity{18.0f};
    float terminalVelocity{20.0f};
    float jumpSpeed{12.0f};
    float jumpProbe{0.1f};
    float waterTouchDamage{1.0f};
    bool gravityEnabled{true};
    bool snapEnabled{true};
  };

  struct PlayerStats {
    float hp{100.f};
    float maxHp{100.f};
    float damageCooldown{0.4f};
    float damageTimer{0.f};
  };

  class PlayerController {
  public:
    PlayerTuning &getTuning() { return tuning; }
    const PlayerTuning &getTuning() const { return tuning; }
    PlayerStats &getStats() { return stats; }
    const PlayerStats &getStats() const { return stats; }
    bool applyDamage(float amount);
    void resetForNewRun(const glm::vec3 &spawnPos);

    void update(
      backend::InputProvider &input,
      float dt,
      LveGameObject &character,
      tilemap::TilemapSystem *tilemapSystem,
      SpriteAnimator *spriteAnimator);

  private:
    PlayerTuning tuning{};
    PlayerStats stats{};
    glm::vec3 velocity{0.f};
    glm::vec3 position{0.f};
    glm::vec3 spawnPosition{0.f};
    bool positionInitialized{false};
    bool spawnInitialized{false};
    CharacterMovementController movementController{};
  };

} // namespace lve::game
