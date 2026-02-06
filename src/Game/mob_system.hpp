#pragma once

#include "Engine/scene_system.hpp"
#include "Game/Tilemap/tilemap_system.hpp"
#include "utils/sprite_metadata.hpp"

// std
#include <vector>

namespace lve::game {

  enum class MobDamageResult {
    NotFound,
    Damaged,
    Killed
  };

  struct MobTuning {
    float moveSpeed{3.5f};
    float gravity{18.0f};
    float terminalVelocity{20.0f};
    float jumpSpeed{10.0f};
    float jumpCooldown{0.4f};
    float jumpProbeDistance{1.0f};
    float waterTouchDamage{1.0f};
    float waterDamageCooldown{0.4f};
    float stopDistance{0.1f};
    bool gravityEnabled{true};
    bool snapEnabled{true};
  };

  class MobSystem {
  public:
    explicit MobSystem(std::string metaPath = "Assets/textures/characters/mob.json");

    void init(SceneSystem &sceneSystem, std::vector<glm::vec3> spawnPoints);
    void update(
      float dt,
      const glm::vec3 &targetPosition,
      SceneSystem &sceneSystem,
      tilemap::TilemapSystem *tilemapSystem);

    bool isInitialized() const { return initialized; }
    MobTuning &getTuning() { return tuning; }
    const MobTuning &getTuning() const { return tuning; }
    std::vector<LveGameObject*> getMobs(SceneSystem &sceneSystem);
    MobDamageResult applyDamage(SceneSystem &sceneSystem, LveGameObject::id_t mobId, float damage);
    void reset(SceneSystem &sceneSystem);

  private:
    bool loadMetadata(SceneSystem &sceneSystem);
    bool applyState(SceneSystem &sceneSystem, LveGameObject &mob, const std::string &stateName);
    void spawnMob(SceneSystem &sceneSystem, const glm::vec3 &spawnPosition);

    std::string metaPath;
    SpriteMetadata metadata{};
    bool metadataLoaded{false};

    MobTuning tuning{};
    bool initialized{false};
    std::vector<glm::vec3> spawnPoints;
    float spawnTimer{0.f};
    std::size_t spawnIndex{0};

    struct MobInstance {
      LveGameObject::id_t id{0};
      glm::vec3 position{0.f};
      glm::vec3 velocity{0.f};
      bool positionInitialized{false};
      float jumpTimer{0.f};
      float waterDamageTimer{0.f};
      float hp{1.f};
      float maxHp{1.f};
    };
    std::vector<MobInstance> mobs;
  };

} // namespace lve::game
