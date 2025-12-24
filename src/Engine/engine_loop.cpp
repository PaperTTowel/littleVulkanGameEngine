#include "engine_loop.hpp"

// backend
#include "camera.hpp"

// utils
#include "utils/keyboard_movement_controller.hpp"

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

  /* Engine bootstrap: initial objects */
  EngineLoop::EngineLoop() {
    sceneSystem.loadGameObjects();
  }

  EngineLoop::~EngineLoop() {}

  /* Main loop: input -> update -> render -> UI */
  void EngineLoop::run() {

    auto &gameObjectManager = sceneSystem.getGameObjectManager();
    auto &resourceBrowserState = sceneSystem.getResourceBrowserState();
    SpriteAnimator *spriteAnimator = sceneSystem.getSpriteAnimator();

    std::cout << "Alignment: " << lveDevice.properties.limits.minUniformBufferOffsetAlignment << "\n";
    std::cout << "atom size: " << lveDevice.properties.limits.nonCoherentAtomSize << "\n";

    LveCamera camera{};
    editorSystem.init(renderContext.getSwapChainRenderPass());

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
      if (useOrthoCamera) {
        float orthoHeight = 10.f;
        float orthoWidth = orthoHeight * aspect;
        camera.setOrthographicProjection(
          -orthoWidth / 2.f,
          orthoWidth / 2.f,
          -orthoHeight / 2.f,
          orthoHeight / 2.f,
          -1.f,
          100.f);
      } else {
        camera.setPerspectiveProjection(glm::radians(50.f), aspect, 0.1f, 100.f);
      }

      // character update (2D sprite)
      const auto characterId = sceneSystem.getCharacterId();
      auto characterIt = gameObjectManager.gameObjects.find(characterId);
      if (characterIt == gameObjectManager.gameObjects.end()) {
        std::cerr << "Character object missing; cannot update\n";
        continue;
      }
      auto &character = characterIt->second;
      characterController.moveInPlaneXZ(lveWindow.getGLFWwindow(), frameTime, character);
      if (spriteAnimator) {
        spriteAnimator->applySpriteState(character, character.objState);
      }
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

      auto commandBuffer = renderContext.beginFrame();
      if (renderContext.wasSwapChainRecreated()) {
        editorSystem.onRenderPassChanged(renderContext.getSwapChainRenderPass());
      }
      if (commandBuffer) {
        FrameInfo frameInfo = renderContext.makeFrameInfo(
          frameTime,
          camera,
          gameObjectManager.gameObjects,
          commandBuffer);

        EditorFrameResult editorResult = editorSystem.buildFrame(
          frameTime,
          viewerObject.transform.translation,
          viewerObject.transform.rotation,
          wireframeEnabled,
          normalViewEnabled,
          useOrthoCamera,
          gameObjectManager,
          characterId,
          spriteAnimator,
          camera.getView(),
          camera.getProjection(),
          lveWindow.getExtent(),
          resourceBrowserState);

        auto &hierarchyActions = editorResult.hierarchyActions;
        auto &sceneActions = editorResult.sceneActions;
        auto &resourceActions = editorResult.resourceActions;
        LveGameObject *selectedObj = editorResult.selectedObject;

        if (resourceActions.setActiveSpriteMeta) {
          if (sceneSystem.setActiveSpriteMetadata(resourceBrowserState.activeSpriteMetaPath)) {
            spriteAnimator = sceneSystem.getSpriteAnimator();
          }
        }

        if (resourceActions.applySpriteMetaToSelection &&
            selectedObj &&
            selectedObj->isSprite) {
          if (sceneSystem.setActiveSpriteMetadata(resourceBrowserState.activeSpriteMetaPath)) {
            selectedObj->spriteMetaPath = resourceBrowserState.activeSpriteMetaPath;
            if (spriteAnimator) {
              spriteAnimator->applySpriteState(*selectedObj, selectedObj->objState);
            }
            spriteAnimator = sceneSystem.getSpriteAnimator();
          }
        }

        if (resourceActions.applyMeshToSelection &&
            selectedObj &&
            selectedObj->model) {
          const std::string meshPath = resourceBrowserState.activeMeshPath.empty()
            ? "Assets/models/colored_cube.obj"
            : resourceBrowserState.activeMeshPath;
          try {
            selectedObj->model = sceneSystem.loadModelCached(meshPath);
            selectedObj->modelPath = meshPath;
          } catch (const std::exception &e) {
            std::cerr << "Failed to load mesh " << meshPath << ": " << e.what() << "\n";
          }
        }

        glm::vec3 spawnForward{0.f, 0.f, -1.f};
        {
          glm::mat4 invView = camera.getInverseView();
          glm::vec3 forward{-invView[2][0], -invView[2][1], -invView[2][2]};
          if (glm::length(forward) > 0.0001f) {
            spawnForward = glm::normalize(forward);
          }
        }
        const glm::vec3 spawnPos = viewerObject.transform.translation + spawnForward * 2.f;
        const std::string meshPathForNew = resourceBrowserState.activeMeshPath.empty()
          ? "Assets/models/colored_cube.obj"
          : resourceBrowserState.activeMeshPath;
        const std::string spriteMetaForNew = resourceBrowserState.activeSpriteMetaPath.empty()
          ? "Assets/textures/characters/player.json"
          : resourceBrowserState.activeSpriteMetaPath;

        switch (hierarchyActions.createRequest) {
          case editor::HierarchyCreateRequest::Sprite: {
            auto &obj = sceneSystem.createSpriteObject(spawnPos, ObjectState::IDLE, spriteMetaForNew);
            editorSystem.setSelectedId(obj.getId());
            break;
          }
          case editor::HierarchyCreateRequest::Mesh: {
            auto &obj = sceneSystem.createMeshObject(spawnPos, meshPathForNew);
            editorSystem.setSelectedId(obj.getId());
            break;
          }
          case editor::HierarchyCreateRequest::PointLight: {
            auto &obj = sceneSystem.createPointLightObject(spawnPos);
            editorSystem.setSelectedId(obj.getId());
            break;
          }
          case editor::HierarchyCreateRequest::None:
          default: break;
        }

        if (hierarchyActions.deleteSelected) {
          auto selectedId = editorSystem.getSelectedId();
          if (selectedId && *selectedId != characterId) {
            if (gameObjectManager.destroyGameObject(*selectedId)) {
              editorSystem.setSelectedId(std::nullopt);
            }
          }
        }

        if (sceneActions.saveRequested) {
          sceneSystem.saveSceneToFile(editorSystem.getScenePanelState().path);
        }
        if (sceneActions.loadRequested) {
          vkDeviceWaitIdle(lveDevice.device());
          sceneSystem.loadSceneFromFile(editorSystem.getScenePanelState().path, viewerId);
          spriteAnimator = sceneSystem.getSpriteAnimator();
        }

        // rendering toggles pushed to shader systems
        renderContext.simpleSystem().setWireframe(wireframeEnabled);
        renderContext.simpleSystem().setNormalView(normalViewEnabled);
        renderContext.spriteSystem().setBillboardMode(character.billboardMode);

        // update UBOs & point lights
        GlobalUbo ubo{};
        ubo.projection = camera.getProjection();
        ubo.view = camera.getView();
        ubo.inverseView = camera.getInverseView();
        renderContext.pointLightSystem().update(frameInfo, ubo);
        renderContext.updateGlobalUbo(frameInfo.frameIndex, ubo);

        // update physics engine (add soon)
        std::vector<LveGameObject*> objPtrs;
        objPtrs.reserve(gameObjectManager.gameObjects.size());
        for (auto& [id, obj] : gameObjectManager.gameObjects) {
            objPtrs.push_back(&obj);
        }

        // final step of update is updating the game objects buffer data
        // The render functions MUST not change a game objects transform data
        gameObjectManager.updateBuffer(frameInfo.frameIndex);

        // render
        renderContext.beginSwapChainRenderPass(commandBuffer);

        // order here matters
        renderContext.simpleSystem().renderGameObjects(frameInfo);
        renderContext.pointLightSystem().render(frameInfo);
        renderContext.spriteSystem().renderSprites(frameInfo);
        editorSystem.render(commandBuffer);

        renderContext.endSwapChainRenderPass(commandBuffer);
        renderContext.endFrame();
      }
    }

    editorSystem.shutdown();
    vkDeviceWaitIdle(lveDevice.device());
  }

} // namespace lve



