#pragma once

#include "Engine/scene_system.hpp"
#include "utils/game_object.hpp"

#include <vector>

namespace lve::game {

  struct BackgroundTuning {
    float scrollSpeed{0.25f};
    float scalePadding{1.2f};
  };

  class BackgroundSystem {
  public:
    explicit BackgroundSystem(std::string texturePath = "Assets/textures/background/diagonal.png");

    void init(SceneSystem &sceneSystem);
    void update(
      SceneSystem &sceneSystem,
      const glm::vec3 &focusPosition,
      float orthoWidth,
      float orthoHeight,
      float dt);

    bool isInitialized() const { return initialized; }
    BackgroundTuning &getTuning() { return tuning; }
    const BackgroundTuning &getTuning() const { return tuning; }

  private:
    struct CloudInstance {
      LveGameObject::id_t id{0};
      float speed{0.0f};
      float baseOffsetX{0.0f};
      float baseOffsetY{0.0f};
      float bobAmplitude{0.0f};
      float bobFrequency{0.0f};
      float phase{0.0f};
      float scaleX{1.0f};
      float scaleY{1.0f};
    };

    struct WindInstance {
      LveGameObject::id_t id{0};
      float speed{0.0f};
      float baseOffsetX{0.0f};
      float baseOffsetY{0.0f};
      float bobAmplitude{0.0f};
      float bobFrequency{0.0f};
      float phase{0.0f};
      float scaleX{1.0f};
      float scaleY{1.0f};
    };

    std::string texturePath;
    std::string sunTexturePath{"Assets/textures/background/sun.png"};
    std::vector<std::string> cloudTexturePaths{
      "Assets/textures/background/cloud1.png",
      "Assets/textures/background/cloud2.png",
      "Assets/textures/background/cloud3.png",
      "Assets/textures/background/cloud4.png"};
    std::vector<std::string> windTexturePaths{
      "Assets/textures/background/wind1.png",
      "Assets/textures/background/wind2.png",
      "Assets/textures/background/wind3.png",
      "Assets/textures/background/wind4.png"};
    BackgroundTuning tuning{};
    LveGameObject::id_t backgroundId{0};
    LveGameObject::id_t sunId{0};
    std::vector<CloudInstance> clouds{};
    std::vector<WindInstance> winds{};
    bool initialized{false};
    glm::vec2 uvScroll{0.f, 0.f};
    float cloudTime{0.f};
    float windTime{0.f};
  };

} // namespace lve::game
