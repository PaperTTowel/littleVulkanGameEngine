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
#include <chrono>
#include <iostream>
#include <vector>

namespace lve {

  /* Engine bootstrap: initial objects */
  EngineLoop::EngineLoop() {
    sceneSystem.loadGameObjects();
    const auto &defaults = sceneSystem.getAssetDefaults();
    resourceBrowserState.browser.rootPath = defaults.rootPath;
    resourceBrowserState.browser.currentPath = defaults.rootPath;
    resourceBrowserState.browser.pendingRefresh = true;
    resourceBrowserState.activeMeshPath = defaults.activeMeshPath;
    resourceBrowserState.activeSpriteMetaPath = defaults.activeSpriteMetaPath;
    resourceBrowserState.activeMaterialPath = defaults.activeMaterialPath;
  }

  EngineLoop::~EngineLoop() {}

  /* Main loop: input -> update -> render -> UI */
  void EngineLoop::run() {

    SpriteAnimator *spriteAnimator = sceneSystem.getSpriteAnimator();

    std::cout << "Alignment: " << lveDevice.properties.limits.minUniformBufferOffsetAlignment << "\n";
    std::cout << "atom size: " << lveDevice.properties.limits.nonCoherentAtomSize << "\n";

    LveCamera editorCamera{};
    LveCamera gameCamera{};
    editorSystem.init(
      renderContext.getSwapChainRenderPass(),
      static_cast<uint32_t>(renderContext.getSwapChainImageCount()));

    auto &viewerObject = sceneSystem.createEmptyObject();
    viewerObject.transform.translation.z = -2.5f;
    viewerId = viewerObject.getId();

    KeyboardMovementController cameraController{};
    CharacterMovementController characterController{};

    ViewportInfo sceneViewInfo{};
    ViewportInfo gameViewInfo{};
    {
      VkExtent2D initialExtent = lveWindow.getExtent();
      sceneViewInfo.width = initialExtent.width;
      sceneViewInfo.height = initialExtent.height;
      sceneViewInfo.visible = true;
      gameViewInfo = sceneViewInfo;
    }

    auto currentTime = std::chrono::high_resolution_clock::now();
    std::vector<LveGameObject*> renderObjects{};

    while (!lveWindow.shouldClose()) {
      glfwPollEvents();

      auto newTime = std::chrono::high_resolution_clock::now();
      float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
      currentTime = newTime;
      
      // editor camera
      if (sceneViewInfo.hovered) {
        cameraController.moveInPlaneXZ(lveWindow.getGLFWwindow(), frameTime, viewerObject);
      }
      if (sceneViewInfo.hovered && sceneViewInfo.rightMouseDown) {
        const float mouseSensitivity = 0.003f;
        viewerObject.transform.rotation.y += sceneViewInfo.mouseDeltaX * mouseSensitivity;
        viewerObject.transform.rotation.x -= sceneViewInfo.mouseDeltaY * mouseSensitivity;
      }
      viewerObject.transformDirty = true;
      viewerObject.transform.rotation.x = glm::clamp(viewerObject.transform.rotation.x, -1.5f, 1.5f);
      viewerObject.transform.rotation.y = glm::mod(viewerObject.transform.rotation.y, glm::two_pi<float>());
      editorCamera.setViewYXZ(viewerObject.transform.translation, viewerObject.transform.rotation);

      // character update (2D sprite)
      const auto characterId = sceneSystem.getCharacterId();
      auto *characterPtr = sceneSystem.findObject(characterId);
      if (!characterPtr) {
        std::cerr << "Character object missing; cannot update\n";
        continue;
      }
      auto &character = *characterPtr;
      characterController.moveInPlaneXZ(lveWindow.getGLFWwindow(), frameTime, character);
      character.transformDirty = true;
      if (spriteAnimator) {
        const char *stateName = (character.objState == ObjectState::WALKING) ? "walking" : "idle";
        if (character.spriteStateName != stateName || !character.diffuseMap) {
          spriteAnimator->applySpriteState(character, stateName);
        }
      }
      sceneSystem.updateAnimationFrame(character, 6, frameTime, 0.15f);

      // game camera follows character
      const glm::vec3 gameCamOffset{-3.0f, -2.0f, 0.0f};
      const glm::vec3 gameCamPos = character.transform.translation + gameCamOffset;
      gameCamera.setViewTarget(gameCamPos, character.transform.translation);

      auto commandBuffer = renderContext.beginFrame();
      if (renderContext.wasSwapChainRecreated()) {
        editorSystem.onRenderPassChanged(
          renderContext.getSwapChainRenderPass(),
          static_cast<uint32_t>(renderContext.getSwapChainImageCount()));
        sceneSystem.resetDescriptorCaches();
      }
      if (commandBuffer) {
        renderContext.ensureOffscreenTargets(
          sceneViewInfo.visible ? sceneViewInfo.width : 0,
          sceneViewInfo.visible ? sceneViewInfo.height : 0,
          gameViewInfo.visible ? gameViewInfo.width : 0,
          gameViewInfo.visible ? gameViewInfo.height : 0);

        const uint32_t sceneWidth = sceneViewInfo.width > 0 ? sceneViewInfo.width : lveWindow.getExtent().width;
        const uint32_t sceneHeight = sceneViewInfo.height > 0 ? sceneViewInfo.height : lveWindow.getExtent().height;
        const float sceneAspect = sceneHeight > 0 ? (static_cast<float>(sceneWidth) / static_cast<float>(sceneHeight)) : lveRenderer.getAspectRatio();
        editorCamera.setPerspectiveProjection(glm::radians(50.f), sceneAspect, 0.1f, 100.f);

        EditorFrameResult editorResult = editorSystem.update(
          frameTime,
          viewerObject.transform.translation,
          viewerObject.transform.rotation,
          wireframeEnabled,
          normalViewEnabled,
          useOrthoCamera,
          sceneSystem,
          characterId,
          viewerId,
          spriteAnimator,
          editorCamera.getView(),
          editorCamera.getProjection(),
          VkExtent2D{sceneWidth, sceneHeight},
          resourceBrowserState,
          reinterpret_cast<void *>(renderContext.getSceneViewDescriptor()),
          reinterpret_cast<void *>(renderContext.getGameViewDescriptor()));

        sceneViewInfo = editorResult.sceneView;
        gameViewInfo = editorResult.gameView;

        // rendering toggles pushed to shader systems
        renderContext.simpleSystem().setWireframe(wireframeEnabled);
        renderContext.simpleSystem().setNormalView(normalViewEnabled);

        const uint32_t gameWidth = gameViewInfo.width > 0 ? gameViewInfo.width : lveWindow.getExtent().width;
        const uint32_t gameHeight = gameViewInfo.height > 0 ? gameViewInfo.height : lveWindow.getExtent().height;
        const float gameAspect = gameHeight > 0 ? (static_cast<float>(gameWidth) / static_cast<float>(gameHeight)) : lveRenderer.getAspectRatio();
        if (useOrthoCamera) {
          float orthoHeight = 10.f;
          float orthoWidth = orthoHeight * gameAspect;
          gameCamera.setOrthographicProjection(
            -orthoWidth / 2.f,
            orthoWidth / 2.f,
            -orthoHeight / 2.f,
            orthoHeight / 2.f,
            -1.f,
            100.f);
        } else {
          gameCamera.setPerspectiveProjection(glm::radians(50.f), gameAspect, 0.1f, 100.f);
        }

        const int frameIndex = lveRenderer.getFrameindex();
        // final step of update is updating the game objects buffer data
        // The render functions MUST not change a game objects transform data
        sceneSystem.updateBuffers(frameIndex);

        if (renderContext.beginSceneViewRenderPass(commandBuffer)) {
          sceneSystem.collectObjects(renderObjects);
          FrameInfo frameInfo = renderContext.makeFrameInfo(
            frameTime,
            editorCamera,
            renderObjects,
            commandBuffer);

          GlobalUbo ubo{};
          ubo.projection = editorCamera.getProjection();
          ubo.view = editorCamera.getView();
          ubo.inverseView = editorCamera.getInverseView();
          renderContext.pointLightSystem().update(frameInfo, ubo);
          renderContext.updateGlobalUbo(frameInfo.frameIndex, ubo);

          renderContext.simpleSystem().renderGameObjects(frameInfo);
          renderContext.pointLightSystem().render(frameInfo);
          renderContext.spriteSystem().renderSprites(frameInfo);
          renderContext.endSceneViewRenderPass(commandBuffer);
        }

        if (renderContext.beginGameViewRenderPass(commandBuffer)) {
          sceneSystem.collectObjects(renderObjects);
          FrameInfo frameInfo = renderContext.makeFrameInfo(
            frameTime,
            gameCamera,
            renderObjects,
            commandBuffer);

          GlobalUbo ubo{};
          ubo.projection = gameCamera.getProjection();
          ubo.view = gameCamera.getView();
          ubo.inverseView = gameCamera.getInverseView();
          renderContext.pointLightSystem().update(frameInfo, ubo);
          renderContext.updateGlobalUbo(frameInfo.frameIndex, ubo);

          renderContext.simpleSystem().renderGameObjects(frameInfo);
          renderContext.pointLightSystem().render(frameInfo);
          renderContext.spriteSystem().renderSprites(frameInfo);
          renderContext.endGameViewRenderPass(commandBuffer);
        }

        renderContext.beginSwapChainRenderPass(commandBuffer);
        editorSystem.render(commandBuffer);
        renderContext.endSwapChainRenderPass(commandBuffer);
        renderContext.endFrame();
        editorSystem.renderPlatformWindows();
      }
    }

    editorSystem.shutdown();
    vkDeviceWaitIdle(lveDevice.device());
  }

} // namespace lve



