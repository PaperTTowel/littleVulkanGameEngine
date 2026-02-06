#pragma once

#include "Engine/Backend/runtime_backend.hpp"
#include "Engine/audio_system.hpp"
#include "Game/Tilemap/tilemap_system.hpp"
#include "Game/background_system.hpp"
#include "Game/bullet_system.hpp"
#include "Game/mob_system.hpp"
#include "Game/UI/score_overlay.hpp"

// std
#include <memory>
#include <string>
#include <vector>

namespace lve {
  class LveCamera;
  class LveGameObject;
  class SceneSystem;
  class SpriteAnimator;

  namespace game {
    class PlayerController;
  }

  namespace backend {
    class EditorRenderBackend;
    class InputProvider;
    class RenderBackend;
  }

  class EngineLoop {
  public:
    static constexpr int WIDTH = 800;
    static constexpr int HEIGHT = 600;

    EngineLoop();
    ~EngineLoop();

    EngineLoop(const EngineLoop &) = delete;
    EngineLoop &operator=(const EngineLoop &) = delete;

    void run();

  private:
    void initWorld(SceneSystem &sceneSystem);
    LveGameObject *updateSimulation(
      float frameTime,
      backend::InputProvider &input,
      SceneSystem &sceneSystem,
      game::PlayerController &playerController,
      SpriteAnimator *spriteAnimator,
      std::string &tileDebugText);
    void handleGameOver(
      SceneSystem &sceneSystem,
      game::PlayerController &playerController,
      LveGameObject &character,
      std::string &tileDebugText);
    backend::CommandBufferHandle beginFrame(
      backend::RenderBackend &renderBackend,
      backend::EditorRenderBackend &debugUi,
      SceneSystem &sceneSystem);
    void updateCameraAndProjection(
      LveCamera &gameCamera,
      backend::RenderBackend &renderBackend,
      const LveGameObject &character,
      float &outOrthoWidth,
      float &outOrthoHeight);
    void renderFrameAndUi(
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
      backend::CommandBufferHandle commandBuffer);

    std::unique_ptr<backend::RuntimeBackend> runtime;
    std::unique_ptr<tilemap::TilemapSystem> tilemapSystem;
    game::BackgroundSystem backgroundSystem{};
    game::BulletSystem bulletSystem{};
    game::MobSystem mobSystem{};
    AudioSystem audioSystem{};
    game::ui::ScoreOverlay scoreOverlay{};
    bool useOrthoCamera{true};
    bool wireframeEnabled{false};
    bool normalViewEnabled{false};
    bool showEngineStats{false};
    float orthoZoomHeight{10.f};
    int score{0};
    std::string activeSignMessage{};
    float activeSignMessageTimer{0.f};
    bool interactKeyHeld{false};
    bool statsToggleKeyHeld{false};
  };
} // namespace lve



