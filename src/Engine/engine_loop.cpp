#include "engine_loop.hpp"

// backend
#include "camera.hpp"
#include "Engine/Backend/Factory/runtime_backend_factory.hpp"
#include "Engine/scene_system.hpp"

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
#include <stdexcept>
#include <vector>

namespace lve {
  namespace {
    std::unique_ptr<backend::RuntimeBackend> createRuntime() {
      backend::RuntimeBackendConfig config{};
      config.api = backend::BackendApi::Vulkan;
      config.width = EngineLoop::WIDTH;
      config.height = EngineLoop::HEIGHT;
      config.title = "PaperTTowelEngine";
      auto runtimeBackend = backend::createRuntimeBackend(config);
      if (!runtimeBackend) {
        throw std::runtime_error("Runtime backend initialization failed.");
      }
      return runtimeBackend;
    }
  } // namespace

  /* Engine bootstrap: initial objects */
  EngineLoop::EngineLoop()
    : runtime{createRuntime()}
    , editorSystem{std::make_unique<EditorSystem>(runtime->editorBackend())} {
    auto &sceneSystem = runtime->sceneSystem();
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

    auto &sceneSystem = runtime->sceneSystem();
    auto &renderBackend = runtime->renderBackend();
    auto &window = runtime->window();
    auto &input = window.input();

    SpriteAnimator *spriteAnimator = sceneSystem.getSpriteAnimator();

    LveCamera editorCamera{};
    LveCamera gameCamera{};
    editorSystem->init(
      renderBackend.getSwapChainRenderPass(),
      static_cast<uint32_t>(renderBackend.getSwapChainImageCount()));

    auto &viewerObject = sceneSystem.createEmptyObject();
    viewerObject.transform.translation.z = -2.5f;
    viewerId = viewerObject.getId();

    KeyboardMovementController cameraController{};
    CharacterMovementController characterController{};

    ViewportInfo sceneViewInfo{};
    ViewportInfo gameViewInfo{};
    {
      const backend::RenderExtent initialExtent = window.getExtent();
      sceneViewInfo.width = initialExtent.width;
      sceneViewInfo.height = initialExtent.height;
      sceneViewInfo.visible = true;
      gameViewInfo = sceneViewInfo;
    }

    auto currentTime = std::chrono::high_resolution_clock::now();
    std::vector<LveGameObject*> renderObjects{};

    while (!window.shouldClose()) {
      window.pollEvents();

      auto newTime = std::chrono::high_resolution_clock::now();
      float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
      currentTime = newTime;
      
      // editor camera
      if (sceneViewInfo.hovered) {
        cameraController.moveInPlaneXZ(input, frameTime, viewerObject);
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
      characterController.moveInPlaneXZ(input, frameTime, character);
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

      auto commandBuffer = renderBackend.beginFrame();
      if (renderBackend.wasSwapChainRecreated()) {
        editorSystem->onRenderPassChanged(
          renderBackend.getSwapChainRenderPass(),
          static_cast<uint32_t>(renderBackend.getSwapChainImageCount()));
        sceneSystem.resetDescriptorCaches();
      }
      if (commandBuffer) {
        renderBackend.ensureOffscreenTargets(
          sceneViewInfo.visible ? sceneViewInfo.width : 0,
          sceneViewInfo.visible ? sceneViewInfo.height : 0,
          gameViewInfo.visible ? gameViewInfo.width : 0,
          gameViewInfo.visible ? gameViewInfo.height : 0);

        const backend::RenderExtent windowExtent = window.getExtent();
        const uint32_t sceneWidth = sceneViewInfo.width > 0 ? sceneViewInfo.width : windowExtent.width;
        const uint32_t sceneHeight = sceneViewInfo.height > 0 ? sceneViewInfo.height : windowExtent.height;
        const float sceneAspect = sceneHeight > 0 ? (static_cast<float>(sceneWidth) / static_cast<float>(sceneHeight)) : renderBackend.getAspectRatio();
        editorCamera.setPerspectiveProjection(glm::radians(50.f), sceneAspect, 0.1f, 100.f);

        EditorFrameResult editorResult = editorSystem->update(
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
          backend::RenderExtent{sceneWidth, sceneHeight},
          resourceBrowserState,
          renderBackend.getSceneViewDescriptor(),
          renderBackend.getGameViewDescriptor());

        sceneViewInfo = editorResult.sceneView;
        gameViewInfo = editorResult.gameView;

        // rendering toggles pushed to shader systems
        renderBackend.setWireframe(wireframeEnabled);
        renderBackend.setNormalView(normalViewEnabled);

        const uint32_t gameWidth = gameViewInfo.width > 0 ? gameViewInfo.width : windowExtent.width;
        const uint32_t gameHeight = gameViewInfo.height > 0 ? gameViewInfo.height : windowExtent.height;
        const float gameAspect = gameHeight > 0 ? (static_cast<float>(gameWidth) / static_cast<float>(gameHeight)) : renderBackend.getAspectRatio();
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

        const int frameIndex = renderBackend.getFrameIndex();
        // final step of update is updating the game objects buffer data
        // The render functions MUST not change a game objects transform data
        sceneSystem.updateBuffers(frameIndex);

        sceneSystem.collectObjects(renderObjects);
        renderBackend.renderSceneView(
          frameTime,
          editorCamera,
          renderObjects,
          commandBuffer);

        sceneSystem.collectObjects(renderObjects);
        renderBackend.renderGameView(
          frameTime,
          gameCamera,
          renderObjects,
          commandBuffer);

        renderBackend.beginSwapChainRenderPass(commandBuffer);
        editorSystem->render(commandBuffer);
        renderBackend.endSwapChainRenderPass(commandBuffer);
        renderBackend.endFrame();
        editorSystem->renderPlatformWindows();
      }
    }

    editorSystem->shutdown();
    runtime->editorBackend().waitIdle();
  }

} // namespace lve
