#include "Game/mob_system.hpp"

// std
#include <cmath>
#include <cstddef>
#include <iostream>
#include <utility>

namespace lve::game {
  namespace {
    std::string resolveStateName(const SpriteMetadata &metadata, const std::string &stateName) {
      if (!stateName.empty() && metadata.states.find(stateName) != metadata.states.end()) {
        return stateName;
      }
      auto idleIt = metadata.states.find("idle");
      if (idleIt != metadata.states.end()) {
        return idleIt->first;
      }
      if (!metadata.states.empty()) {
        return metadata.states.begin()->first;
      }
      return {};
    }
  } // namespace

  MobSystem::MobSystem(std::string metaPath)
    : metaPath(std::move(metaPath)) {}

  bool MobSystem::loadMetadata(SceneSystem &sceneSystem) {
    if (metadataLoaded) {
      return true;
    }

    const std::string resolvedPath = sceneSystem.getAssetDatabase().resolveAssetPath(metaPath);
    if (!loadSpriteMetadata(resolvedPath, metadata)) {
      std::cerr << "Failed to load mob sprite metadata: " << metaPath << "\n";
      return false;
    }

    metadataLoaded = true;
    return true;
  }

  bool MobSystem::applyState(SceneSystem &sceneSystem, LveGameObject &mob, const std::string &stateName) {
    if (!metadataLoaded) {
      return false;
    }

    const std::string resolvedName = resolveStateName(metadata, stateName);
    if (resolvedName.empty()) {
      return false;
    }

    auto it = metadata.states.find(resolvedName);
    if (it == metadata.states.end()) {
      return false;
    }

    const SpriteStateInfo &stateInfo = it->second;
    std::string texturePath = stateInfo.texturePath;
    if (texturePath.empty()) {
      texturePath = metadata.texturePath;
    }
    if (texturePath.empty()) {
      std::cerr << "Mob sprite state missing texture path: " << resolvedName << "\n";
      return false;
    }

    auto texture = sceneSystem.loadTextureCached(texturePath);
    if (!texture) {
      std::cerr << "Failed to load mob sprite texture: " << texturePath << "\n";
      return false;
    }

    const bool stateChanged = (resolvedName != mob.spriteStateName);

    mob.diffuseMap = texture;
    mob.atlasColumns = (stateInfo.atlasCols > 0) ? stateInfo.atlasCols : metadata.atlasCols;
    mob.atlasRows = (stateInfo.atlasRows > 0) ? stateInfo.atlasRows : metadata.atlasRows;

    mob.spriteState = stateInfo;
    if (mob.atlasRows > 0) {
      mob.spriteState.row = mob.atlasRows - 1 - mob.spriteState.row;
    }
    mob.hasSpriteState = true;
    mob.spriteStateName = resolvedName;

    float ppu = metadata.pixelsPerUnit;
    if (ppu <= 0.f) {
      ppu = (metadata.size.y != 0.f) ? metadata.size.y : 1.f;
    }
    const float width = metadata.size.x / ppu;
    const float height = metadata.size.y / ppu;
    mob.transform.scale = glm::vec3(width, height, 1.f);
    mob.transformDirty = true;
    if (stateChanged) {
      mob.currentFrame = 0;
      mob.animationTimeAccumulator = 0.0f;
    }
    return true;
  }

  void MobSystem::spawnMob(SceneSystem &sceneSystem, const glm::vec3 &spawnPosition) {
    auto &mob = sceneSystem.createSpriteObject(spawnPosition, ObjectState::IDLE, metaPath);
    mob.name = "Mob";
    mob.spriteMetaPath = metaPath;
    mob.objState = ObjectState::IDLE;
    mob.directions = Direction::RIGHT;

    if (!applyState(sceneSystem, mob, "idle")) {
      std::cerr << "Failed to apply mob sprite state; mob will use defaults\n";
    }

    MobInstance inst{};
    inst.id = mob.getId();
    inst.position = spawnPosition;
    inst.velocity = {0.f, 0.f, 0.f};
    inst.positionInitialized = true;
    inst.jumpTimer = 0.f;
    inst.maxHp = (metadata.hp > 0.f) ? metadata.hp : 1.f;
    inst.hp = inst.maxHp;
    mobs.push_back(inst);
  }

  void MobSystem::init(SceneSystem &sceneSystem, std::vector<glm::vec3> spawnPoints) {
    if (initialized) {
      return;
    }
    if (!loadMetadata(sceneSystem)) {
      return;
    }

    this->spawnPoints = std::move(spawnPoints);
    mobs.clear();
    spawnTimer = 0.f;
    spawnIndex = 0;
    initialized = true;

    if (!this->spawnPoints.empty()) {
      spawnMob(sceneSystem, this->spawnPoints.front());
      if (this->spawnPoints.size() > 1) {
        spawnIndex = 1;
      }
    }
  }

  void MobSystem::update(
    float dt,
    const glm::vec3 &targetPosition,
    SceneSystem &sceneSystem,
    tilemap::TilemapSystem *tilemapSystem) {
    if (!initialized) {
      return;
    }

    if (metadata.spawnInterval > 0.f && !spawnPoints.empty()) {
      spawnTimer += dt;
      while (spawnTimer >= metadata.spawnInterval) {
        spawnMob(sceneSystem, spawnPoints[spawnIndex]);
        spawnIndex = (spawnIndex + 1) % spawnPoints.size();
        spawnTimer -= metadata.spawnInterval;
      }
    }

    for (std::size_t i = 0; i < mobs.size();) {
      auto &mobInst = mobs[i];
      auto *mobPtr = sceneSystem.findObject(mobInst.id);
      if (!mobPtr) {
        mobs.erase(mobs.begin() + static_cast<std::ptrdiff_t>(i));
        continue;
      }
      auto &mob = *mobPtr;

      if (!mobInst.positionInitialized) {
        mobInst.position = mob.transform.translation;
        mobInst.positionInitialized = true;
      }
      if (mobInst.jumpTimer > 0.f) {
        mobInst.jumpTimer -= dt;
        if (mobInst.jumpTimer < 0.f) {
          mobInst.jumpTimer = 0.f;
        }
      }
      if (mobInst.waterDamageTimer > 0.f) {
        mobInst.waterDamageTimer -= dt;
        if (mobInst.waterDamageTimer < 0.f) {
          mobInst.waterDamageTimer = 0.f;
        }
      }

      const glm::vec3 prevPos = mobInst.position;
      const bool onLadder = tilemapSystem ? tilemapSystem->isLadderAtWorld(mobInst.position) : false;
      const glm::vec2 delta{
        targetPosition.x - mobInst.position.x,
        targetPosition.y - mobInst.position.y
      };
      const float distance = glm::length(delta);

      glm::vec2 moveDir{0.f, 0.f};
      if (distance > tuning.stopDistance) {
        moveDir = delta / distance;
        mob.objState = ObjectState::WALKING;
        if (std::abs(moveDir.x) > 0.001f) {
          mob.directions = (moveDir.x < 0.f) ? Direction::LEFT : Direction::RIGHT;
        }
      } else {
        mob.objState = ObjectState::IDLE;
      }

      const bool wantsWalking = mob.objState == ObjectState::WALKING;
      if (metadataLoaded) {
        const bool hasWalk = metadata.states.find("walking") != metadata.states.end();
        const char *desiredState = (wantsWalking && hasWalk) ? "walking" : "idle";
        if (mob.spriteStateName != desiredState) {
          applyState(sceneSystem, mob, desiredState);
        }
      }

      if (!onLadder && tilemapSystem && mobInst.jumpTimer <= 0.f && std::abs(moveDir.x) > 0.001f) {
        const float halfWidth = mob.transform.scale.x * 0.5f;
        const float halfHeight = mob.transform.scale.y * 0.5f;
        const float edgeEps = 0.01f;
        const float footY = mobInst.position.y + halfHeight + 0.02f;
        const glm::vec3 footLeft{mobInst.position.x - halfWidth + edgeEps, footY, 0.f};
        const glm::vec3 footRight{mobInst.position.x + halfWidth - edgeEps, footY, 0.f};
        const bool onGround = tilemapSystem->isGroundAtWorld(footLeft) ||
          tilemapSystem->isGroundAtWorld(footRight);
        if (onGround) {
          const float sign = (moveDir.x < 0.f) ? -1.f : 1.f;
          const float probeX = mobInst.position.x + sign * (halfWidth + tuning.jumpProbeDistance);
          const glm::vec3 probeFoot{probeX, footY, 0.f};
          const glm::vec3 probeTop{probeX, mobInst.position.y - halfHeight + edgeEps, 0.f};
          const glm::vec3 probeMid{probeX, mobInst.position.y - (halfHeight * 0.5f), 0.f};
          const bool waterAhead = tilemapSystem->isWaterAtWorld(probeFoot);
          const bool wallAhead =
            tilemapSystem->isGroundAtWorld(probeTop) || tilemapSystem->isGroundAtWorld(probeMid);
          if (waterAhead || wallAhead) {
            mobInst.velocity.y = -tuning.jumpSpeed;
            mobInst.jumpTimer = tuning.jumpCooldown;
          }
        }
      }

      bool ladderNearby = onLadder;
      if (tilemapSystem) {
        const float halfHeight = mob.transform.scale.y * 0.5f;
        const glm::vec3 abovePos = mobInst.position + glm::vec3(0.f, -halfHeight - 0.02f, 0.f);
        const glm::vec3 belowPos = mobInst.position + glm::vec3(0.f, halfHeight + 0.02f, 0.f);
        ladderNearby =
          ladderNearby ||
          tilemapSystem->isLadderAtWorld(mobInst.position) ||
          tilemapSystem->isLadderAtWorld(abovePos) ||
          tilemapSystem->isLadderAtWorld(belowPos);
      }
      if (!onLadder) {
        if (!ladderNearby || std::abs(moveDir.y) < 0.001f) {
          moveDir.y = 0.f;
        }
      }

      mobInst.position.x += tuning.moveSpeed * dt * moveDir.x;
      mobInst.position.y += tuning.moveSpeed * dt * moveDir.y;

      if (tuning.gravityEnabled && !onLadder) {
        mobInst.velocity.y += tuning.gravity * dt;
        if (mobInst.velocity.y > tuning.terminalVelocity) {
          mobInst.velocity.y = tuning.terminalVelocity;
        }
        mobInst.position.y += mobInst.velocity.y * dt;
      } else {
        mobInst.velocity.y = 0.0f;
      }

      mob.transform.translation = mobInst.position;
      mob.transformDirty = true;

      if (tilemapSystem) {
        const bool touchedWater = tilemapSystem->isWaterAtWorld(mobInst.position);
        if (touchedWater && mobInst.waterDamageTimer <= 0.f) {
          mobInst.hp -= tuning.waterTouchDamage;
          mobInst.waterDamageTimer = tuning.waterDamageCooldown;
          if (mobInst.hp <= 0.f) {
            sceneSystem.destroyObject(mobInst.id);
            mobs.erase(mobs.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
          }
        }

        const bool wantsDownOnLadder = ladderNearby && (moveDir.y > 0.001f);
        const bool collided = tilemapSystem->resolveCollisions(prevPos, mob, wantsDownOnLadder);
        mobInst.position = mob.transform.translation;
        if (collided) {
          mobInst.velocity.y = 0.0f;
        }
      }

      if (tuning.snapEnabled && metadataLoaded) {
        const float ppu = metadata.pixelsPerUnit;
        if (ppu > 0.f) {
          const float snap = 1.0f / ppu;
          mob.transform.translation.x = std::round(mobInst.position.x / snap) * snap;
          mob.transform.translation.y = std::round(mobInst.position.y / snap) * snap;
          mob.transformDirty = true;
        }
      }

      sceneSystem.updateAnimationFrame(mob, 1, dt, 0.15f);
      ++i;
    }
  }

  std::vector<LveGameObject*> MobSystem::getMobs(SceneSystem &sceneSystem) {
    std::vector<LveGameObject*> result;
    result.reserve(mobs.size());
    for (const auto &mobInst : mobs) {
      if (auto *mob = sceneSystem.findObject(mobInst.id)) {
        result.push_back(mob);
      }
    }
    return result;
  }

  MobDamageResult MobSystem::applyDamage(SceneSystem &sceneSystem, LveGameObject::id_t mobId, float damage) {
    if (damage <= 0.f) {
      return MobDamageResult::NotFound;
    }

    for (auto it = mobs.begin(); it != mobs.end(); ++it) {
      if (it->id != mobId) {
        continue;
      }

      it->hp -= damage;
      if (it->hp > 0.f) {
        return MobDamageResult::Damaged;
      }

      sceneSystem.destroyObject(it->id);
      mobs.erase(it);
      return MobDamageResult::Killed;
    }

    return MobDamageResult::NotFound;
  }

  void MobSystem::reset(SceneSystem &sceneSystem) {
    for (const auto &mobInst : mobs) {
      sceneSystem.destroyObject(mobInst.id);
    }
    mobs.clear();
    spawnTimer = 0.f;
    spawnIndex = 0;

    if (!initialized || spawnPoints.empty()) {
      return;
    }

    spawnMob(sceneSystem, spawnPoints.front());
    if (spawnPoints.size() > 1) {
      spawnIndex = 1;
    }
  }

} // namespace lve::game
