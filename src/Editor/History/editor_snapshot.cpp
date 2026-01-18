#include "Editor/History/editor_snapshot.hpp"

#include "Engine/scene_system.hpp"
#include "utils/sprite_animator.hpp"

namespace lve::editor {

  GameObjectSnapshot CaptureSnapshot(const LveGameObject &obj) {
    GameObjectSnapshot snapshot{};
    snapshot.id = obj.getId();
    snapshot.isSprite = obj.isSprite;
    snapshot.isPointLight = obj.pointLight != nullptr;
    snapshot.isCamera = obj.camera.has_value();
    snapshot.transform = obj.transform;
    snapshot.color = obj.color;
    snapshot.objState = obj.objState;
    snapshot.billboardMode = obj.billboardMode;
    snapshot.spriteMetaPath = obj.spriteMetaPath;
    snapshot.spriteStateName = obj.spriteStateName;
    snapshot.modelPath = obj.modelPath;
    snapshot.materialPath = obj.materialPath;
    if (obj.camera) {
      snapshot.camera = *obj.camera;
    }
    snapshot.nodeOverrides = obj.nodeOverrides;
    snapshot.name = obj.name;
    if (obj.pointLight) {
      snapshot.lightIntensity = obj.pointLight->lightIntensity;
    }
    return snapshot;
  }

  void RestoreSnapshot(
    SceneSystem &sceneSystem,
    SpriteAnimator *animator,
    const GameObjectSnapshot &snapshot) {
    if (snapshot.isPointLight) {
      auto &obj = sceneSystem.createPointLightObjectWithId(
        snapshot.id,
        snapshot.transform.translation,
        snapshot.lightIntensity,
        snapshot.transform.scale.x,
        snapshot.color);
      obj.transform.rotation = snapshot.transform.rotation;
      obj.transform.scale = snapshot.transform.scale;
      obj.name = snapshot.name;
      obj.transformDirty = true;
      return;
    }

    if (snapshot.isSprite) {
      auto &obj = sceneSystem.createSpriteObjectWithId(
        snapshot.id,
        snapshot.transform.translation,
        snapshot.objState,
        snapshot.spriteMetaPath);
      obj.transform.rotation = snapshot.transform.rotation;
      obj.transform.scale = snapshot.transform.scale;
      obj.billboardMode = snapshot.billboardMode;
      obj.name = snapshot.name;
      if (!snapshot.spriteStateName.empty()) {
        obj.spriteStateName = snapshot.spriteStateName;
      }
      if (animator) {
        if (!obj.spriteStateName.empty()) {
          animator->applySpriteState(obj, obj.spriteStateName);
        } else {
          animator->applySpriteState(obj, obj.objState);
        }
      }
      obj.transformDirty = true;
      return;
    }

    if (snapshot.isCamera) {
      auto &obj = sceneSystem.createCameraObjectWithId(
        snapshot.id,
        snapshot.transform.translation,
        snapshot.camera);
      obj.transform.rotation = snapshot.transform.rotation;
      obj.transform.scale = snapshot.transform.scale;
      obj.name = snapshot.name;
      obj.transformDirty = true;
      if (snapshot.camera.active) {
        sceneSystem.setActiveCamera(obj.getId(), true);
      }
      return;
    }

    auto &obj = sceneSystem.createMeshObjectWithId(
      snapshot.id,
      snapshot.transform.translation,
      snapshot.modelPath);
    obj.transform.rotation = snapshot.transform.rotation;
    obj.transform.scale = snapshot.transform.scale;
    obj.name = snapshot.name;
    if (!snapshot.materialPath.empty()) {
      sceneSystem.applyMaterialToObject(obj, snapshot.materialPath);
    }
    if (!snapshot.nodeOverrides.empty()) {
      sceneSystem.ensureNodeOverrides(obj);
      obj.nodeOverrides = snapshot.nodeOverrides;
    }
    obj.transformDirty = true;
  }

} // namespace lve::editor
