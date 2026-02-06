#pragma once

#include "Engine/Backend/input.hpp"
#include "Engine/scene_system.hpp"

#include <string>
#include <vector>

namespace lve::game {

  struct BulletTuning {
    float speed{8.0f};
    float lifetime{1.5f};
    float cooldown{0.15f};
    float sizePixels{1.0f};
    float pixelsPerUnit{8.0f};
    int renderOrder{1100};
  };

  class BulletSystem {
  public:
    explicit BulletSystem(std::string texturePath = "Assets/textures/characters/bullet.png");

    void update(
      backend::InputProvider &input,
      float dt,
      SceneSystem &sceneSystem,
      const LveGameObject &player);
    void reset(SceneSystem &sceneSystem);
    bool consumeShotEvent();
    std::vector<LveGameObject::id_t> collectMobHits(
      SceneSystem &sceneSystem,
      const std::vector<LveGameObject*> &mobs);

    BulletTuning &getTuning() { return tuning; }
    const BulletTuning &getTuning() const { return tuning; }

  private:
    struct Bullet {
      LveGameObject::id_t id{0};
      glm::vec3 position{0.f};
      glm::vec3 velocity{0.f};
      float life{0.f};
    };

    bool spawnBullet(SceneSystem &sceneSystem, const LveGameObject &player);

    std::string texturePath;
    BulletTuning tuning{};
    float cooldownTimer{0.f};
    bool shotTriggered{false};
    std::vector<Bullet> bullets;
  };

} // namespace lve::game
