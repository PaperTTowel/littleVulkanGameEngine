#include "engine.hpp"

// backend
#include "Backend/buffer.hpp"
#include "camera.hpp"

// rendering
#include "Rendering/simple_render_system.hpp"
#include "Rendering/sprite_render_system.hpp"
#include "Rendering/point_light_system.hpp"

// utils
#include "utils/keyboard_movement_controller.hpp"
#include "ImGui/imgui_layer.hpp"
#include "ImGui/panels/character_panel.hpp"
#include "Editor/inspector_panel.hpp"
#include "Editor/scene_panel.hpp"
#include "Engine/scene.hpp"

// libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

// std
#include <array>
#include <chrono>
#include <cassert>
#include <iostream>
#include <sstream>
#include <string>
#include <stdexcept>

namespace lve {

  ObjectState ControlApp::objectStateFromString(const std::string &name) {
    if (name == "walking" || name == "walk") return ObjectState::WALKING;
    return ObjectState::IDLE;
  }

  std::string ControlApp::objectStateToString(ObjectState state) {
    switch (state) {
      case ObjectState::WALKING: return "walking";
      case ObjectState::IDLE:
      default: return "idle";
    }
  }

  ControlApp::ControlApp() {
    globalPool = LveDescriptorPool::Builder(lveDevice)
      .setMaxSets(LveSwapChain::MAX_FRAMES_IN_FLIGHT)
      .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, LveSwapChain::MAX_FRAMES_IN_FLIGHT)
      .build();

    // build frame descriptor pools
    framePools.resize(LveSwapChain::MAX_FRAMES_IN_FLIGHT);
    auto framePoolBuilder = LveDescriptorPool::Builder(lveDevice)
      .setMaxSets(1000)
      .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000)
      .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000)
      .setPoolFlags(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT);
    for (int i = 0; i < framePools.size(); i++) {
      framePools[i] = framePoolBuilder.build();
    }
    loadGameObjects();
  }

  ControlApp::~ControlApp() {}

  LveGameObject &ControlApp::createMeshObject(const glm::vec3 &position) {
    if (!cubeModel) {
      cubeModel = LveModel::createModelFromFile(lveDevice, "Assets/models/colored_cube.obj");
    }
    auto &obj = gameObjectManager.createGameObject();
    obj.model = cubeModel;
    obj.modelPath = "Assets/models/colored_cube.obj";
    obj.enableTextureType = 0;
    obj.isSprite = false;
    obj.billboardMode = BillboardMode::None;
    obj.useOrthoCamera = false;
    obj.transform.translation = position;
    obj.transform.scale = glm::vec3(1.f);
    return obj;
  }

  LveGameObject &ControlApp::createSpriteObject(const glm::vec3 &position, ObjectState state, bool useOrtho, const std::string &metaPath) {
    if (!spriteModel) {
      spriteModel = LveModel::createModelFromFile(lveDevice, "Assets/models/quad.obj");
    }
    auto &obj = gameObjectManager.createGameObject();
    obj.model = spriteModel;
    obj.enableTextureType = 1;
    obj.isSprite = true;
    obj.billboardMode = BillboardMode::Cylindrical;
    obj.useOrthoCamera = useOrtho;
    obj.spriteMetaPath = metaPath;
    obj.transform.translation = position;
    obj.transform.rotation = {0.f, 0.f, 0.f};
    obj.objState = state;
    if (spriteAnimator) {
      spriteAnimator->applySpriteState(obj, state);
    }
    return obj;
  }

  LveGameObject &ControlApp::createPointLightObject(const glm::vec3 &position) {
    auto &light = gameObjectManager.makePointLight(0.2f);
    light.transform.translation = position;
    return light;
  }

  Scene ControlApp::exportSceneSnapshot() const {
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
      e.name = e.id;
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
        sc.useOrtho = obj.useOrthoCamera;
        sc.layer = 0;
        e.sprite = sc;
      } else {
        e.type = EntityType::Mesh;
        MeshComponent mc{};
        mc.model = obj.modelPath.empty() ? "Assets/models/colored_cube.obj" : obj.modelPath;
        mc.material = obj.materialPath;
        e.mesh = mc;
      }

      scene.entities.push_back(std::move(e));
    }

    return scene;
  }

  void ControlApp::importSceneSnapshot(const Scene &scene) {
    gameObjectManager.clearAllExcept(viewerId);
    cubeModel.reset();
    spriteModel.reset();

    // reload sprite metadata if provided by first sprite entity
    SpriteMetadata loadedMeta = playerMeta;
    std::string metaPath = "Assets/textures/characters/player.json";
    for (const auto &e : scene.entities) {
      if (e.sprite) {
        metaPath = e.sprite->spriteMeta.empty() ? metaPath : e.sprite->spriteMeta;
        break;
      }
    }
    loadSpriteMetadata(metaPath, loadedMeta);
    playerMeta = loadedMeta;
    spriteAnimator = std::make_unique<SpriteAnimator>(lveDevice, playerMeta);

    characterId = 0;
    bool characterAssigned = false;
    for (const auto &e : scene.entities) {
      if (e.type == EntityType::Light) {
        auto &light = createPointLightObject(e.transform.position);
        light.color = e.light ? e.light->color : glm::vec3(1.f);
        if (light.pointLight && e.light) {
          light.pointLight->lightIntensity = e.light->intensity;
        }
        continue;
      }

      if (e.type == EntityType::Sprite && e.sprite) {
        ObjectState desiredState = objectStateFromString(e.sprite->state);
        auto &obj = createSpriteObject(e.transform.position, desiredState, e.sprite->useOrtho, e.sprite->spriteMeta);
        obj.transform.rotation = e.transform.rotation;
        obj.transform.scale = e.transform.scale;
        obj.billboardMode = (e.sprite->billboard == BillboardKind::Spherical) ? BillboardMode::Spherical
          : (e.sprite->billboard == BillboardKind::Cylindrical ? BillboardMode::Cylindrical : BillboardMode::None);
        if (!characterAssigned) {
          characterId = obj.getId();
          characterAssigned = true;
        }
        continue;
      }

      if (e.type == EntityType::Mesh && e.mesh) {
        auto &obj = createMeshObject(e.transform.position);
        obj.transform.rotation = e.transform.rotation;
        obj.transform.scale = e.transform.scale;
        obj.modelPath = e.mesh->model;
        continue;
      }
    }

    if (!characterAssigned) {
      auto &characterObj = createSpriteObject({0.f, 0.f, 0.f}, ObjectState::IDLE, true, metaPath);
      characterId = characterObj.getId();
    }
  }

  void ControlApp::saveSceneToFile(const std::string &path) {
    Scene scene = exportSceneSnapshot();
    if (!SceneSerializer::saveToFile(scene, path)) {
      std::cerr << "Failed to save scene to " << path << "\n";
    }
  }

  void ControlApp::loadSceneFromFile(const std::string &path) {
    Scene scene{};
    if (!SceneSerializer::loadFromFile(path, scene)) {
      std::cerr << "Failed to load scene from " << path << "\n";
      return;
    }
    importSceneSnapshot(scene);
  }

  void ControlApp::run() {

    std::vector<std::unique_ptr<LveBuffer>> uboBuffers(LveSwapChain::MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < uboBuffers.size(); i++) {
      uboBuffers[i] = std::make_unique<LveBuffer>(
        lveDevice,
        sizeof(GlobalUbo),
        1,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
      uboBuffers[i]->map();
    }

    auto globalSetLayout = LveDescriptorSetLayout::Builder(lveDevice)
      .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
      .build();

    std::vector<VkDescriptorSet> globalDescriptorSets(LveSwapChain::MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < globalDescriptorSets.size(); i++) {
      auto bufferInfo = uboBuffers[i]->descriptorInfo();
      LveDescriptorWriter(*globalSetLayout, *globalPool)
        .writeBuffer(0, &bufferInfo)
        .build(globalDescriptorSets[i]);
    }

    std::cout << "Alignment: " << lveDevice.properties.limits.minUniformBufferOffsetAlignment << "\n";
    std::cout << "atom size: " << lveDevice.properties.limits.nonCoherentAtomSize << "\n";

    SimpleRenderSystem simpleRenderSystem {
      lveDevice,
      lveRenderer.getSwapChainRenderPass(),
      globalSetLayout->getDescriptorSetLayout()};
    SpriteRenderSystem spriteRenderSystem {
      lveDevice,
      lveRenderer.getSwapChainRenderPass(),
      globalSetLayout->getDescriptorSetLayout()};
    PointLightSystem pointLightSystem {
      lveDevice,
      lveRenderer.getSwapChainRenderPass(),
      globalSetLayout->getDescriptorSetLayout()};
    LveCamera camera{};
    ImGuiLayer imgui{lveWindow, lveDevice};
    imgui.init(lveRenderer.getSwapChainRenderPass());

    auto &viewerObject = gameObjectManager.createGameObject();
    viewerObject.transform.translation.z = -2.5f;
    viewerId = viewerObject.getId();

    KeyboardMovementController cameraController{};
    CharacterMovementController characterController{};

    auto currentTime = std::chrono::high_resolution_clock::now();

    while (!lveWindow.shouldClose()) {
      glfwPollEvents();

      auto newTime = std::chrono::high_resolution_clock::now();
      float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
      currentTime = newTime;
      
      // camera
      cameraController.moveInPlaneXZ(lveWindow.getGLFWwindow(), frameTime, viewerObject);
      camera.setViewYXZ(viewerObject.transform.translation, viewerObject.transform.rotation);

      float aspect = lveRenderer.getAspectRatio();
      camera.setPerspectiveProjection(glm::radians(50.f), aspect, 0.1f, 100.f);

      // character update (2D sprite)
      auto characterIt = gameObjectManager.gameObjects.find(characterId);
      if (characterIt == gameObjectManager.gameObjects.end()) {
        std::cerr << "Character object missing; cannot update\n";
        continue;
      }
      auto &character = characterIt->second;
      characterController.moveInPlaneXZ(lveWindow.getGLFWwindow(), frameTime, character);
      spriteAnimator->applySpriteState(character, character.objState);
      const SpriteStateInfo* stateInfoPtr = nullptr;
      auto stateIt = character.spriteStates.find(static_cast<int>(character.objState));
      if (stateIt != character.spriteStates.end()) {
        stateInfoPtr = &stateIt->second;
      }
      int frameCount = stateInfoPtr ? stateInfoPtr->frameCount : 6;
      float frameDuration = stateInfoPtr ? stateInfoPtr->frameDuration : 0.15f;
      gameObjectManager.updateFrame(character, frameCount, frameTime, frameDuration);

      // camera target character
      // glm::vec3 targetPos = character.transform.translation + glm::vec3(-3.0f, -2.0f, .0f);
      // viewerObject.transform.translation = glm::mix(viewerObject.transform.translation, targetPos, 0.1f);

      if (auto commandBuffer = lveRenderer.beginFrame()) {
        int frameIndex = lveRenderer.getFrameindex();
        framePools[frameIndex]->resetPool();
        FrameInfo frameInfo{
          frameIndex,
          frameTime,
          commandBuffer,
          camera,
          globalDescriptorSets[frameIndex],
          *framePools[frameIndex],
          gameObjectManager.gameObjects};

        imgui.newFrame();
        imgui.buildUI(
          frameTime,
          viewerObject.transform.translation,
          viewerObject.transform.rotation,
          wireframeEnabled,
          normalViewEnabled);

        BuildCharacterPanel(character, *spriteAnimator);
        auto hierarchyActions = editor::BuildHierarchyPanel(gameObjectManager, hierarchyState, characterId);
        auto sceneActions = editor::BuildScenePanel(scenePanelState);
        LveGameObject *selectedObj = nullptr;
        if (hierarchyState.selectedId) {
          auto itSel = gameObjectManager.gameObjects.find(*hierarchyState.selectedId);
          if (itSel != gameObjectManager.gameObjects.end()) {
            selectedObj = &itSel->second;
          }
        }
        editor::BuildInspectorPanel(
          selectedObj,
          spriteAnimator.get(),
          camera.getView(),
          camera.getProjection(),
          lveWindow.getExtent());

        glm::vec3 spawnForward{0.f, 0.f, -1.f};
        {
          glm::mat4 invView = camera.getInverseView();
          glm::vec3 forward{-invView[2][0], -invView[2][1], -invView[2][2]};
          if (glm::length(forward) > 0.0001f) {
            spawnForward = glm::normalize(forward);
          }
        }
        const glm::vec3 spawnPos = viewerObject.transform.translation + spawnForward * 2.f;

        switch (hierarchyActions.createRequest) {
          case editor::HierarchyCreateRequest::Sprite: {
            auto &obj = createSpriteObject(spawnPos, ObjectState::IDLE, false);
            hierarchyState.selectedId = obj.getId();
            break;
          }
          case editor::HierarchyCreateRequest::Mesh: {
            auto &obj = createMeshObject(spawnPos);
            hierarchyState.selectedId = obj.getId();
            break;
          }
          case editor::HierarchyCreateRequest::PointLight: {
            auto &obj = createPointLightObject(spawnPos);
            hierarchyState.selectedId = obj.getId();
            break;
          }
          case editor::HierarchyCreateRequest::None:
          default: break;
        }

        if (hierarchyActions.deleteSelected &&
            hierarchyState.selectedId &&
            *hierarchyState.selectedId != characterId) {
          if (gameObjectManager.destroyGameObject(*hierarchyState.selectedId)) {
            hierarchyState.selectedId.reset();
          }
        }

        if (sceneActions.saveRequested) {
          saveSceneToFile(scenePanelState.path);
        }
        if (sceneActions.loadRequested) {
          vkDeviceWaitIdle(lveDevice.device());
          loadSceneFromFile(scenePanelState.path);
        }

        simpleRenderSystem.setWireframe(wireframeEnabled);
        simpleRenderSystem.setNormalView(normalViewEnabled);
        spriteRenderSystem.setBillboardMode(character.billboardMode);
        spriteRenderSystem.setUseOrthoCamera(character.useOrthoCamera);

        // update
        GlobalUbo ubo{};
        ubo.projection = camera.getProjection();
        ubo.view = camera.getView();
        ubo.inverseView = camera.getInverseView();
        pointLightSystem.update(frameInfo, ubo);
        uboBuffers[frameIndex]->writeToBuffer(&ubo);
        uboBuffers[frameIndex]->flush();

        // update physics engine (add soon)
        std::vector<LveGameObject*> objPtrs;
        objPtrs.reserve(gameObjectManager.gameObjects.size());
        for (auto& [id, obj] : gameObjectManager.gameObjects) {
            objPtrs.push_back(&obj);
        }

        // final step of update is updating the game objects buffer data
        // The render functions MUST not change a game objects transform data
        gameObjectManager.updateBuffer(frameIndex);

        // render
        lveRenderer.beginSwapChainRenderPass(commandBuffer);

        // order here matters
        simpleRenderSystem.renderGameObjects(frameInfo);
        pointLightSystem.render(frameInfo);

        bool useOrthoForSprites = false;
        for (auto &kv : gameObjectManager.gameObjects) {
          auto &obj = kv.second;
          if (obj.isSprite && obj.useOrthoCamera) {
            useOrthoForSprites = true;
            break;
          }
        }
        if (useOrthoForSprites) {
          LveCamera orthoCam{};
          orthoCam.setViewYXZ(viewerObject.transform.translation, viewerObject.transform.rotation);
          float aspectOrtho = lveRenderer.getAspectRatio();
          float orthoHeight = 10.f;
          float orthoWidth = orthoHeight * aspectOrtho;
          orthoCam.setOrthographicProjection(-orthoWidth / 2.f, orthoWidth / 2.f, -orthoHeight / 2.f, orthoHeight / 2.f, -1.f, 10.f);

          GlobalUbo orthoUbo{};
          orthoUbo.projection = orthoCam.getProjection();
          orthoUbo.view = orthoCam.getView();
          orthoUbo.inverseView = orthoCam.getInverseView();
          pointLightSystem.update(frameInfo, orthoUbo);
          uboBuffers[frameIndex]->writeToBuffer(&orthoUbo);
          uboBuffers[frameIndex]->flush();
        }

        spriteRenderSystem.renderSprites(frameInfo);
        imgui.render(commandBuffer);

        lveRenderer.endSwapChainRenderPass(commandBuffer);
        lveRenderer.endFrame();
      }
    }

    imgui.shutdown();
    vkDeviceWaitIdle(lveDevice.device());
  }

  void ControlApp::loadGameObjects() {

    cubeModel = LveModel::createModelFromFile(lveDevice, "Assets/models/colored_cube.obj");
    spriteModel = LveModel::createModelFromFile(lveDevice, "Assets/models/quad.obj");

    createMeshObject({-.5f, .5f, 0.f});

    if (!loadSpriteMetadata("Assets/textures/characters/player.json", playerMeta)) {
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

    auto &characterObj = createSpriteObject({0.f, 0.f, 0.f}, ObjectState::IDLE, true);
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

