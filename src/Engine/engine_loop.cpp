#include "engine_loop.hpp"

// backend
#include "camera.hpp"
#include "Engine/Backend/Factory/runtime_backend_factory.hpp"
#include "Engine/audio_system.hpp"
#include "Engine/scene_system.hpp"
#include "Game/UI/hud_overlay.hpp"
#include "Game/player_controller.hpp"

// libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

// std
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace lve {
  namespace {
    struct FrameConfig {
      float maxFrameTime{0.05f};
    };

    struct CameraConfig {
      glm::vec3 followOffset{0.f, 0.f, 10.f};
      float orthoHeight{10.f};
      float orthoZoomStep{0.8f};
      float orthoMinHeight{3.f};
      float orthoMaxHeight{24.f};
      float perspectiveFovDegrees{50.f};
      float orthoNear{-1.f};
      float orthoFar{100.f};
      float perspectiveNear{0.1f};
      float perspectiveFar{100.f};
    };

    struct GameplayConfig {
      const char *mapPath{"Assets/map/testMap.json"};
      float mobContactDamage{10.f};
      float bulletDamageToMob{1.f};
      glm::vec3 fallbackMobSpawnOffset{2.f, 0.f, 0.f};
      int characterAnimFrames{6};
      float characterAnimFrameDuration{0.15f};
    };

    struct AudioConfig {
      const char *runtimeAudioDirectory{"Assets/audio"};
      const char *sourceAudioDirectory{"src/Assets/audio"};
      const char *seShoot{"shoot"};
      const char *seHit{"hit"};
      const char *seMobDeath{"mobDeath"};
    };

    struct SignConfig {
      float displayDurationSeconds{5.0f};
    };

    const FrameConfig kFrameConfig{};
    const CameraConfig kCameraConfig{};
    const GameplayConfig kGameplayConfig{};
    const AudioConfig kAudioConfig{};
    const SignConfig kSignConfig{};

    std::unique_ptr<backend::RuntimeBackend> createRuntime() {
      backend::RuntimeBackendConfig config{};
      config.api = backend::BackendApi::Vulkan;
      config.width = EngineLoop::WIDTH;
      config.height = EngineLoop::HEIGHT;
      config.title = "2dVK";
      auto runtimeBackend = backend::createRuntimeBackend(config);
      if (!runtimeBackend) {
        throw std::runtime_error("Runtime backend initialization failed.");
      }
      return runtimeBackend;
    }

    float computeClampedFrameTime(std::chrono::high_resolution_clock::time_point &currentTime) {
      const auto newTime = std::chrono::high_resolution_clock::now();
      float frameTime =
        std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
      if (frameTime > kFrameConfig.maxFrameTime) {
        frameTime = kFrameConfig.maxFrameTime;
      }
      currentTime = newTime;
      return frameTime;
    }

    bool overlapsAabb(const LveGameObject &a, const LveGameObject &b) {
      const float ax = a.transform.scale.x * 0.5f;
      const float ay = a.transform.scale.y * 0.5f;
      const float bx = b.transform.scale.x * 0.5f;
      const float by = b.transform.scale.y * 0.5f;
      const float dx = std::abs(a.transform.translation.x - b.transform.translation.x);
      const float dy = std::abs(a.transform.translation.y - b.transform.translation.y);
      return dx <= (ax + bx) && dy <= (ay + by);
    }

    bool initializeAudio(AudioSystem &audioSystem) {
      if (!audioSystem.init()) {
        return false;
      }
      if (audioSystem.loadFromDirectory(kAudioConfig.runtimeAudioDirectory)) {
        return true;
      }
      return audioSystem.loadFromDirectory(kAudioConfig.sourceAudioDirectory);
    }
  } // namespace

  /* Engine bootstrap: initial objects */
  EngineLoop::EngineLoop()
    : runtime{createRuntime()} {
    auto &sceneSystem = runtime->sceneSystem();
    initWorld(sceneSystem);

    if (!initializeAudio(audioSystem)) {
      std::cerr << "Audio assets not found or failed to load.\n";
    } else if (!audioSystem.playFirstBgm()) {
      std::cerr << "No BGM found to auto-play.\n";
    }
  }

  EngineLoop::~EngineLoop() {
    audioSystem.shutdown();
  }

  void EngineLoop::initWorld(SceneSystem &sceneSystem) {
    sceneSystem.loadGameObjects();

    tilemapSystem = std::make_unique<tilemap::TilemapSystem>();
    std::string mapError;
    if (!tilemapSystem->load(sceneSystem, kGameplayConfig.mapPath, &mapError)) {
      std::cerr << "Failed to load tilemap: " << mapError << "\n";
    }

    auto *character = sceneSystem.findObject(sceneSystem.getCharacterId());
    if (character) {
      glm::vec3 spawnPos{0.f, 0.f, 0.f};
      if (tilemapSystem && tilemapSystem->hasPlayerSpawnWorld()) {
        spawnPos = tilemapSystem->getPlayerSpawnWorld();
      }
      character->transform.translation = spawnPos;
      character->transformDirty = true;
    }

    std::vector<glm::vec3> mobSpawns{};
    if (tilemapSystem) {
      mobSpawns = tilemapSystem->getMobSpawnPointsWorld();
    }
    if (mobSpawns.empty()) {
      if (auto *character = sceneSystem.findObject(sceneSystem.getCharacterId())) {
        mobSpawns.push_back(character->transform.translation + kGameplayConfig.fallbackMobSpawnOffset);
      }
    }

    mobSystem.init(sceneSystem, std::move(mobSpawns));
    backgroundSystem.init(sceneSystem);
    scoreOverlay.init(sceneSystem);
    score = 0;
    activeSignMessage.clear();
    activeSignMessageTimer = 0.f;
    interactKeyHeld = false;
    statsToggleKeyHeld = false;
  }

  LveGameObject *EngineLoop::updateSimulation(
    float frameTime,
    backend::InputProvider &input,
    SceneSystem &sceneSystem,
    game::PlayerController &playerController,
    SpriteAnimator *spriteAnimator,
    std::string &tileDebugText) {
    const auto characterId = sceneSystem.getCharacterId();
    auto *characterPtr = sceneSystem.findObject(characterId);
    if (!characterPtr) {
      std::cerr << "Character object missing; cannot update\n";
      return nullptr;
    }

    auto &character = *characterPtr;

    if (activeSignMessageTimer > 0.f) {
      activeSignMessageTimer -= frameTime;
      if (activeSignMessageTimer <= 0.f) {
        activeSignMessageTimer = 0.f;
        activeSignMessage.clear();
      }
    }

    const bool interactDown = input.isKeyPressed(backend::KeyCode::E);
    if (interactDown && !interactKeyHeld && tilemapSystem) {
      std::string signMessage{};
      if (tilemapSystem->getSignMessageAtWorld(character.transform.translation, signMessage)) {
        activeSignMessage = signMessage;
        activeSignMessageTimer = kSignConfig.displayDurationSeconds;
      }
    }
    interactKeyHeld = interactDown;

    bool shouldPlayHitSe = false;
    const float hpBeforeUpdate = playerController.getStats().hp;
    playerController.update(input, frameTime, character, tilemapSystem.get(), spriteAnimator);
    if (playerController.getStats().hp < hpBeforeUpdate) {
      shouldPlayHitSe = true;
    }
    bulletSystem.update(input, frameTime, sceneSystem, character);
    if (bulletSystem.consumeShotEvent()) {
      audioSystem.playSe(kAudioConfig.seShoot);
    }

    if (spriteAnimator) {
      const char *stateName = (character.objState == ObjectState::WALKING) ? "walking" : "idle";
      if (character.spriteStateName != stateName || !character.diffuseMap) {
        spriteAnimator->applySpriteState(character, stateName);
      }
    }

    sceneSystem.updateAnimationFrame(
      character,
      kGameplayConfig.characterAnimFrames,
      frameTime,
      kGameplayConfig.characterAnimFrameDuration);
    mobSystem.update(frameTime, character.transform.translation, sceneSystem, tilemapSystem.get());

    const auto mobs = mobSystem.getMobs(sceneSystem);
    for (auto *mob : mobs) {
      if (mob && overlapsAabb(character, *mob)) {
        if (playerController.applyDamage(kGameplayConfig.mobContactDamage)) {
          shouldPlayHitSe = true;
        }
      }
    }

    const auto hitMobIds = bulletSystem.collectMobHits(sceneSystem, mobs);
    if (!hitMobIds.empty()) {
      shouldPlayHitSe = true;
    }
    for (auto mobId : hitMobIds) {
      const auto result = mobSystem.applyDamage(sceneSystem, mobId, kGameplayConfig.bulletDamageToMob);
      if (result == game::MobDamageResult::Killed) {
        score += 1;
        audioSystem.playSe(kAudioConfig.seMobDeath);
      }
    }

    if (shouldPlayHitSe) {
      audioSystem.playSe(kAudioConfig.seHit);
    }

    if (tilemapSystem) {
      tileDebugText = tilemapSystem->buildDebugString(character.transform.translation);
    } else {
      tileDebugText.clear();
    }
    std::ostringstream debugStream;
    debugStream << "Score: " << score << "\n" << tileDebugText;
    tileDebugText = debugStream.str();

    return characterPtr;
  }

  void EngineLoop::handleGameOver(
    SceneSystem &sceneSystem,
    game::PlayerController &playerController,
    LveGameObject &character,
    std::string &tileDebugText) {
    if (playerController.getStats().hp > 0.f) {
      return;
    }

    glm::vec3 respawnPos{0.f, 0.f, 0.f};
    if (tilemapSystem && tilemapSystem->hasPlayerSpawnWorld()) {
      respawnPos = tilemapSystem->getPlayerSpawnWorld();
    }

    character.transform.translation = respawnPos;
    character.transformDirty = true;
    character.objState = ObjectState::IDLE;

    playerController.resetForNewRun(respawnPos);
    bulletSystem.reset(sceneSystem);
    mobSystem.reset(sceneSystem);
    score = 0;
    activeSignMessage.clear();
    activeSignMessageTimer = 0.f;
    interactKeyHeld = false;

    tileDebugText = "Game Over - Restarted\nScore: 0";
  }

  backend::CommandBufferHandle EngineLoop::beginFrame(
    backend::RenderBackend &renderBackend,
    backend::EditorRenderBackend &debugUi,
    SceneSystem &sceneSystem) {
    auto commandBuffer = renderBackend.beginFrame();
    if (renderBackend.wasSwapChainRecreated()) {
      debugUi.onRenderPassChanged(
        renderBackend.getSwapChainRenderPass(),
        static_cast<uint32_t>(renderBackend.getSwapChainImageCount()));
      sceneSystem.resetDescriptorCaches();
    }
    return commandBuffer;
  }

  void EngineLoop::updateCameraAndProjection(
    LveCamera &gameCamera,
    backend::RenderBackend &renderBackend,
    const LveGameObject &character,
    float &outOrthoWidth,
    float &outOrthoHeight) {
    const backend::RenderExtent windowExtent = runtime->window().getExtent();
    const uint32_t windowWidth = windowExtent.width;
    const uint32_t windowHeight = windowExtent.height;
    const float windowAspect = windowHeight > 0
      ? (static_cast<float>(windowWidth) / static_cast<float>(windowHeight))
      : renderBackend.getAspectRatio();

    const glm::vec3 gameCamPos = character.transform.translation + kCameraConfig.followOffset;
    gameCamera.setViewTarget(gameCamPos, character.transform.translation);

    outOrthoHeight = kCameraConfig.orthoHeight;
    outOrthoHeight = orthoZoomHeight;
    outOrthoWidth = outOrthoHeight * windowAspect;

    if (useOrthoCamera) {
      gameCamera.setOrthographicProjection(
        -outOrthoWidth * 0.5f,
        outOrthoWidth * 0.5f,
        -outOrthoHeight * 0.5f,
        outOrthoHeight * 0.5f,
        kCameraConfig.orthoNear,
        kCameraConfig.orthoFar);
    } else {
      gameCamera.setPerspectiveProjection(
        glm::radians(kCameraConfig.perspectiveFovDegrees),
        windowAspect,
        kCameraConfig.perspectiveNear,
        kCameraConfig.perspectiveFar);
    }
  }

  void EngineLoop::renderFrameAndUi(
    float frameTime,
    LveCamera &gameCamera,
    float orthoWidth,
    float orthoHeight,
    SceneSystem &sceneSystem,
    backend::RenderBackend &renderBackend,
    backend::EditorRenderBackend &debugUi,
    game::PlayerController &playerController,
    LveGameObject &character,
    const std::string &tileDebugText,
    std::vector<LveGameObject*> &renderObjects,
    backend::CommandBufferHandle commandBuffer) {
    backgroundSystem.update(sceneSystem, character.transform.translation, orthoWidth, orthoHeight, frameTime);
    scoreOverlay.update(sceneSystem, gameCamera, orthoWidth, orthoHeight, score);

    renderBackend.setWireframe(wireframeEnabled);
    renderBackend.setNormalView(normalViewEnabled);

    const int frameIndex = renderBackend.getFrameIndex();
    sceneSystem.updateBuffers(frameIndex);
    sceneSystem.collectObjects(renderObjects);

    debugUi.newFrame();
    game::ui::drawPlayerHpBar(gameCamera, character, playerController.getStats());
    game::ui::drawTimedMessage(activeSignMessage, activeSignMessageTimer);
    const glm::vec3 cameraPos = gameCamera.getPosition();
    const glm::vec3 cameraRot{};
    debugUi.buildUI(
      frameTime,
      cameraPos,
      cameraRot,
      tileDebugText,
      wireframeEnabled,
      normalViewEnabled,
      useOrthoCamera,
      playerController.getTuning(),
      showEngineStats);

    renderBackend.beginSwapChainRenderPass(commandBuffer);
    renderBackend.renderMainView(
      frameTime,
      gameCamera,
      renderObjects,
      commandBuffer);
    debugUi.render(commandBuffer);
    renderBackend.endSwapChainRenderPass(commandBuffer);
    renderBackend.endFrame();
    debugUi.renderPlatformWindows();
  }

  /* Main loop: input -> update -> render -> UI */
  void EngineLoop::run() {
    auto &sceneSystem = runtime->sceneSystem();
    auto &renderBackend = runtime->renderBackend();
    auto &window = runtime->window();
    auto &input = window.input();
    auto &debugUi = runtime->editorBackend();

    SpriteAnimator *spriteAnimator = sceneSystem.getSpriteAnimator();

    LveCamera gameCamera{};
    debugUi.init(
      renderBackend.getSwapChainRenderPass(),
      static_cast<uint32_t>(renderBackend.getSwapChainImageCount()));

    game::PlayerController playerController{};

    auto currentTime = std::chrono::high_resolution_clock::now();
    std::vector<LveGameObject*> renderObjects{};
    std::string tileDebugText;

    while (!window.shouldClose()) {
      window.pollEvents();
      const bool f3Down = input.isKeyPressed(backend::KeyCode::F3);
      if (f3Down && !statsToggleKeyHeld) {
        showEngineStats = !showEngineStats;
      }
      statsToggleKeyHeld = f3Down;

      const float wheelDelta = input.consumeMouseWheelDeltaY();
      if (useOrthoCamera && std::abs(wheelDelta) > 0.0001f) {
        orthoZoomHeight -= wheelDelta * kCameraConfig.orthoZoomStep;
        orthoZoomHeight = std::clamp(
          orthoZoomHeight,
          kCameraConfig.orthoMinHeight,
          kCameraConfig.orthoMaxHeight);
      }

      const float frameTime = computeClampedFrameTime(currentTime);

      auto *character = updateSimulation(
        frameTime,
        input,
        sceneSystem,
        playerController,
        spriteAnimator,
        tileDebugText);
      if (!character) {
        continue;
      }
      handleGameOver(sceneSystem, playerController, *character, tileDebugText);

      auto commandBuffer = beginFrame(renderBackend, debugUi, sceneSystem);
      if (!commandBuffer) {
        continue;
      }

      float orthoWidth = 0.f;
      float orthoHeight = 0.f;
      updateCameraAndProjection(
        gameCamera,
        renderBackend,
        *character,
        orthoWidth,
        orthoHeight);

      renderFrameAndUi(
        frameTime,
        gameCamera,
        orthoWidth,
        orthoHeight,
        sceneSystem,
        renderBackend,
        debugUi,
        playerController,
        *character,
        tileDebugText,
        renderObjects,
        commandBuffer);
    }

    debugUi.shutdown();
    runtime->editorBackend().waitIdle();
  }

} // namespace lve
