#pragma once

#include "Backend/device.hpp"
#include "Engine/scene.hpp"
#include "Editor/resource_browser_panel.hpp"
#include "utils/game_object.hpp"
#include "utils/sprite_animator.hpp"
#include "utils/sprite_metadata.hpp"

// std
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace lve {
  class SceneSystem {
  public:
    explicit SceneSystem(LveDevice &device);

    LveGameObjectManager &getGameObjectManager() { return gameObjectManager; }
    const LveGameObjectManager &getGameObjectManager() const { return gameObjectManager; }

    editor::ResourceBrowserState &getResourceBrowserState() { return resourceBrowserState; }
    const editor::ResourceBrowserState &getResourceBrowserState() const { return resourceBrowserState; }

    SpriteAnimator *getSpriteAnimator() { return spriteAnimator.get(); }
    const SpriteMetadata &getSpriteMetadata() const { return playerMeta; }

    LveGameObject::id_t getCharacterId() const { return characterId; }
    void setCharacterId(LveGameObject::id_t id) { characterId = id; }

    void loadGameObjects();
    Scene exportSceneSnapshot() const;
    void importSceneSnapshot(const Scene &scene, std::optional<LveGameObject::id_t> protectedId);
    void saveSceneToFile(const std::string &path);
    void loadSceneFromFile(const std::string &path, std::optional<LveGameObject::id_t> protectedId);

    std::shared_ptr<LveModel> loadModelCached(const std::string &path);
    bool setActiveSpriteMetadata(const std::string &path);

    LveGameObject &createSpriteObject(
      const glm::vec3 &position,
      ObjectState state = ObjectState::IDLE,
      const std::string &metaPath = "Assets/textures/characters/player.json");
    LveGameObject &createMeshObject(
      const glm::vec3 &position,
      const std::string &modelPath = "Assets/models/colored_cube.obj");
    LveGameObject &createPointLightObject(const glm::vec3 &position);

  private:
    static ObjectState objectStateFromString(const std::string &name);
    static std::string objectStateToString(ObjectState state);

    LveDevice &lveDevice;
    LveGameObjectManager gameObjectManager;
    editor::ResourceBrowserState resourceBrowserState;
    SpriteMetadata playerMeta;
    std::unique_ptr<SpriteAnimator> spriteAnimator;
    std::shared_ptr<LveModel> cubeModel;
    std::shared_ptr<LveModel> spriteModel;
    std::unordered_map<std::string, std::shared_ptr<LveModel>> modelCache;
    LveGameObject::id_t characterId{0};
  };
} // namespace lve
