#include "utils/game_object.hpp"

#include <algorithm>
#include <numeric>
#include <stdexcept>

namespace lve {

  glm::mat4 TransformComponent::mat4() const {
    const float c3 = glm::cos(rotation.z);
    const float s3 = glm::sin(rotation.z);
    const float c2 = glm::cos(rotation.x);
    const float s2 = glm::sin(rotation.x);
    const float c1 = glm::cos(rotation.y);
    const float s1 = glm::sin(rotation.y);
    return glm::mat4{
      {
        scale.x * (c1 * c3 + s1 * s2 * s3),
        scale.x * (c2 * s3),
        scale.x * (c1 * s2 * s3 - c3 * s1),
        0.0f,
      },
      {
        scale.y * (c3 * s1 * s2 - c1 * s3),
        scale.y * (c2 * c3),
        scale.y * (c1 * c3 * s2 + s1 * s3),
        0.0f,
      },
      {
        scale.z * (c2 * s1),
        scale.z * (-s2),
        scale.z * (c1 * c2),
        0.0f,
      },
      {translation.x, translation.y, translation.z, 1.0f}};
  }

  glm::mat3 TransformComponent::normalMatrix() const {
    const float c3 = glm::cos(rotation.z);
    const float s3 = glm::sin(rotation.z);
    const float c2 = glm::cos(rotation.x);
    const float s2 = glm::sin(rotation.x);
    const float c1 = glm::cos(rotation.y);
    const float s1 = glm::sin(rotation.y);
    const glm::vec3 invScale = 1.0f / scale;
    return glm::mat3{
      {
        invScale.x * (c1 * c3 + s1 * s2 * s3),
        invScale.x * (c2 * s3),
        invScale.x * (c1 * s2 * s3 - c3 * s1),
      },
      {
        invScale.y * (c3 * s1 * s2 - c1 * s3),
        invScale.y * (c2 * c3),
        invScale.y * (c1 * c3 * s2 + s1 * s3),
      },
      {
        invScale.z * (c2 * s1),
        invScale.z * (-s2),
        invScale.z * (c1 * c2),
      }
    };
  }

  LveGameObject &LveGameObjectManager::makePointLight(float intensity, float radius, glm::vec3 color) {
    auto &gameObj = createGameObject();
    gameObj.color = color;
    gameObj.transform.scale.x = radius;
    gameObj.pointLight = std::make_unique<PointLightComponent>();
    gameObj.pointLight->lightIntensity = intensity;
    return gameObj;
  }

  LveGameObject &LveGameObjectManager::createGameObjectWithId(LveGameObject::id_t id) {
    if (gameObjects.count(id)) {
      return gameObjects.at(id);
    }
    if (id >= MAX_GAME_OBJECTS) {
      throw std::runtime_error("GameObject id exceeds MAX_GAME_OBJECTS");
    }
    auto freeIt = std::find(freeIds.begin(), freeIds.end(), id);
    if (freeIt != freeIds.end()) {
      freeIds.erase(freeIt);
    }
    auto gameObject = LveGameObject{id, *this};
    gameObject.diffuseMap = textureDefault;
    gameObjects.emplace(id, std::move(gameObject));
    if (id >= currentId) {
      for (LveGameObject::id_t nextId = currentId; nextId < id; ++nextId) {
        freeIds.push_back(nextId);
      }
      currentId = id + 1;
    }
    return gameObjects.at(id);
  }

  LveGameObject &LveGameObjectManager::makePointLightWithId(
    LveGameObject::id_t id,
    float intensity,
    float radius,
    glm::vec3 color) {
    auto &gameObj = createGameObjectWithId(id);
    gameObj.color = color;
    gameObj.transform.scale.x = radius;
    gameObj.pointLight = std::make_unique<PointLightComponent>();
    gameObj.pointLight->lightIntensity = intensity;
    return gameObj;
  }

  LveGameObjectManager::LveGameObjectManager(LveDevice &device) {/* : physicsEngine{std::make_unique<PhysicsEngine>()} */ // physicsEngine initialization temporarily disabled {
    // including nonCoherentAtomSize allows us to flush a specific index at once
    int alignment = std::lcm(
      device.properties.limits.nonCoherentAtomSize,
      device.properties.limits.minUniformBufferOffsetAlignment);
    for (int i = 0; i < uboBuffers.size(); i++) {
      uboBuffers[i] = std::make_unique<LveBuffer>(
      device,
      sizeof(GameObjectBufferData),
      LveGameObjectManager::MAX_GAME_OBJECTS,
      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
      alignment);
      uboBuffers[i]->map();
    }

    textureDefault = LveTexture::createTextureFromFile(device, "Assets/textures/missing.png");
  }

  bool LveGameObjectManager::destroyGameObject(LveGameObject::id_t id) {
    auto it = gameObjects.find(id);
    if (it == gameObjects.end()) {
      return false;
    }
    gameObjects.erase(it);
    freeIds.push_back(id);
    return true;
  }

  void LveGameObjectManager::clearAll() {
    gameObjects.clear();
    currentId = 0;
    freeIds.clear();
  }

  void LveGameObjectManager::clearAllExcept(std::optional<LveGameObject::id_t> protectedId) {
    if (!protectedId.has_value()) {
      clearAll();
      return;
    }
    LveGameObject::id_t keepId = *protectedId;
    for (auto it = gameObjects.begin(); it != gameObjects.end();) {
      if (it->first == keepId) {
        ++it;
      } else {
        it = gameObjects.erase(it);
      }
    }
    // reset currentId to next available id
    LveGameObject::id_t maxId = 0;
    for (auto &kv : gameObjects) {
      if (kv.first > maxId) maxId = kv.first;
    }
    currentId = gameObjects.empty() ? 0 : (maxId + 1);
    freeIds.clear();
    if (currentId > 0) {
      std::vector<bool> used(currentId, false);
      for (const auto &kv : gameObjects) {
        if (kv.first < currentId) {
          used[kv.first] = true;
        }
      }
      for (LveGameObject::id_t id = 0; id < currentId; ++id) {
        if (!used[id]) {
          freeIds.push_back(id);
        }
      }
    }
  }

  void LveGameObjectManager::updateBuffer(int frameIndex) {
    // copy model matrix and normal matrix for each gameObj into
    // buffer for this frame
    bool anyDirty = false;
    for (auto &kv : gameObjects) {
      auto &obj = kv.second;
      if (!obj.transformDirty) {
        continue;
      }
      GameObjectBufferData data{};
      data.modelMatrix = obj.transform.mat4();
      data.normalMatrix = obj.transform.normalMatrix();
      for (auto &buffer : uboBuffers) {
        buffer->writeToIndex(&data, kv.first);
      }
      obj.transformDirty = false;
      anyDirty = true;
    }
    if (anyDirty) {
      for (auto &buffer : uboBuffers) {
        buffer->flush();
      }
    }
  }

  void LveGameObjectManager::resetDescriptorCaches() {
    for (auto &kv : gameObjects) {
      auto &obj = kv.second;
      obj.descriptorSets.fill(VK_NULL_HANDLE);
      obj.descriptorTextures.fill(nullptr);
      for (auto &cache : obj.subMeshDescriptors) {
        cache.sets.fill(VK_NULL_HANDLE);
        cache.textures.fill(nullptr);
      }
    }
  }

  VkDescriptorBufferInfo LveGameObject::getBufferInfo(int frameIndex) {
    return gameObjectManager.getBufferInfoForGameObject(frameIndex, id);
  }

  LveGameObject::LveGameObject(id_t objId, const LveGameObjectManager &manager)
    : id{objId}, gameObjectManager{manager} {}

  // 오브젝트 프레임 업데이트
  void LveGameObjectManager::updateFrame(LveGameObject &character, int /*maxFrames*/, float frameTime, float /*animationSpeed*/) {
    // prefer per-object sprite state metadata
    const int stateKey = static_cast<int>(character.objState);
    auto it = character.spriteStates.find(stateKey);
    if (it == character.spriteStates.end()) {
      return;
    }

    const SpriteStateInfo &stateInfo = it->second;
    if (stateInfo.frameCount <= 0) {
      return;
    }

    character.animationTimeAccumulator += frameTime;
    if (character.animationTimeAccumulator >= stateInfo.frameDuration) {
      character.currentFrame = (character.currentFrame + 1) % stateInfo.frameCount;
      character.animationTimeAccumulator = 0.0f;
    }
  }

} // namespace lve
