#pragma once

#include "utils/game_object.hpp"

#include <string>
#include <vector>

namespace lve {
  class SceneSystem;
  class SpriteAnimator;
}

namespace lve::editor {

  struct GameObjectSnapshot {
    LveGameObject::id_t id{0};
    bool isSprite{false};
    bool isPointLight{false};
    bool isCamera{false};
    TransformComponent transform{};
    glm::vec3 color{1.f, 1.f, 1.f};
    float lightIntensity{1.f};
    ObjectState objState{ObjectState::IDLE};
    BillboardMode billboardMode{BillboardMode::None};
    std::string spriteMetaPath{};
    std::string spriteStateName{};
    std::string modelPath{};
    std::string materialPath{};
    CameraComponent camera{};
    std::vector<NodeTransformOverride> nodeOverrides{};
    std::string name{};
  };

  GameObjectSnapshot CaptureSnapshot(const LveGameObject &obj);
  void RestoreSnapshot(
    SceneSystem &sceneSystem,
    SpriteAnimator *animator,
    const GameObjectSnapshot &snapshot);

} // namespace lve::editor
