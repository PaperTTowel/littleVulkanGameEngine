#include "editor_system.hpp"

#include "Engine/scene_system.hpp"

#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>

namespace lve {

  namespace {
    struct GameObjectSnapshot {
      LveGameObject::id_t id{0};
      bool isSprite{false};
      bool isPointLight{false};
      editor::TransformSnapshot transform{};
      glm::vec3 color{1.f, 1.f, 1.f};
      float lightIntensity{1.f};
      ObjectState objState{ObjectState::IDLE};
      BillboardMode billboardMode{BillboardMode::None};
      std::string spriteMetaPath{};
      std::string spriteStateName{};
      std::string modelPath{};
      std::string name{};
    };

    GameObjectSnapshot captureSnapshot(const LveGameObject &obj) {
      GameObjectSnapshot snapshot{};
      snapshot.id = obj.getId();
      snapshot.isSprite = obj.isSprite;
      snapshot.isPointLight = obj.pointLight != nullptr;
      snapshot.transform = {obj.transform.translation, obj.transform.rotation, obj.transform.scale};
      snapshot.color = obj.color;
      snapshot.objState = obj.objState;
      snapshot.billboardMode = obj.billboardMode;
      snapshot.spriteMetaPath = obj.spriteMetaPath;
      snapshot.spriteStateName = obj.spriteStateName;
      snapshot.modelPath = obj.modelPath;
      snapshot.name = obj.name;
      if (obj.pointLight) {
        snapshot.lightIntensity = obj.pointLight->lightIntensity;
      }
      return snapshot;
    }

    void restoreSnapshot(
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
          animator->applySpriteState(obj, obj.objState);
        }
        obj.transformDirty = true;
        return;
      }

      auto &obj = sceneSystem.createMeshObjectWithId(
        snapshot.id,
        snapshot.transform.translation,
        snapshot.modelPath);
      obj.transform.rotation = snapshot.transform.rotation;
      obj.transform.scale = snapshot.transform.scale;
      obj.name = snapshot.name;
      obj.transformDirty = true;
    }

    struct Ray {
      glm::vec3 origin{};
      glm::vec3 direction{};
      bool valid{false};
    };

    Ray buildPickRay(const ViewportInfo &view, const glm::mat4 &viewMat, const glm::mat4 &projMat) {
      Ray ray{};
      if (!view.visible || view.width == 0 || view.height == 0) return ray;

      const float relX = (view.mousePosX - view.x) / static_cast<float>(view.width);
      const float relY = (view.mousePosY - view.y) / static_cast<float>(view.height);
      if (relX < 0.f || relX > 1.f || relY < 0.f || relY > 1.f) return ray;

      const float ndcX = relX * 2.f - 1.f;
      const float ndcY = relY * 2.f - 1.f;
      const glm::mat4 inv = glm::inverse(projMat * viewMat);

      glm::vec4 nearClip{ndcX, ndcY, 0.f, 1.f};
      glm::vec4 farClip{ndcX, ndcY, 1.f, 1.f};
      glm::vec4 nearWorld = inv * nearClip;
      glm::vec4 farWorld = inv * farClip;
      if (nearWorld.w == 0.f || farWorld.w == 0.f) return ray;
      nearWorld /= nearWorld.w;
      farWorld /= farWorld.w;

      ray.origin = glm::vec3(nearWorld);
      ray.direction = glm::normalize(glm::vec3(farWorld - nearWorld));
      ray.valid = true;
      return ray;
    }

    bool intersectSphere(const Ray &ray, const glm::vec3 &center, float radius, float &tHit) {
      glm::vec3 oc = ray.origin - center;
      float b = glm::dot(oc, ray.direction);
      float c = glm::dot(oc, oc) - radius * radius;
      float h = b * b - c;
      if (h < 0.f) return false;
      h = std::sqrt(h);
      float t = -b - h;
      if (t < 0.f) t = -b + h;
      if (t < 0.f) return false;
      tHit = t;
      return true;
    }

    bool intersectAabbLocal(
      const glm::vec3 &origin,
      const glm::vec3 &dir,
      const glm::vec3 &min,
      const glm::vec3 &max,
      float &tHit) {
      float tMin = -std::numeric_limits<float>::infinity();
      float tMax = std::numeric_limits<float>::infinity();

      for (int axis = 0; axis < 3; ++axis) {
        const float o = origin[axis];
        const float d = dir[axis];
        if (std::abs(d) < 1e-6f) {
          if (o < min[axis] || o > max[axis]) return false;
          continue;
        }
        float invD = 1.0f / d;
        float t1 = (min[axis] - o) * invD;
        float t2 = (max[axis] - o) * invD;
        if (t1 > t2) std::swap(t1, t2);
        tMin = std::max(tMin, t1);
        tMax = std::min(tMax, t2);
        if (tMin > tMax) return false;
      }

      if (tMax < 0.f) return false;
      tHit = (tMin >= 0.f) ? tMin : tMax;
      return true;
    }

    bool intersectBillboardQuad(
      const Ray &ray,
      const glm::vec3 &center,
      const glm::vec3 &right,
      const glm::vec3 &up,
      const glm::vec2 &halfSize,
      float &tHit) {
      const glm::vec3 normal = glm::normalize(glm::cross(right, up));
      float denom = glm::dot(normal, ray.direction);
      if (std::abs(denom) < 1e-6f) return false;
      float t = glm::dot(center - ray.origin, normal) / denom;
      if (t < 0.f) return false;
      glm::vec3 hit = ray.origin + ray.direction * t;
      glm::vec3 delta = hit - center;
      float localX = glm::dot(delta, right);
      float localY = glm::dot(delta, up);
      if (std::abs(localX) > halfSize.x || std::abs(localY) > halfSize.y) return false;
      tHit = t;
      return true;
    }
  } // namespace

  EditorSystem::EditorSystem(LveWindow &window, LveDevice &device)
    : imgui{window, device}, device{device} {
    gizmoOperation = static_cast<int>(ImGuizmo::TRANSLATE);
    gizmoMode = static_cast<int>(ImGuizmo::LOCAL);
  }

  void EditorSystem::init(VkRenderPass renderPass) {
    imgui.init(renderPass);
  }

  void EditorSystem::onRenderPassChanged(VkRenderPass renderPass) {
    imgui.shutdown();
    imgui.init(renderPass);
  }

  void EditorSystem::shutdown() {
    imgui.shutdown();
  }

  EditorFrameResult EditorSystem::update(
    float frameTime,
    const glm::vec3 &cameraPos,
    const glm::vec3 &cameraRot,
    bool &wireframeEnabled,
    bool &normalViewEnabled,
    bool &useOrthoCamera,
    SceneSystem &sceneSystem,
    LveGameObjectManager &gameObjectManager,
    LveGameObject::id_t protectedId,
    LveGameObject::id_t viewerId,
    SpriteAnimator *&animator,
    const glm::mat4 &view,
    const glm::mat4 &projection,
    VkExtent2D viewportExtent,
    editor::ResourceBrowserState &resourceBrowserState,
    void *sceneViewTextureId,
    void *gameViewTextureId) {

    EditorFrameResult result{};

    imgui.newFrame();
    imgui.buildUI(
      frameTime,
      cameraPos,
      cameraRot,
      wireframeEnabled,
      normalViewEnabled,
      useOrthoCamera,
      showEngineStats);

    ImGuiIO &io = ImGui::GetIO();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
      if (io.KeyShift) {
        result.redoRequested = true;
      } else {
        result.undoRequested = true;
      }
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y)) {
      result.redoRequested = true;
    }

    if (ImGui::BeginMainMenuBar()) {
      if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Save Scene")) {
          result.sceneActions.saveRequested = true;
        }
        if (ImGui::MenuItem("Load Scene")) {
          result.sceneActions.loadRequested = true;
        }
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("Edit")) {
        const bool canUndo = history.canUndo();
        const bool canRedo = history.canRedo();
        if (ImGui::MenuItem("Undo", "Ctrl+Z", false, canUndo)) {
          result.undoRequested = true;
        }
        if (ImGui::MenuItem("Redo", "Ctrl+Y", false, canRedo)) {
          result.redoRequested = true;
        }
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("Engine Stats", nullptr, &showEngineStats);
        ImGui::MenuItem("Hierarchy", nullptr, &showHierarchy);
        ImGui::MenuItem("Inspector", nullptr, &showInspector);
        ImGui::MenuItem("Scene", nullptr, &showScene);
        ImGui::MenuItem("Resource Browser", nullptr, &showResourceBrowser);
        ImGui::MenuItem("Scene View", nullptr, &showSceneView);
        ImGui::MenuItem("Game View", nullptr, &showGameView);
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("Help")) {
        ImGui::MenuItem("About", nullptr, false, false);
        ImGui::EndMenu();
      }
      ImGui::EndMainMenuBar();
    }

    editor::GizmoContext gizmoContext{};
    if (showSceneView) {
      if (ImGui::Begin("Scene View", &showSceneView, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        if (ImGui::RadioButton("Move", gizmoOperation == static_cast<int>(ImGuizmo::TRANSLATE))) {
          gizmoOperation = static_cast<int>(ImGuizmo::TRANSLATE);
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Rotate", gizmoOperation == static_cast<int>(ImGuizmo::ROTATE))) {
          gizmoOperation = static_cast<int>(ImGuizmo::ROTATE);
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Scale", gizmoOperation == static_cast<int>(ImGuizmo::SCALE))) {
          gizmoOperation = static_cast<int>(ImGuizmo::SCALE);
        }
        ImGui::SameLine();
        ImGui::Separator();
        ImGui::SameLine();
        if (ImGui::RadioButton("Local", gizmoMode == static_cast<int>(ImGuizmo::LOCAL))) {
          gizmoMode = static_cast<int>(ImGuizmo::LOCAL);
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("World", gizmoMode == static_cast<int>(ImGuizmo::WORLD))) {
          gizmoMode = static_cast<int>(ImGuizmo::WORLD);
        }

        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImVec2 contentPos = ImGui::GetCursorScreenPos();
        result.sceneView.width = static_cast<uint32_t>(avail.x > 0 ? avail.x : 0);
        result.sceneView.height = static_cast<uint32_t>(avail.y > 0 ? avail.y : 0);
        result.sceneView.visible = true;
        result.sceneView.x = contentPos.x;
        result.sceneView.y = contentPos.y;
        result.sceneView.hovered = ImGui::IsWindowHovered();
        ImGuiIO &io = ImGui::GetIO();
        result.sceneView.rightMouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Right);
        result.sceneView.leftMouseClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        result.sceneView.mouseDeltaX = io.MouseDelta.x;
        result.sceneView.mouseDeltaY = io.MouseDelta.y;
        result.sceneView.mousePosX = io.MousePos.x;
        result.sceneView.mousePosY = io.MousePos.y;
        result.sceneView.allowPick = result.sceneView.hovered && !ImGuizmo::IsOver();
        gizmoContext.drawList = ImGui::GetWindowDrawList();
        gizmoContext.x = contentPos.x;
        gizmoContext.y = contentPos.y;
        gizmoContext.width = avail.x;
        gizmoContext.height = avail.y;
        gizmoContext.valid = (avail.x > 0 && avail.y > 0);
        if (sceneViewTextureId && result.sceneView.width > 0 && result.sceneView.height > 0) {
          ImGui::Image(sceneViewTextureId, avail);
        } else {
          ImGui::TextUnformatted("Scene view not ready");
        }

        if (gizmoContext.valid && gizmoContext.drawList) {
          ImDrawList *drawList = ImGui::GetForegroundDrawList(ImGui::GetWindowViewport());
          const float gizmoSize = 70.0f;
          const float margin = 10.0f;
          const ImVec2 center{
            gizmoContext.x + gizmoContext.width - margin - gizmoSize * 0.5f,
            gizmoContext.y + margin + gizmoSize * 0.5f};

          const glm::vec3 axisX = glm::normalize(glm::vec3(view * glm::vec4(1.f, 0.f, 0.f, 0.f)));
          const glm::vec3 axisY = glm::normalize(glm::vec3(view * glm::vec4(0.f, 1.f, 0.f, 0.f)));
          const glm::vec3 axisZ = glm::normalize(glm::vec3(view * glm::vec4(0.f, 0.f, 1.f, 0.f)));

          auto drawAxis = [&](const glm::vec3 &axis, ImU32 color, const char *label) {
            ImVec2 end{
              center.x + axis.x * (gizmoSize * 0.45f),
              center.y - axis.y * (gizmoSize * 0.45f)};
            drawList->AddLine(center, end, color, 2.0f);
            drawList->AddText(
              ImVec2(end.x + 4.0f, end.y + 2.0f),
              color,
              label);
          };

          drawAxis(axisX, IM_COL32(220, 60, 60, 255), "X");
          drawAxis(axisY, IM_COL32(60, 220, 60, 255), "Y");
          drawAxis(axisZ, IM_COL32(60, 120, 220, 255), "Z");
        }
      }
      ImGui::End();
    }

    if (showGameView) {
      if (ImGui::Begin("Game View", &showGameView, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        result.gameView.width = static_cast<uint32_t>(avail.x > 0 ? avail.x : 0);
        result.gameView.height = static_cast<uint32_t>(avail.y > 0 ? avail.y : 0);
        result.gameView.visible = true;
        if (gameViewTextureId && result.gameView.width > 0 && result.gameView.height > 0) {
          ImGui::Image(gameViewTextureId, avail);
        } else {
          ImGui::TextUnformatted("Game view not ready");
        }
      }
      ImGui::End();
    }

    if (showHierarchy) {
    result.hierarchyActions = editor::BuildHierarchyPanel(gameObjectManager, hierarchyState, protectedId, &showHierarchy);
    }

    if (showScene) {
      auto sceneActions = editor::BuildScenePanel(scenePanelState, &showScene);
      result.sceneActions.saveRequested |= sceneActions.saveRequested;
      result.sceneActions.loadRequested |= sceneActions.loadRequested;
    }

    if (hierarchyState.selectedId) {
      auto itSel = gameObjectManager.gameObjects.find(*hierarchyState.selectedId);
      if (itSel != gameObjectManager.gameObjects.end()) {
        result.selectedObject = &itSel->second;
      }
    }

    if (showInspector) {
      result.inspectorActions = editor::BuildInspectorPanel(
        result.selectedObject,
        animator,
        view,
        projection,
        viewportExtent,
        &showInspector,
        gizmoContext,
        gizmoOperation,
        gizmoMode);
    }

    if (showResourceBrowser) {
      result.resourceActions = editor::BuildResourceBrowserPanel(resourceBrowserState, result.selectedObject, &showResourceBrowser);
    }

    auto &history = getHistory();
    bool historyTriggered = false;
    if (result.undoRequested) {
      history.undo();
      historyTriggered = true;
    }
    if (result.redoRequested) {
      history.redo();
      historyTriggered = true;
    }

    if (!historyTriggered && result.selectedObject) {
      const auto selectedId = result.selectedObject->getId();
      if (result.inspectorActions.transformChanged && result.inspectorActions.transformCommitted) {
        const auto before = result.inspectorActions.beforeTransform;
        const auto after = result.inspectorActions.afterTransform;
        history.push({
          "Transform",
          [&, selectedId, before]() {
            auto it = gameObjectManager.gameObjects.find(selectedId);
            if (it == gameObjectManager.gameObjects.end()) return;
            it->second.transform.translation = before.translation;
            it->second.transform.rotation = before.rotation;
            it->second.transform.scale = before.scale;
            it->second.transformDirty = true;
          },
          [&, selectedId, after]() {
            auto it = gameObjectManager.gameObjects.find(selectedId);
            if (it == gameObjectManager.gameObjects.end()) return;
            it->second.transform.translation = after.translation;
            it->second.transform.rotation = after.rotation;
            it->second.transform.scale = after.scale;
            it->second.transformDirty = true;
          }});
      }
      if (result.inspectorActions.nameChanged) {
        const std::string beforeName = result.inspectorActions.beforeName;
        const std::string afterName = result.inspectorActions.afterName;
        history.push({
          "Rename",
          [&, selectedId, beforeName]() {
            auto it = gameObjectManager.gameObjects.find(selectedId);
            if (it == gameObjectManager.gameObjects.end()) return;
            it->second.name = beforeName;
          },
          [&, selectedId, afterName]() {
            auto it = gameObjectManager.gameObjects.find(selectedId);
            if (it == gameObjectManager.gameObjects.end()) return;
            it->second.name = afterName;
          }});
      }
    }

    if (result.resourceActions.setActiveSpriteMeta) {
      if (sceneSystem.setActiveSpriteMetadata(resourceBrowserState.activeSpriteMetaPath)) {
        animator = sceneSystem.getSpriteAnimator();
      }
    }

    if (result.resourceActions.applySpriteMetaToSelection &&
        result.selectedObject &&
        result.selectedObject->isSprite) {
      if (sceneSystem.setActiveSpriteMetadata(resourceBrowserState.activeSpriteMetaPath)) {
        result.selectedObject->spriteMetaPath = resourceBrowserState.activeSpriteMetaPath;
        if (animator) {
          animator->applySpriteState(*result.selectedObject, result.selectedObject->objState);
        }
        animator = sceneSystem.getSpriteAnimator();
      }
    }

    if (result.resourceActions.applyMeshToSelection &&
        result.selectedObject &&
        result.selectedObject->model) {
      const std::string meshPath = resourceBrowserState.activeMeshPath.empty()
        ? "Assets/models/colored_cube.obj"
        : resourceBrowserState.activeMeshPath;
      try {
        result.selectedObject->model = sceneSystem.loadModelCached(meshPath);
        result.selectedObject->modelPath = meshPath;
      } catch (const std::exception &e) {
        std::cerr << "Failed to load mesh " << meshPath << ": " << e.what() << "\n";
      }
    }

    if (result.sceneView.leftMouseClicked && result.sceneView.allowPick) {
      const Ray ray = buildPickRay(result.sceneView, view, projection);
      if (ray.valid) {
        float bestT = std::numeric_limits<float>::max();
        std::optional<LveGameObject::id_t> hitId{};
        const glm::mat4 invView = glm::inverse(view);
        const glm::vec3 camRight = glm::vec3(invView[0]);
        const glm::vec3 camUp = glm::vec3(invView[1]);

        for (auto &kv : gameObjectManager.gameObjects) {
          auto &obj = kv.second;
          if (!obj.model && !obj.pointLight && !obj.isSprite) continue;
          float tHitWorld = std::numeric_limits<float>::max();
          bool hit = false;
          if (obj.pointLight) {
            float tHit = 0.f;
            if (intersectSphere(ray, obj.transform.translation, obj.transform.scale.x, tHit)) {
              tHitWorld = tHit;
              hit = true;
            }
          } else if (obj.isSprite) {
            float tHit = 0.f;
            glm::vec2 halfSize{
              std::abs(obj.transform.scale.x) * 0.5f,
              std::abs(obj.transform.scale.y) * 0.5f};
            if (intersectBillboardQuad(
                  ray,
                  obj.transform.translation,
                  camRight,
                  camUp,
                  halfSize,
                  tHit)) {
              tHitWorld = tHit - 0.01f; // slight bias toward sprites
              hit = true;
            }
          } else if (obj.model) {
            const auto &bbox = obj.model->getBoundingBox();
            glm::mat4 modelMat = obj.transform.mat4();
            glm::mat4 invModel = glm::inverse(modelMat);
            glm::vec3 localOrigin = glm::vec3(invModel * glm::vec4(ray.origin, 1.f));
            glm::vec3 localDir = glm::normalize(glm::mat3(invModel) * ray.direction);
            float tLocal = 0.f;
            if (intersectAabbLocal(localOrigin, localDir, bbox.min, bbox.max, tLocal)) {
              glm::vec3 hitLocal = localOrigin + localDir * tLocal;
              glm::vec3 hitWorld = glm::vec3(modelMat * glm::vec4(hitLocal, 1.f));
              tHitWorld = glm::length(hitWorld - ray.origin);
              hit = true;
            }
          }

          if (hit && tHitWorld < bestT) {
            bestT = tHitWorld;
            hitId = obj.getId();
          }
        }
        if (hitId) {
          setSelectedId(hitId);
        }
      }
    }

    glm::vec3 spawnForward{0.f, 0.f, -1.f};
    {
      glm::mat4 invView = glm::inverse(view);
      glm::vec3 forward{-invView[2][0], -invView[2][1], -invView[2][2]};
      if (glm::length(forward) > 0.0001f) {
        spawnForward = glm::normalize(forward);
      }
    }
    const glm::vec3 spawnPos = cameraPos + spawnForward * 2.f;
    const std::string meshPathForNew = resourceBrowserState.activeMeshPath.empty()
      ? "Assets/models/colored_cube.obj"
      : resourceBrowserState.activeMeshPath;
    const std::string spriteMetaForNew = resourceBrowserState.activeSpriteMetaPath.empty()
      ? "Assets/textures/characters/player.json"
      : resourceBrowserState.activeSpriteMetaPath;

    switch (result.hierarchyActions.createRequest) {
      case editor::HierarchyCreateRequest::Sprite: {
        auto &obj = sceneSystem.createSpriteObject(spawnPos, ObjectState::IDLE, spriteMetaForNew);
        setSelectedId(obj.getId());
        obj.transformDirty = true;
        if (!historyTriggered) {
          const GameObjectSnapshot snapshot = captureSnapshot(obj);
          history.push({
            "Create Sprite",
            [&, id = obj.getId()]() {
              gameObjectManager.destroyGameObject(id);
            },
            [&, snapshot]() {
              restoreSnapshot(sceneSystem, animator, snapshot);
              setSelectedId(snapshot.id);
            }});
        }
        break;
      }
      case editor::HierarchyCreateRequest::Mesh: {
        auto &obj = sceneSystem.createMeshObject(spawnPos, meshPathForNew);
        setSelectedId(obj.getId());
        obj.transformDirty = true;
        if (!historyTriggered) {
          const GameObjectSnapshot snapshot = captureSnapshot(obj);
          history.push({
            "Create Mesh",
            [&, id = obj.getId()]() {
              gameObjectManager.destroyGameObject(id);
            },
            [&, snapshot]() {
              restoreSnapshot(sceneSystem, animator, snapshot);
              setSelectedId(snapshot.id);
            }});
        }
        break;
      }
      case editor::HierarchyCreateRequest::PointLight: {
        auto &obj = sceneSystem.createPointLightObject(spawnPos);
        setSelectedId(obj.getId());
        obj.transformDirty = true;
        if (!historyTriggered) {
          const GameObjectSnapshot snapshot = captureSnapshot(obj);
          history.push({
            "Create Light",
            [&, id = obj.getId()]() {
              gameObjectManager.destroyGameObject(id);
            },
            [&, snapshot]() {
              restoreSnapshot(sceneSystem, animator, snapshot);
              setSelectedId(snapshot.id);
            }});
        }
        break;
      }
      case editor::HierarchyCreateRequest::None:
      default: break;
    }

    if (result.hierarchyActions.deleteSelected) {
      auto selectedId = getSelectedId();
      if (selectedId && *selectedId != protectedId) {
        GameObjectSnapshot snapshot{};
        bool hasSnapshot = false;
        auto it = gameObjectManager.gameObjects.find(*selectedId);
        if (it != gameObjectManager.gameObjects.end()) {
          snapshot = captureSnapshot(it->second);
          hasSnapshot = true;
        }
        if (gameObjectManager.destroyGameObject(*selectedId)) {
          setSelectedId(std::nullopt);
          if (!historyTriggered && hasSnapshot) {
            history.push({
              "Delete Object",
              [&, snapshot]() {
                restoreSnapshot(sceneSystem, animator, snapshot);
                setSelectedId(snapshot.id);
              },
              [&, id = *selectedId]() {
                gameObjectManager.destroyGameObject(id);
                setSelectedId(std::nullopt);
              }});
          }
        }
      }
    }

    if (result.sceneActions.saveRequested) {
      sceneSystem.saveSceneToFile(getScenePanelState().path);
    }
    if (result.sceneActions.loadRequested) {
      vkDeviceWaitIdle(device.device());
      sceneSystem.loadSceneFromFile(getScenePanelState().path, viewerId);
      animator = sceneSystem.getSpriteAnimator();
    }

    return result;
  }

  void EditorSystem::render(VkCommandBuffer commandBuffer) {
    imgui.render(commandBuffer);
  }

  void EditorSystem::renderPlatformWindows() {
    imgui.renderPlatformWindows();
  }

} // namespace lve
