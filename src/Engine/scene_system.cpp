#include "scene_system.hpp"

// rendering
#include "Rendering/model.hpp"

// std
#include <iostream>
#include <vector>

// libs
#include <glm/gtc/constants.hpp>

namespace lve {

  namespace {
    bool isIdentityTransform(const TransformComponent &transform) {
      const float eps = 0.0001f;
      return glm::length(transform.translation) < eps &&
        glm::length(transform.rotation) < eps &&
        glm::length(transform.scale - glm::vec3(1.f)) < eps;
    }
  } // namespace

  SceneSystem::SceneSystem(LveDevice &device)
    : lveDevice{device},
      gameObjectManager{device} {}

  ObjectState SceneSystem::objectStateFromString(const std::string &name) {
    if (name == "walking" || name == "walk") return ObjectState::WALKING;
    return ObjectState::IDLE;
  }

  std::string SceneSystem::objectStateToString(ObjectState state) {
    switch (state) {
      case ObjectState::WALKING: return "walking";
      case ObjectState::IDLE:
      default: return "idle";
    }
  }

  std::shared_ptr<LveModel> SceneSystem::loadModelCached(const std::string &path) {
    auto it = modelCache.find(path);
    if (it != modelCache.end()) {
      return it->second;
    }
    auto uniqueModel = LveModel::createModelFromFile(lveDevice, path);
    auto sharedModel = std::shared_ptr<LveModel>(std::move(uniqueModel));
    modelCache[path] = sharedModel;
    if (path == "Assets/models/colored_cube.obj") {
      cubeModel = sharedModel;
    }
    return sharedModel;
  }

  void SceneSystem::ensureNodeOverrides(LveGameObject &obj) {
    if (!obj.model) {
      obj.nodeOverrides.clear();
      return;
    }
    const auto &nodes = obj.model->getNodes();
    if (obj.nodeOverrides.size() != nodes.size()) {
      obj.nodeOverrides.clear();
      obj.nodeOverrides.resize(nodes.size());
    }
  }

  void SceneSystem::applyNodeOverrides(LveGameObject &obj, const MeshComponent &mesh) {
    ensureNodeOverrides(obj);
    for (auto &override : obj.nodeOverrides) {
      override.enabled = false;
      override.transform.translation = {};
      override.transform.rotation = {};
      override.transform.scale = {1.f, 1.f, 1.f};
    }
    for (const auto &override : mesh.nodeOverrides) {
      if (override.node < 0 || static_cast<std::size_t>(override.node) >= obj.nodeOverrides.size()) {
        continue;
      }
      auto &target = obj.nodeOverrides[static_cast<std::size_t>(override.node)];
      target.enabled = true;
      target.transform.translation = override.transform.position;
      target.transform.rotation = override.transform.rotation;
      target.transform.scale = override.transform.scale;
    }
  }

  bool SceneSystem::setActiveSpriteMetadata(const std::string &path) {
    SpriteMetadata meta{};
    if (!loadSpriteMetadata(path, meta)) {
      std::cerr << "Failed to load sprite metadata: " << path << "\n";
      return false;
    }
    playerMeta = meta;
    resourceBrowserState.activeSpriteMetaPath = path;
    spriteAnimator = std::make_unique<SpriteAnimator>(lveDevice, playerMeta);

    for (auto &kv : gameObjectManager.gameObjects) {
      auto &obj = kv.second;
      if (!obj.isSprite) continue;
      obj.spriteMetaPath = path;
      spriteAnimator->applySpriteState(obj, obj.objState);
    }
    return true;
  }

  LveGameObject &SceneSystem::createMeshObject(const glm::vec3 &position, const std::string &modelPath) {
    const std::string pathToUse = modelPath.empty() ? "Assets/models/colored_cube.obj" : modelPath;
    auto model = loadModelCached(pathToUse);
    auto &obj = gameObjectManager.createGameObject();
    obj.model = model;
    obj.modelPath = pathToUse;
    obj.name = "Mesh " + std::to_string(obj.getId());
    obj.enableTextureType = model && model->hasAnyDiffuseTexture() ? 1 : 0;
    obj.isSprite = false;
    obj.billboardMode = BillboardMode::None;
    obj.transform.translation = position;
    obj.transform.scale = glm::vec3(1.f);
    obj.transformDirty = true;
    ensureNodeOverrides(obj);
    return obj;
  }

  LveGameObject &SceneSystem::createSpriteObject(const glm::vec3 &position, ObjectState state, const std::string &metaPath) {
    if (!spriteModel) {
      spriteModel = LveModel::createModelFromFile(lveDevice, "Assets/models/quad.obj");
    }
    auto &obj = gameObjectManager.createGameObject();
    obj.model = spriteModel;
    obj.name = "Sprite " + std::to_string(obj.getId());
    obj.enableTextureType = 1;
    obj.isSprite = true;
    obj.billboardMode = BillboardMode::Cylindrical;
    obj.spriteMetaPath = metaPath;
    obj.transform.translation = position;
    obj.transform.rotation = {0.f, 0.f, 0.f};
    obj.objState = state;
    obj.transformDirty = true;
    if (spriteAnimator) {
      spriteAnimator->applySpriteState(obj, state);
    }
    return obj;
  }

  LveGameObject &SceneSystem::createPointLightObject(const glm::vec3 &position) {
    auto &light = gameObjectManager.makePointLight(0.2f);
    light.name = "PointLight " + std::to_string(light.getId());
    light.transform.translation = position;
    light.transformDirty = true;
    return light;
  }

  LveGameObject &SceneSystem::createMeshObjectWithId(
    LveGameObject::id_t id,
    const glm::vec3 &position,
    const std::string &modelPath) {
    const std::string pathToUse = modelPath.empty() ? "Assets/models/colored_cube.obj" : modelPath;
    auto model = loadModelCached(pathToUse);
    auto &obj = gameObjectManager.createGameObjectWithId(id);
    obj.model = model;
    obj.modelPath = pathToUse;
    obj.name = "Mesh " + std::to_string(obj.getId());
    obj.enableTextureType = model && model->hasAnyDiffuseTexture() ? 1 : 0;
    obj.isSprite = false;
    obj.billboardMode = BillboardMode::None;
    obj.transform.translation = position;
    obj.transform.scale = glm::vec3(1.f);
    obj.transformDirty = true;
    ensureNodeOverrides(obj);
    return obj;
  }

  LveGameObject &SceneSystem::createSpriteObjectWithId(
    LveGameObject::id_t id,
    const glm::vec3 &position,
    ObjectState state,
    const std::string &metaPath) {
    if (!spriteModel) {
      spriteModel = LveModel::createModelFromFile(lveDevice, "Assets/models/quad.obj");
    }
    auto &obj = gameObjectManager.createGameObjectWithId(id);
    obj.model = spriteModel;
    obj.name = "Sprite " + std::to_string(obj.getId());
    obj.enableTextureType = 1;
    obj.isSprite = true;
    obj.billboardMode = BillboardMode::Cylindrical;
    obj.spriteMetaPath = metaPath;
    obj.transform.translation = position;
    obj.transform.rotation = {0.f, 0.f, 0.f};
    obj.objState = state;
    obj.transformDirty = true;
    if (spriteAnimator) {
      spriteAnimator->applySpriteState(obj, state);
    }
    return obj;
  }

  LveGameObject &SceneSystem::createPointLightObjectWithId(
    LveGameObject::id_t id,
    const glm::vec3 &position,
    float intensity,
    float radius,
    const glm::vec3 &color) {
    auto &light = gameObjectManager.makePointLightWithId(id, intensity, radius, color);
    light.name = "PointLight " + std::to_string(light.getId());
    light.transform.translation = position;
    light.transformDirty = true;
    return light;
  }

  Scene SceneSystem::exportSceneSnapshot() const {
    Scene scene{};
    scene.version = 1;
    scene.resources.basePath = "Assets/";
    scene.resources.spritePath = "Assets/textures/characters/";
    scene.resources.modelPath = "Assets/models/";
    scene.resources.materialPath = "Assets/materials/";

    for (const auto &kv : gameObjectManager.gameObjects) {
      const auto &obj = kv.second;
      if (!obj.model && !obj.pointLight && !obj.isSprite) {
        continue;
      }
      SceneEntity e{};
      e.id = "obj_" + std::to_string(obj.getId());
      e.name = obj.name.empty() ? e.id : obj.name;
      e.transform.position = obj.transform.translation;
      e.transform.rotation = obj.transform.rotation;
      e.transform.scale = obj.transform.scale;

      if (obj.pointLight) {
        e.type = EntityType::Light;
        LightComponent lc{};
        lc.kind = LightKind::Point;
        lc.color = obj.color;
        lc.intensity = obj.pointLight->lightIntensity;
        lc.range = 10.f;
        lc.angle = 45.f;
        e.light = lc;
      } else if (obj.isSprite) {
        e.type = EntityType::Sprite;
        SpriteComponent sc{};
        sc.spriteMeta = obj.spriteMetaPath.empty() ? "Assets/textures/characters/player.json" : obj.spriteMetaPath;
        sc.state = obj.spriteStateName.empty() ? objectStateToString(obj.objState) : obj.spriteStateName;
        sc.billboard = (obj.billboardMode == BillboardMode::Spherical) ? BillboardKind::Spherical
          : (obj.billboardMode == BillboardMode::Cylindrical ? BillboardKind::Cylindrical : BillboardKind::None);
        sc.layer = 0;
        e.sprite = sc;
      } else {
        e.type = EntityType::Mesh;
        MeshComponent mc{};
        mc.model = obj.modelPath.empty() ? "Assets/models/colored_cube.obj" : obj.modelPath;
        mc.material = obj.materialPath;
        if (!obj.nodeOverrides.empty()) {
          for (std::size_t i = 0; i < obj.nodeOverrides.size(); ++i) {
            const auto &override = obj.nodeOverrides[i];
            if (!override.enabled || isIdentityTransform(override.transform)) {
              continue;
            }
            MeshComponent::NodeOverride nodeOverride{};
            nodeOverride.node = static_cast<int>(i);
            nodeOverride.transform.position = override.transform.translation;
            nodeOverride.transform.rotation = override.transform.rotation;
            nodeOverride.transform.scale = override.transform.scale;
            mc.nodeOverrides.push_back(std::move(nodeOverride));
          }
        }
        e.mesh = mc;
      }

      scene.entities.push_back(std::move(e));
    }

    return scene;
  }

  void SceneSystem::importSceneSnapshot(const Scene &scene, std::optional<LveGameObject::id_t> protectedId) {
    gameObjectManager.clearAllExcept(protectedId);
    cubeModel.reset();
    spriteModel.reset();
    modelCache.clear();

    // reload sprite metadata if provided by first sprite entity
    std::string metaPath = resourceBrowserState.activeSpriteMetaPath.empty()
      ? "Assets/textures/characters/player.json"
      : resourceBrowserState.activeSpriteMetaPath;
    for (const auto &e : scene.entities) {
      if (e.sprite) {
        metaPath = e.sprite->spriteMeta.empty() ? metaPath : e.sprite->spriteMeta;
        break;
      }
    }
    if (!setActiveSpriteMetadata(metaPath)) {
      std::cerr << "Falling back to previous sprite metadata\n";
      if (!spriteAnimator) {
        spriteAnimator = std::make_unique<SpriteAnimator>(lveDevice, playerMeta);
      }
    }

    characterId = 0;
    bool characterAssigned = false;
    for (const auto &e : scene.entities) {
      if (e.type == EntityType::Light) {
        auto &light = createPointLightObject(e.transform.position);
        light.color = e.light ? e.light->color : glm::vec3(1.f);
        if (light.pointLight && e.light) {
          light.pointLight->lightIntensity = e.light->intensity;
        }
        light.name = !e.name.empty() ? e.name : "PointLight " + std::to_string(light.getId());
        light.transformDirty = true;
        continue;
      }

      if (e.type == EntityType::Sprite && e.sprite) {
        ObjectState desiredState = objectStateFromString(e.sprite->state);
        auto &obj = createSpriteObject(e.transform.position, desiredState, e.sprite->spriteMeta);
        obj.transform.rotation = e.transform.rotation;
        obj.transform.scale = e.transform.scale;
        obj.name = !e.name.empty() ? e.name : "Sprite " + std::to_string(obj.getId());
        obj.billboardMode = (e.sprite->billboard == BillboardKind::Spherical) ? BillboardMode::Spherical
          : (e.sprite->billboard == BillboardKind::Cylindrical ? BillboardMode::Cylindrical : BillboardMode::None);
        obj.transformDirty = true;
        if (!characterAssigned) {
          characterId = obj.getId();
          characterAssigned = true;
        }
        continue;
      }

      if (e.type == EntityType::Mesh && e.mesh) {
        auto &obj = createMeshObject(e.transform.position, e.mesh->model);
        obj.transform.rotation = e.transform.rotation;
        obj.transform.scale = e.transform.scale;
        obj.name = !e.name.empty() ? e.name : "Mesh " + std::to_string(obj.getId());
        obj.transformDirty = true;
        applyNodeOverrides(obj, *e.mesh);
        continue;
      }
    }

    if (!characterAssigned) {
      auto &characterObj = createSpriteObject({0.f, 0.f, 0.f}, ObjectState::IDLE, metaPath);
      characterId = characterObj.getId();
    }
  }

  void SceneSystem::saveSceneToFile(const std::string &path) {
    Scene scene = exportSceneSnapshot();
    if (!SceneSerializer::saveToFile(scene, path)) {
      std::cerr << "Failed to save scene to " << path << "\n";
    }
  }

  void SceneSystem::loadSceneFromFile(const std::string &path, std::optional<LveGameObject::id_t> protectedId) {
    Scene scene{};
    if (!SceneSerializer::loadFromFile(path, scene)) {
      std::cerr << "Failed to load scene from " << path << "\n";
      return;
    }
    importSceneSnapshot(scene, protectedId);
  }

  void SceneSystem::loadGameObjects() {
    resourceBrowserState.browser.pendingRefresh = true;
    resourceBrowserState.activeMeshPath = "Assets/models/colored_cube.obj";
    resourceBrowserState.activeSpriteMetaPath = "Assets/textures/characters/player.json";

    cubeModel = loadModelCached(resourceBrowserState.activeMeshPath);
    spriteModel = LveModel::createModelFromFile(lveDevice, "Assets/models/quad.obj");

    createMeshObject({-.5f, .5f, 0.f}, resourceBrowserState.activeMeshPath);

    const std::string defaultMetaPath = resourceBrowserState.activeSpriteMetaPath;
    if (!loadSpriteMetadata(defaultMetaPath, playerMeta)) {
      std::cerr << "Failed to load player sprite metadata; using defaults\n";
      playerMeta.atlasCols = 6;
      playerMeta.atlasRows = 1;
      playerMeta.size = {33.f, 44.f};
      SpriteStateInfo idle{};
      idle.row = 0; idle.frameCount = 6; idle.frameDuration = 0.15f; idle.loop = true; idle.atlasCols = 6; idle.atlasRows = 1; idle.texturePath = "Assets/textures/characters/playerIDLE.png";
      SpriteStateInfo walk{};
      walk.row = 0; walk.frameCount = 8; walk.frameDuration = 0.125f; walk.loop = true; walk.atlasCols = 8; walk.atlasRows = 1; walk.texturePath = "Assets/textures/characters/playerWalking.png";
      playerMeta.states["idle"] = idle;
      playerMeta.states["walking"] = walk;
    }
    spriteAnimator = std::make_unique<SpriteAnimator>(lveDevice, playerMeta);

    auto &characterObj = createSpriteObject({0.f, 0.f, 0.f}, ObjectState::IDLE, resourceBrowserState.activeSpriteMetaPath);
    characterId = characterObj.getId();

    std::vector<glm::vec3> lightColors{
        {1.f, .1f, .1f},
        {.1f, .1f, 1.f},
        {.1f, 1.f, .1f},
        {1.f, 1.f, .1f},
        {.1f, 1.f, 1.f},
        {1.f, 1.f, 1.f}};

    for (int i = 0; i < lightColors.size(); i++) {
      auto &pointLight = gameObjectManager.makePointLight(0.2f);
      pointLight.color = lightColors[i];
      auto rotateLight = glm::rotate(
        glm::mat4(1.f),
        (i * glm::two_pi<float>()) / lightColors.size(),
        {0.f, -1.f, 0.f});
      pointLight.transform.translation = glm::vec3(rotateLight * glm::vec4(-1.f, -1.f, -1.f, -1.f));
    }
  }
} // namespace lve
