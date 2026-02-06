#include "Game/bullet_system.hpp"

// std
#include <cstddef>
#include <cmath>
#include <iostream>
#include <utility>

namespace lve::game {
  namespace {
    bool overlapsAabb(const LveGameObject &a, const LveGameObject &b) {
      const float ax = a.transform.scale.x * 0.5f;
      const float ay = a.transform.scale.y * 0.5f;
      const float bx = b.transform.scale.x * 0.5f;
      const float by = b.transform.scale.y * 0.5f;
      const float dx = std::abs(a.transform.translation.x - b.transform.translation.x);
      const float dy = std::abs(a.transform.translation.y - b.transform.translation.y);
      return dx <= (ax + bx) && dy <= (ay + by);
    }
  } // namespace

  BulletSystem::BulletSystem(std::string texturePath)
    : texturePath(std::move(texturePath)) {}

  void BulletSystem::update(
    backend::InputProvider &input,
    float dt,
    SceneSystem &sceneSystem,
    const LveGameObject &player) {
    if (cooldownTimer > 0.f) {
      cooldownTimer -= dt;
      if (cooldownTimer < 0.f) {
        cooldownTimer = 0.f;
      }
    }

    if (input.isMouseButtonPressed(backend::MouseButton::Left) && cooldownTimer <= 0.f) {
      if (spawnBullet(sceneSystem, player)) {
        cooldownTimer = tuning.cooldown;
      }
    }

    for (std::size_t i = 0; i < bullets.size();) {
      auto &bullet = bullets[i];
      auto *obj = sceneSystem.findObject(bullet.id);
      if (!obj) {
        bullets.erase(bullets.begin() + static_cast<std::ptrdiff_t>(i));
        continue;
      }

      bullet.life += dt;
      if (bullet.life >= tuning.lifetime) {
        sceneSystem.destroyObject(bullet.id);
        bullets.erase(bullets.begin() + static_cast<std::ptrdiff_t>(i));
        continue;
      }

      bullet.position += bullet.velocity * dt;
      obj->transform.translation = bullet.position;
      obj->transformDirty = true;
      ++i;
    }
  }

  std::vector<LveGameObject::id_t> BulletSystem::collectMobHits(
    SceneSystem &sceneSystem,
    const std::vector<LveGameObject*> &mobs) {
    std::vector<LveGameObject::id_t> hitMobIds{};

    for (std::size_t i = 0; i < bullets.size();) {
      auto &bullet = bullets[i];
      auto *bulletObj = sceneSystem.findObject(bullet.id);
      if (!bulletObj) {
        bullets.erase(bullets.begin() + static_cast<std::ptrdiff_t>(i));
        continue;
      }

      LveGameObject::id_t hitMobId = 0;
      bool hit = false;
      for (auto *mob : mobs) {
        if (!mob) continue;
        if (overlapsAabb(*bulletObj, *mob)) {
          hit = true;
          hitMobId = mob->getId();
          break;
        }
      }

      if (hit) {
        sceneSystem.destroyObject(bullet.id);
        bullets.erase(bullets.begin() + static_cast<std::ptrdiff_t>(i));
        hitMobIds.push_back(hitMobId);
        continue;
      }

      ++i;
    }

    return hitMobIds;
  }

  void BulletSystem::reset(SceneSystem &sceneSystem) {
    for (const auto &bullet : bullets) {
      sceneSystem.destroyObject(bullet.id);
    }
    bullets.clear();
    cooldownTimer = 0.f;
    shotTriggered = false;
  }

  bool BulletSystem::consumeShotEvent() {
    const bool fired = shotTriggered;
    shotTriggered = false;
    return fired;
  }

  bool BulletSystem::spawnBullet(SceneSystem &sceneSystem, const LveGameObject &player) {
    auto texture = sceneSystem.loadTextureCached(texturePath);
    if (!texture) {
      std::cerr << "Failed to load bullet texture: " << texturePath << "\n";
      return false;
    }

    const float dir = (player.directions == Direction::LEFT) ? -1.f : 1.f;
    const float offsetX = (player.transform.scale.x * 0.6f) * dir;
    const glm::vec3 spawnPos = player.transform.translation + glm::vec3(offsetX, 0.f, 0.f);

    const float size = (tuning.pixelsPerUnit > 0.f)
      ? (tuning.sizePixels / tuning.pixelsPerUnit)
      : tuning.sizePixels;

    auto &obj = sceneSystem.createTileSpriteObject(
      spawnPos,
      texture,
      1,
      1,
      0,
      0,
      {size, size, 1.f},
      tuning.renderOrder);
    obj.name = "Bullet";
    obj.objState = ObjectState::IDLE;
    obj.directions = player.directions;
    obj.transformDirty = true;

    Bullet bullet{};
    bullet.id = obj.getId();
    bullet.position = spawnPos;
    bullet.velocity = glm::vec3(tuning.speed * dir, 0.f, 0.f);
    bullet.life = 0.f;
    bullets.push_back(bullet);
    shotTriggered = true;
    return true;
  }

} // namespace lve::game
