#pragma once

#include "Engine/Backend/object_buffer.hpp"
#include "Engine/Backend/render_assets.hpp"
#include "Engine/asset_database.hpp"
#include "Engine/asset_defaults.hpp"
#include "Engine/material_data.hpp"
#include "Engine/scene.hpp"
#include "utils/game_object.hpp"
#include "utils/sprite_animator.hpp"
#include "utils/sprite_metadata.hpp"

// std
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace lve {
  class SceneSystem {
  public:
    SceneSystem(
      backend::RenderAssetFactory &assets,
      backend::ObjectBufferPoolPtr objectBuffers);

    LveGameObject &createEmptyObject();
    LveGameObject *findObject(LveGameObject::id_t id);
    const LveGameObject *findObject(LveGameObject::id_t id) const;
    bool destroyObject(LveGameObject::id_t id);
    void collectObjects(std::vector<LveGameObject*> &out);
    void collectObjects(std::vector<const LveGameObject*> &out) const;
    void updateBuffers(int frameIndex);
    void resetDescriptorCaches();
    void updateAnimationFrame(LveGameObject &obj, int maxFrames, float frameTime, float animationSpeed);

    const AssetDefaults &getAssetDefaults() const { return assetDefaults; }
    void setAssetDefaults(const AssetDefaults &defaults);
    void setAssetRootPath(const std::string &rootPath);
    void setActiveMeshPath(const std::string &path);
    void setActiveMaterialPath(const std::string &path);

    AssetDatabase &getAssetDatabase() { return assetDatabase; }
    const AssetDatabase &getAssetDatabase() const { return assetDatabase; }

    SpriteAnimator *getSpriteAnimator() { return spriteAnimator.get(); }
    const SpriteMetadata &getSpriteMetadata() const { return playerMeta; }

    LveGameObject::id_t getCharacterId() const { return characterId; }
    void setCharacterId(LveGameObject::id_t id) { characterId = id; }

    void loadGameObjects();
    Scene exportSceneSnapshot();
    void importSceneSnapshot(const Scene &scene, std::optional<LveGameObject::id_t> protectedId);
    void saveSceneToFile(const std::string &path);
    void loadSceneFromFile(const std::string &path, std::optional<LveGameObject::id_t> protectedId);

    std::shared_ptr<backend::RenderModel> loadModelCached(const std::string &path);
    std::shared_ptr<backend::RenderMaterial> loadMaterialCached(const std::string &path);
    bool updateMaterialFromData(const std::string &path, const MaterialData &data);
    bool applyMaterialToObject(LveGameObject &obj, const std::string &path);
    bool setActiveSpriteMetadata(const std::string &path);
    void ensureNodeOverrides(LveGameObject &obj);
    void applyNodeOverrides(LveGameObject &obj, const MeshComponent &mesh);

    LveGameObject &createSpriteObject(
      const glm::vec3 &position,
      ObjectState state = ObjectState::IDLE,
      const std::string &metaPath = "Assets/textures/characters/player.json");
    LveGameObject &createMeshObject(
      const glm::vec3 &position,
      const std::string &modelPath = "Assets/models/colored_cube.obj");
    LveGameObject &createPointLightObject(const glm::vec3 &position);
    LveGameObject &createSpriteObjectWithId(
      LveGameObject::id_t id,
      const glm::vec3 &position,
      ObjectState state,
      const std::string &metaPath);
    LveGameObject &createMeshObjectWithId(
      LveGameObject::id_t id,
      const glm::vec3 &position,
      const std::string &modelPath);
    LveGameObject &createPointLightObjectWithId(
      LveGameObject::id_t id,
      const glm::vec3 &position,
      float intensity,
      float radius,
      const glm::vec3 &color);

  private:
    static ObjectState objectStateFromString(const std::string &name);
    static std::string objectStateToString(ObjectState state);

    backend::RenderAssetFactory &assetFactory;
    LveGameObjectManager gameObjectManager;
    AssetDefaults assetDefaults;
    AssetDatabase assetDatabase;
    SpriteMetadata playerMeta;
    std::unique_ptr<SpriteAnimator> spriteAnimator;
    std::shared_ptr<backend::RenderModel> cubeModel;
    std::shared_ptr<backend::RenderModel> spriteModel;
    std::unordered_map<std::string, std::shared_ptr<backend::RenderModel>> modelCache;
    std::unordered_map<std::string, std::shared_ptr<backend::RenderMaterial>> materialCache;
    LveGameObject::id_t characterId{0};
  };
} // namespace lve


