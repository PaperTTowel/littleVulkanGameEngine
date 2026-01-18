#include "scene_system.hpp"

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

  SceneSystem::SceneSystem(
    backend::RenderAssetFactory &assets,
    backend::ObjectBufferPoolPtr objectBuffers)
    : assetFactory{assets},
      gameObjectManager{std::move(objectBuffers), assets.getDefaultTexture()},
      assetDatabase{"Assets"} {}

  void SceneSystem::setAssetDefaults(const AssetDefaults &defaults) {
    assetDefaults = defaults;
    if (assetDefaults.rootPath.empty()) {
      assetDefaults.rootPath = "Assets";
    }
    if (assetDefaults.activeMeshPath.empty()) {
      assetDefaults.activeMeshPath = "Assets/models/colored_cube.obj";
    }
    if (assetDefaults.activeSpriteMetaPath.empty()) {
      assetDefaults.activeSpriteMetaPath = "Assets/textures/characters/player.json";
    }
    assetDatabase.setRootPath(assetDefaults.rootPath);
  }

  void SceneSystem::setAssetRootPath(const std::string &rootPath) {
    assetDefaults.rootPath = rootPath.empty() ? "Assets" : rootPath;
    assetDatabase.setRootPath(assetDefaults.rootPath);
  }

  void SceneSystem::setActiveMeshPath(const std::string &path) {
    assetDefaults.activeMeshPath = path.empty() ? "Assets/models/colored_cube.obj" : path;
  }

  void SceneSystem::setActiveMaterialPath(const std::string &path) {
    assetDefaults.activeMaterialPath = path;
  }

  LveGameObject &SceneSystem::createEmptyObject() {
    return gameObjectManager.createGameObject();
  }

  LveGameObject *SceneSystem::findObject(LveGameObject::id_t id) {
    auto it = gameObjectManager.gameObjects.find(id);
    if (it == gameObjectManager.gameObjects.end()) return nullptr;
    return &it->second;
  }

  const LveGameObject *SceneSystem::findObject(LveGameObject::id_t id) const {
    auto it = gameObjectManager.gameObjects.find(id);
    if (it == gameObjectManager.gameObjects.end()) return nullptr;
    return &it->second;
  }

  bool SceneSystem::destroyObject(LveGameObject::id_t id) {
    return gameObjectManager.destroyGameObject(id);
  }

  void SceneSystem::collectObjects(std::vector<LveGameObject*> &out) {
    out.clear();
    out.reserve(gameObjectManager.gameObjects.size());
    for (auto &kv : gameObjectManager.gameObjects) {
      out.push_back(&kv.second);
    }
  }

  void SceneSystem::collectObjects(std::vector<const LveGameObject*> &out) const {
    out.clear();
    out.reserve(gameObjectManager.gameObjects.size());
    for (const auto &kv : gameObjectManager.gameObjects) {
      out.push_back(&kv.second);
    }
  }

  void SceneSystem::updateBuffers(int frameIndex) {
    gameObjectManager.updateBuffer(frameIndex);
  }

  void SceneSystem::resetDescriptorCaches() {
    gameObjectManager.resetDescriptorCaches();
  }

  void SceneSystem::updateAnimationFrame(
    LveGameObject &obj,
    int maxFrames,
    float frameTime,
    float animationSpeed) {
    gameObjectManager.updateFrame(obj, maxFrames, frameTime, animationSpeed);
  }

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

  std::shared_ptr<backend::RenderModel> SceneSystem::loadModelCached(const std::string &path) {
    if (path.empty()) return {};
    const std::string assetPath = path;
    auto it = modelCache.find(assetPath);
    if (it != modelCache.end()) {
      return it->second;
    }
    const std::string resolvedPath = assetDatabase.resolveAssetPath(assetPath);
    auto sharedModel = assetFactory.loadModel(resolvedPath);
    if (!sharedModel) {
      std::cerr << "Failed to load model " << resolvedPath << "\n";
      return {};
    }
    modelCache[assetPath] = sharedModel;
    if (assetPath == "Assets/models/colored_cube.obj") {
      cubeModel = sharedModel;
    }
    return sharedModel;
  }

  std::shared_ptr<backend::RenderMaterial> SceneSystem::loadMaterialCached(const std::string &path) {
    if (path.empty()) return {};
    auto it = materialCache.find(path);
    if (it != materialCache.end()) {
      return it->second;
    }
    std::string error;
    auto material = assetFactory.loadMaterial(
      path,
      &error,
      [this](const std::string &assetPath) {
        return assetDatabase.resolveAssetPath(assetPath);
      });
    if (!material) {
      std::cerr << "Failed to load material " << path;
      if (!error.empty()) {
        std::cerr << ": " << error;
      }
      std::cerr << "\n";
      return {};
    }
    materialCache[path] = material;
    return material;
  }

  bool SceneSystem::updateMaterialFromData(const std::string &path, const MaterialData &data) {
    if (path.empty()) return false;
    auto it = materialCache.find(path);
    std::shared_ptr<backend::RenderMaterial> target;
    if (it != materialCache.end()) {
      target = it->second;
    } else {
      target = assetFactory.createMaterial();
      if (target) {
        materialCache[path] = target;
      }
    }

    if (!target) {
      return false;
    }

    target->setPath(path);
    std::string error;
    const bool ok = target->applyData(
      data,
      &error,
      [this](const std::string &assetPath) {
        return assetDatabase.resolveAssetPath(assetPath);
      });
    if (!ok && !error.empty()) {
      std::cerr << "Failed to update material " << path << ": " << error << "\n";
    }
    return ok;
  }

  bool SceneSystem::applyMaterialToObject(LveGameObject &obj, const std::string &path) {
    if (path.empty()) {
      obj.materialPath.clear();
      obj.material.reset();
    } else {
      auto material = loadMaterialCached(path);
      if (!material) {
        return false;
      }
      obj.materialPath = path;
      obj.material = material;
    }

    if (obj.material && obj.material->hasBaseColorTexture()) {
      obj.enableTextureType = 1;
    } else if (obj.model && obj.model->hasAnyDiffuseTexture()) {
      obj.enableTextureType = 1;
    } else {
      obj.enableTextureType = 0;
    }
    return true;
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
    const std::string assetPath = path;
    const std::string resolvedPath = assetDatabase.resolveAssetPath(assetPath);
    if (!loadSpriteMetadata(resolvedPath, meta)) {
      std::cerr << "Failed to load sprite metadata: " << path << "\n";
      return false;
    }
    playerMeta = meta;
    assetDefaults.activeSpriteMetaPath = assetPath;
    spriteAnimator = std::make_unique<SpriteAnimator>(assetFactory, playerMeta);

    for (auto &kv : gameObjectManager.gameObjects) {
      auto &obj = kv.second;
      if (!obj.isSprite) continue;
      obj.spriteMetaPath = assetPath;
      if (!obj.spriteStateName.empty()) {
        spriteAnimator->applySpriteState(obj, obj.spriteStateName);
      } else {
        spriteAnimator->applySpriteState(obj, obj.objState);
      }
    }
    return true;
  }

  LveGameObject &SceneSystem::createMeshObject(const glm::vec3 &position, const std::string &modelPath) {
    const std::string fallbackPath = assetDefaults.activeMeshPath.empty()
      ? "Assets/models/colored_cube.obj"
      : assetDefaults.activeMeshPath;
    const std::string pathToUse = modelPath.empty() ? fallbackPath : modelPath;
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
      spriteModel = loadModelCached("Assets/models/quad.obj");
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

  LveGameObject &SceneSystem::createCameraObject(const glm::vec3 &position) {
    auto &cameraObj = gameObjectManager.createGameObject();
    cameraObj.name = "Camera " + std::to_string(cameraObj.getId());
    cameraObj.transform.translation = position;
    cameraObj.transform.rotation = {0.f, 0.f, 0.f};
    cameraObj.transform.scale = glm::vec3(1.f);
    cameraObj.transformDirty = true;
    cameraObj.camera = CameraComponent{};
    return cameraObj;
  }

  LveGameObject &SceneSystem::createMeshObjectWithId(
    LveGameObject::id_t id,
    const glm::vec3 &position,
    const std::string &modelPath) {
    const std::string fallbackPath = assetDefaults.activeMeshPath.empty()
      ? "Assets/models/colored_cube.obj"
      : assetDefaults.activeMeshPath;
    const std::string pathToUse = modelPath.empty() ? fallbackPath : modelPath;
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
      spriteModel = loadModelCached("Assets/models/quad.obj");
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

  LveGameObject &SceneSystem::createCameraObjectWithId(
    LveGameObject::id_t id,
    const glm::vec3 &position,
    const CameraComponent &camera) {
    auto &cameraObj = gameObjectManager.createGameObjectWithId(id);
    cameraObj.name = "Camera " + std::to_string(cameraObj.getId());
    cameraObj.transform.translation = position;
    cameraObj.transform.rotation = {0.f, 0.f, 0.f};
    cameraObj.transform.scale = glm::vec3(1.f);
    cameraObj.transformDirty = true;
    cameraObj.camera = camera;
    return cameraObj;
  }

  LveGameObject *SceneSystem::findActiveCamera() {
    for (auto &kv : gameObjectManager.gameObjects) {
      auto &obj = kv.second;
      if (obj.camera && obj.camera->active) {
        return &obj;
      }
    }
    return nullptr;
  }

  const LveGameObject *SceneSystem::findActiveCamera() const {
    for (const auto &kv : gameObjectManager.gameObjects) {
      const auto &obj = kv.second;
      if (obj.camera && obj.camera->active) {
        return &obj;
      }
    }
    return nullptr;
  }

  void SceneSystem::setActiveCamera(LveGameObject::id_t id, bool active) {
    for (auto &kv : gameObjectManager.gameObjects) {
      auto &obj = kv.second;
      if (!obj.camera) {
        continue;
      }
      if (obj.getId() == id) {
        obj.camera->active = active;
      } else if (active) {
        obj.camera->active = false;
      }
    }
  }

  Scene SceneSystem::exportSceneSnapshot() {
    Scene scene{};
    scene.version = 1;
    scene.resources.basePath = "Assets/";
    scene.resources.spritePath = "Assets/textures/characters/";
    scene.resources.modelPath = "Assets/models/";
    scene.resources.materialPath = "Assets/materials/";

    for (const auto &kv : gameObjectManager.gameObjects) {
      const auto &obj = kv.second;
      if (!obj.model && !obj.pointLight && !obj.isSprite && !obj.camera) {
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
        sc.spriteMetaGuid = assetDatabase.ensureMetaForAsset(sc.spriteMeta);
        sc.state = obj.spriteStateName.empty() ? objectStateToString(obj.objState) : obj.spriteStateName;
        sc.billboard = (obj.billboardMode == BillboardMode::Spherical) ? BillboardKind::Spherical
          : (obj.billboardMode == BillboardMode::Cylindrical ? BillboardKind::Cylindrical : BillboardKind::None);
        sc.layer = 0;
        e.sprite = sc;
      } else if (obj.camera) {
        e.type = EntityType::Camera;
        e.camera = *obj.camera;
      } else {
        e.type = EntityType::Mesh;
        MeshComponent mc{};
        mc.model = obj.modelPath.empty() ? "Assets/models/colored_cube.obj" : obj.modelPath;
        mc.modelGuid = assetDatabase.ensureMetaForAsset(mc.model);
        mc.material = obj.materialPath;
        if (!mc.material.empty()) {
          mc.materialGuid = assetDatabase.ensureMetaForAsset(mc.material);
        }
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
    materialCache.clear();

    auto resolveAssetPath = [this](const std::string &guid, const std::string &path) {
      if (!guid.empty()) {
        const std::string assetPath = assetDatabase.getPathForGuid(guid);
        if (!assetPath.empty()) {
          return assetPath;
        }
      }
      return path;
    };

    // reload sprite metadata if provided by first sprite entity
    std::string metaPath = assetDefaults.activeSpriteMetaPath.empty()
      ? "Assets/textures/characters/player.json"
      : assetDefaults.activeSpriteMetaPath;
    for (const auto &e : scene.entities) {
      if (e.sprite) {
        const std::string assetPath = resolveAssetPath(e.sprite->spriteMetaGuid, e.sprite->spriteMeta);
        if (!assetPath.empty()) {
          metaPath = assetPath;
        }
        break;
      }
    }
    if (!setActiveSpriteMetadata(metaPath)) {
      std::cerr << "Falling back to previous sprite metadata\n";
      if (!spriteAnimator) {
        spriteAnimator = std::make_unique<SpriteAnimator>(assetFactory, playerMeta);
      }
    }

    characterId = 0;
    bool characterAssigned = false;
    std::optional<LveGameObject::id_t> activeCameraId{};
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

      if (e.type == EntityType::Camera && e.camera) {
        auto &cameraObj = createCameraObject(e.transform.position);
        cameraObj.transform.rotation = e.transform.rotation;
        cameraObj.transform.scale = e.transform.scale;
        cameraObj.name = !e.name.empty() ? e.name : "Camera " + std::to_string(cameraObj.getId());
        cameraObj.camera = *e.camera;
        cameraObj.transformDirty = true;
        if (e.camera->active && !activeCameraId) {
          activeCameraId = cameraObj.getId();
        }
        continue;
      }

      if (e.type == EntityType::Sprite && e.sprite) {
        ObjectState desiredState = objectStateFromString(e.sprite->state);
        std::string spritePath = resolveAssetPath(e.sprite->spriteMetaGuid, e.sprite->spriteMeta);
        if (spritePath.empty()) {
          spritePath = metaPath;
        }
        auto &obj = createSpriteObject(e.transform.position, desiredState, spritePath);
        obj.transform.rotation = e.transform.rotation;
        obj.transform.scale = e.transform.scale;
        obj.name = !e.name.empty() ? e.name : "Sprite " + std::to_string(obj.getId());
        obj.billboardMode = (e.sprite->billboard == BillboardKind::Spherical) ? BillboardMode::Spherical
          : (e.sprite->billboard == BillboardKind::Cylindrical ? BillboardMode::Cylindrical : BillboardMode::None);
        if (!e.sprite->state.empty()) {
          obj.spriteStateName = e.sprite->state;
          if (spriteAnimator) {
            spriteAnimator->applySpriteState(obj, obj.spriteStateName);
          }
        }
        obj.transformDirty = true;
        if (!characterAssigned) {
          characterId = obj.getId();
          characterAssigned = true;
        }
        continue;
      }

      if (e.type == EntityType::Mesh && e.mesh) {
        const std::string modelPath = resolveAssetPath(e.mesh->modelGuid, e.mesh->model);
        auto &obj = createMeshObject(e.transform.position, modelPath);
        obj.transform.rotation = e.transform.rotation;
        obj.transform.scale = e.transform.scale;
        obj.name = !e.name.empty() ? e.name : "Mesh " + std::to_string(obj.getId());
        obj.transformDirty = true;
        applyNodeOverrides(obj, *e.mesh);
        const std::string materialPath = resolveAssetPath(e.mesh->materialGuid, e.mesh->material);
        if (!materialPath.empty()) {
          applyMaterialToObject(obj, materialPath);
        }
        continue;
      }
    }

    if (activeCameraId) {
      setActiveCamera(*activeCameraId, true);
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
    if (assetDefaults.rootPath.empty()) {
      assetDefaults.rootPath = "Assets";
    }
    if (assetDefaults.activeMeshPath.empty()) {
      assetDefaults.activeMeshPath = "Assets/models/colored_cube.obj";
    }
    if (assetDefaults.activeSpriteMetaPath.empty()) {
      assetDefaults.activeSpriteMetaPath = "Assets/textures/characters/player.json";
    }
    assetDatabase.setRootPath(assetDefaults.rootPath);
    assetDatabase.initialize();

    cubeModel = loadModelCached(assetDefaults.activeMeshPath);
    spriteModel = loadModelCached("Assets/models/quad.obj");

    createMeshObject({-.5f, .5f, 0.f}, assetDefaults.activeMeshPath);

    const std::string defaultMetaPath = assetDefaults.activeSpriteMetaPath;
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
    spriteAnimator = std::make_unique<SpriteAnimator>(assetFactory, playerMeta);

    auto &characterObj = createSpriteObject({0.f, 0.f, 0.f}, ObjectState::IDLE, assetDefaults.activeSpriteMetaPath);
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


