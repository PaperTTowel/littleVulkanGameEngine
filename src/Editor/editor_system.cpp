#include "editor_system.hpp"

#include "Engine/scene_system.hpp"

#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/glm.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>

namespace lve {

  namespace {
    namespace fs = std::filesystem;

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
      std::string materialPath{};
      std::vector<NodeTransformOverride> nodeOverrides{};
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
      snapshot.materialPath = obj.materialPath;
      snapshot.nodeOverrides = obj.nodeOverrides;
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
      if (!snapshot.materialPath.empty()) {
        sceneSystem.applyMaterialToObject(obj, snapshot.materialPath);
      }
      if (!snapshot.nodeOverrides.empty()) {
        sceneSystem.ensureNodeOverrides(obj);
        obj.nodeOverrides = snapshot.nodeOverrides;
      }
      obj.transformDirty = true;
    }

    std::string toLowerCopy(const std::string &value) {
      std::string out;
      out.reserve(value.size());
      for (char ch : value) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
      }
      return out;
    }

    bool hasExtension(const fs::path &path, std::initializer_list<const char *> exts) {
      const std::string ext = toLowerCopy(path.extension().string());
      for (const char *candidate : exts) {
        if (ext == toLowerCopy(candidate)) {
          return true;
        }
      }
      return false;
    }

    bool isMeshFile(const fs::path &path) {
      return hasExtension(path, {".obj", ".fbx", ".gltf", ".glb"});
    }

    bool isSpriteMetaFile(const fs::path &path) {
      return hasExtension(path, {".json"});
    }

    bool isMaterialFile(const fs::path &path) {
      return hasExtension(path, {".mat"});
    }

    bool isTextureFile(const fs::path &path) {
      return hasExtension(path, {".png", ".jpg", ".jpeg", ".tga", ".bmp", ".dds", ".hdr", ".tiff", ".ktx", ".ktx2"});
    }

    std::string pickImportSubdir(const fs::path &path) {
      if (isMeshFile(path)) return "models";
      if (isMaterialFile(path)) return "materials";
      if (isTextureFile(path) || isSpriteMetaFile(path)) return "textures";
      return "imported";
    }

    fs::path makeUniquePath(const fs::path &path) {
      std::error_code ec;
      if (!fs::exists(path, ec)) {
        return path;
      }
      const fs::path parent = path.parent_path();
      const std::string stem = path.stem().string();
      const std::string ext = path.extension().string();
      for (int i = 1; i < 1000; ++i) {
        fs::path candidate = parent / (stem + "_" + std::to_string(i) + ext);
        if (!fs::exists(candidate, ec)) {
          return candidate;
        }
      }
      return path;
    }

    bool copyIntoAssets(
      const fs::path &source,
      const std::string &root,
      fs::path &outPath,
      std::string &outError) {
      std::error_code ec;
      if (!fs::exists(source, ec) || fs::is_directory(source, ec)) {
        outError = "Source file not found";
        return false;
      }
      fs::path rootPath = root.empty() ? fs::path("Assets") : fs::path(root);
      fs::path targetDir = rootPath / pickImportSubdir(source);
      fs::create_directories(targetDir, ec);
      if (ec) {
        outError = "Failed to create target directory";
        return false;
      }
      fs::path destPath = makeUniquePath(targetDir / source.filename());
      fs::copy_file(source, destPath, fs::copy_options::none, ec);
      if (ec) {
        outError = "Copy failed";
        return false;
      }
      outPath = destPath;
      return true;
    }

    fs::path normalizePath(const fs::path &path) {
      std::error_code ec;
      fs::path normalized = fs::weakly_canonical(path, ec);
      if (ec) {
        normalized = path.lexically_normal();
      }
      return normalized;
    }

    bool isSubPath(const fs::path &path, const fs::path &base) {
      auto pathIt = path.begin();
      auto baseIt = base.begin();
      for (; baseIt != base.end(); ++baseIt, ++pathIt) {
        if (pathIt == path.end()) return false;
        if (*pathIt != *baseIt) return false;
      }
      return true;
    }

    std::string toAssetPath(const std::string &path, const std::string &root) {
      if (path.empty()) return {};
      fs::path inputPath(path);
      if (!inputPath.is_absolute()) {
        return inputPath.generic_string();
      }
      fs::path rootPath = root.empty() ? fs::path("Assets") : fs::path(root);
      const fs::path normalizedRoot = normalizePath(rootPath);
      const fs::path normalizedInput = normalizePath(inputPath);
      if (isSubPath(normalizedInput, normalizedRoot)) {
        std::error_code ec;
        fs::path rel = fs::relative(normalizedInput, normalizedRoot, ec);
        if (!ec) {
          return (rootPath / rel).generic_string();
        }
      }
      return inputPath.generic_string();
    }

    bool createMaterialInstance(
      SceneSystem &sceneSystem,
      const std::string &sourcePath,
      const LveModel *model,
      LveGameObject::id_t objectId,
      const std::string &root,
      std::string &outPath,
      std::string &outError) {
      MaterialData data{};
      if (!sourcePath.empty()) {
        auto material = sceneSystem.loadMaterialCached(sourcePath);
        if (material) {
          data = material->getData();
        }
      }
      if (data.name.empty()) {
        data.name = "Material_" + std::to_string(objectId);
      }
      if (data.textures.baseColor.empty() && model) {
        std::string diffusePath;
        const auto &subMeshes = model->getSubMeshes();
        for (const auto &subMesh : subMeshes) {
          diffusePath = model->getDiffusePathForSubMesh(subMesh);
          if (!diffusePath.empty()) {
            break;
          }
        }
        if (diffusePath.empty()) {
          const auto &infos = model->getMaterialPathInfo();
          for (std::size_t i = 0; i < infos.size(); ++i) {
            diffusePath = model->getDiffusePathForMaterialIndex(static_cast<int>(i));
            if (!diffusePath.empty()) {
              break;
            }
          }
        }
        if (!diffusePath.empty()) {
          data.textures.baseColor = toAssetPath(diffusePath, root);
        }
      }

      fs::path rootPath = root.empty() ? fs::path("Assets") : fs::path(root);
      fs::path targetDir = rootPath / "materials";
      fs::path targetPath = makeUniquePath(targetDir / (data.name + ".mat"));
      std::string error;
      if (!LveMaterial::saveToFile(targetPath.generic_string(), data, &error)) {
        outError = error.empty() ? "Failed to save material instance" : error;
        return false;
      }
      outPath = targetPath.generic_string();
      sceneSystem.getAssetDatabase().registerAsset(outPath);
      return true;
    }

    bool createLinkStub(
      const fs::path &source,
      const std::string &root,
      fs::path &outPath,
      std::string &outError) {
      std::error_code ec;
      if (!fs::exists(source, ec) || fs::is_directory(source, ec)) {
        outError = "Source file not found";
        return false;
      }
      fs::path rootPath = root.empty() ? fs::path("Assets") : fs::path(root);
      fs::path targetDir = rootPath / "links";
      fs::create_directories(targetDir, ec);
      if (ec) {
        outError = "Failed to create link directory";
        return false;
      }
      fs::path destPath = makeUniquePath(targetDir / source.filename());
      std::ofstream file(destPath, std::ios::out | std::ios::binary | std::ios::trunc);
      if (!file) {
        outError = "Failed to create link stub";
        return false;
      }
      outPath = destPath;
      return true;
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

    void transformAabb(
      const glm::mat4 &transform,
      const glm::vec3 &min,
      const glm::vec3 &max,
      glm::vec3 &outMin,
      glm::vec3 &outMax) {
      outMin = glm::vec3(std::numeric_limits<float>::max());
      outMax = glm::vec3(std::numeric_limits<float>::lowest());
      glm::vec3 corners[8] = {
        {min.x, min.y, min.z},
        {max.x, min.y, min.z},
        {min.x, max.y, min.z},
        {max.x, max.y, min.z},
        {min.x, min.y, max.z},
        {max.x, min.y, max.z},
        {min.x, max.y, max.z},
        {max.x, max.y, max.z}
      };
      for (const auto &corner : corners) {
        glm::vec3 world = glm::vec3(transform * glm::vec4(corner, 1.f));
        outMin = glm::min(outMin, world);
        outMax = glm::max(outMax, world);
      }
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

  void EditorSystem::init(VkRenderPass renderPass, uint32_t imageCount) {
    imgui.init(renderPass, imageCount);
  }

  void EditorSystem::onRenderPassChanged(VkRenderPass renderPass, uint32_t imageCount) {
    imgui.shutdown();
    imgui.init(renderPass, imageCount);
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
        ImGui::Separator();
        if (ImGui::MenuItem("Import")) {
          fileDialogPurpose = FileDialogPurpose::Import;
          showFileDialog = true;
          fileDialogState.title = "Import";
          fileDialogState.okLabel = "Open";
          fileDialogState.allowDirectories = false;
          fileDialogState.browser.restrictToRoot = false;
          std::error_code ec;
          const auto current = std::filesystem::current_path(ec);
          if (!ec) {
            fileDialogState.browser.rootPath = current.root_path().generic_string();
            if (fileDialogState.browser.currentPath.empty()) {
              fileDialogState.browser.currentPath = current.generic_string();
            }
          }
          fileDialogState.browser.pendingRefresh = true;
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
      const editor::MaterialPickResult materialPick = pendingMaterialPick;
      result.inspectorActions = editor::BuildInspectorPanel(
        result.selectedObject,
        animator,
        view,
        projection,
        viewportExtent,
        &showInspector,
        gizmoContext,
        gizmoOperation,
        gizmoMode,
        hierarchyState.selectedNodeIndex,
        materialPick);
      if (materialPick.available) {
        pendingMaterialPick.available = false;
      }
    }

    if (showResourceBrowser) {
      result.resourceActions = editor::BuildResourceBrowserPanel(resourceBrowserState, result.selectedObject, &showResourceBrowser);
    }

    if (showFileDialog) {
      result.fileDialogActions = editor::BuildFileDialogPanel(fileDialogState, &showFileDialog);
    }

    if (result.fileDialogActions.accepted) {
      if (fileDialogPurpose == FileDialogPurpose::Import) {
        importOptions.pendingPath = result.fileDialogActions.selectedPath;
        importOptions.error.clear();
        importOptions.mode = 0;
        importOptions.show = true;
        importOptions.openRequested = true;
      } else if (fileDialogPurpose == FileDialogPurpose::MaterialTexture) {
        const std::string rootPath = resourceBrowserState.browser.rootPath.empty()
          ? "Assets"
          : resourceBrowserState.browser.rootPath;
        pendingMaterialPick.available = true;
        pendingMaterialPick.slot = pendingMaterialPickSlot;
        pendingMaterialPick.path = toAssetPath(result.fileDialogActions.selectedPath, rootPath);
      }
      fileDialogPurpose = FileDialogPurpose::Import;
    }

    if (importOptions.openRequested) {
      ImGui::OpenPopup("Import Options");
      importOptions.openRequested = false;
    }

    if (importOptions.show) {
      bool popupOpen = true;
      if (ImGui::BeginPopupModal("Import Options", &popupOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Source: %s", importOptions.pendingPath.c_str());
        ImGui::Separator();
        if (ImGui::RadioButton("Link external file", importOptions.mode == 0)) {
          importOptions.mode = 0;
        }
        if (ImGui::RadioButton("Copy into Assets", importOptions.mode == 1)) {
          importOptions.mode = 1;
        }

        std::string previewTarget = "-";
        const fs::path srcPath = importOptions.pendingPath;
        const std::string root = resourceBrowserState.browser.rootPath.empty()
          ? "Assets"
          : resourceBrowserState.browser.rootPath;
        if (!importOptions.pendingPath.empty()) {
          fs::path targetDir = fs::path(root) / "links";
          if (importOptions.mode == 1) {
            targetDir = fs::path(root) / pickImportSubdir(srcPath);
          }
          previewTarget = (targetDir / srcPath.filename()).generic_string();
        }

        ImGui::Spacing();
        ImGui::Text("Target: %s", previewTarget.c_str());

        if (!importOptions.error.empty()) {
          ImGui::Spacing();
          ImGui::TextColored(ImVec4(1.f, 0.35f, 0.35f, 1.f), "Error: %s", importOptions.error.c_str());
        }

        ImGui::Spacing();
        bool doImport = false;
        if (ImGui::Button("Import")) {
          doImport = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
          popupOpen = false;
          ImGui::CloseCurrentPopup();
        }

        if (doImport) {
          importOptions.error.clear();
          const fs::path sourcePath = importOptions.pendingPath;
          std::string finalPath = sourcePath.generic_string();
          fs::path importedPath;

          if (importOptions.mode == 1) {
            if (!copyIntoAssets(sourcePath, resourceBrowserState.browser.rootPath, importedPath, importOptions.error)) {
              doImport = false;
            } else {
              finalPath = importedPath.generic_string();
              resourceBrowserState.browser.currentPath = importedPath.parent_path().generic_string();
              resourceBrowserState.browser.pendingRefresh = true;
              sceneSystem.getAssetDatabase().registerAsset(finalPath);
            }
          } else {
            if (!createLinkStub(sourcePath, resourceBrowserState.browser.rootPath, importedPath, importOptions.error)) {
              doImport = false;
            } else {
              finalPath = importedPath.generic_string();
              resourceBrowserState.browser.currentPath = importedPath.parent_path().generic_string();
              resourceBrowserState.browser.pendingRefresh = true;
              sceneSystem.getAssetDatabase().registerAsset(finalPath, sourcePath.generic_string());
            }
          }

          if (doImport) {
            if (isMeshFile(finalPath)) {
              resourceBrowserState.activeMeshPath = finalPath;
            } else if (isSpriteMetaFile(finalPath)) {
              resourceBrowserState.activeSpriteMetaPath = finalPath;
            } else if (isMaterialFile(finalPath)) {
              resourceBrowserState.activeMaterialPath = finalPath;
            }
            popupOpen = false;
            ImGui::CloseCurrentPopup();
          }
        }

        ImGui::EndPopup();
      }
      if (!popupOpen) {
        importOptions.show = false;
      }
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
        if (result.inspectorActions.nodeOverridesChanged &&
            result.inspectorActions.nodeOverridesCommitted) {
          const auto before = result.inspectorActions.beforeNodeOverrides;
          const auto after = result.inspectorActions.afterNodeOverrides;
          history.push({
            "Node Override",
            [&, selectedId, before]() {
              auto it = gameObjectManager.gameObjects.find(selectedId);
              if (it == gameObjectManager.gameObjects.end()) return;
              if (!it->second.model) return;
              sceneSystem.ensureNodeOverrides(it->second);
              auto &target = it->second.nodeOverrides;
              for (auto &override : target) {
                override.enabled = false;
                override.transform = TransformComponent{};
              }
              const std::size_t count = std::min(target.size(), before.size());
              for (std::size_t i = 0; i < count; ++i) {
                target[i] = before[i];
              }
            },
            [&, selectedId, after]() {
              auto it = gameObjectManager.gameObjects.find(selectedId);
              if (it == gameObjectManager.gameObjects.end()) return;
              if (!it->second.model) return;
              sceneSystem.ensureNodeOverrides(it->second);
              auto &target = it->second.nodeOverrides;
              for (auto &override : target) {
                override.enabled = false;
                override.transform = TransformComponent{};
              }
              const std::size_t count = std::min(target.size(), after.size());
              for (std::size_t i = 0; i < count; ++i) {
                target[i] = after[i];
              }
            }});
        }
      }

    if (result.resourceActions.setActiveSpriteMeta) {
      if (sceneSystem.setActiveSpriteMetadata(resourceBrowserState.activeSpriteMetaPath)) {
        animator = sceneSystem.getSpriteAnimator();
      }
    }

    if (result.resourceActions.setActiveMaterial &&
        !resourceBrowserState.activeMaterialPath.empty()) {
      sceneSystem.loadMaterialCached(resourceBrowserState.activeMaterialPath);
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
        result.selectedObject->enableTextureType =
          result.selectedObject->model && result.selectedObject->model->hasAnyDiffuseTexture() ? 1 : 0;
        result.selectedObject->nodeOverrides.clear();
        result.selectedObject->subMeshDescriptors.clear();
        sceneSystem.ensureNodeOverrides(*result.selectedObject);
        hierarchyState.selectedNodeIndex = -1;
        if (!result.selectedObject->materialPath.empty()) {
          sceneSystem.applyMaterialToObject(*result.selectedObject, result.selectedObject->materialPath);
        }
      } catch (const std::exception &e) {
        std::cerr << "Failed to load mesh " << meshPath << ": " << e.what() << "\n";
      }
    }

    if (result.resourceActions.applyMaterialToSelection &&
        result.selectedObject &&
        result.selectedObject->model) {
      if (!sceneSystem.applyMaterialToObject(*result.selectedObject, resourceBrowserState.activeMaterialPath)) {
        std::cerr << "Failed to apply material " << resourceBrowserState.activeMaterialPath << "\n";
      }
    }

    if (result.inspectorActions.materialPreviewRequested &&
        result.selectedObject &&
        result.selectedObject->model) {
      const std::string &path = result.inspectorActions.materialPath;
      if (!path.empty()) {
        sceneSystem.updateMaterialFromData(path, result.inspectorActions.materialData);
        if (!sceneSystem.applyMaterialToObject(*result.selectedObject, path)) {
          std::cerr << "Failed to apply material " << path << "\n";
        } else {
          resourceBrowserState.activeMaterialPath = path;
        }
      }
    }

    if (result.inspectorActions.materialPickRequested) {
      fileDialogPurpose = FileDialogPurpose::MaterialTexture;
      pendingMaterialPickSlot = result.inspectorActions.materialPickSlot;
      showFileDialog = true;
      fileDialogState.title = "Select Texture";
      fileDialogState.okLabel = "Select";
      fileDialogState.allowDirectories = false;
      fileDialogState.browser.restrictToRoot = true;
      fileDialogState.browser.filter.clear();
      const std::string rootPath = resourceBrowserState.browser.rootPath.empty()
        ? "Assets"
        : resourceBrowserState.browser.rootPath;
      fileDialogState.browser.rootPath = rootPath;
      fileDialogState.browser.currentPath = rootPath;
      fileDialogState.browser.pendingRefresh = true;
    }

    if (result.inspectorActions.materialClearRequested &&
        result.selectedObject &&
        result.selectedObject->model) {
      sceneSystem.applyMaterialToObject(*result.selectedObject, {});
    }

    if (result.inspectorActions.materialLoadRequested &&
        result.selectedObject &&
        result.selectedObject->model) {
      const std::string &path = result.inspectorActions.materialPath;
      if (!sceneSystem.applyMaterialToObject(*result.selectedObject, path)) {
        std::cerr << "Failed to apply material " << path << "\n";
      } else {
        resourceBrowserState.activeMaterialPath = path;
      }
    }

    if (result.inspectorActions.materialSaveRequested &&
        result.selectedObject &&
        result.selectedObject->model) {
      const std::string &path = result.inspectorActions.materialPath;
      std::string error;
      if (!LveMaterial::saveToFile(path, result.inspectorActions.materialData, &error)) {
        std::cerr << "Failed to save material " << path;
        if (!error.empty()) {
          std::cerr << ": " << error;
        }
        std::cerr << "\n";
      } else {
        sceneSystem.getAssetDatabase().registerAsset(path);
        sceneSystem.updateMaterialFromData(path, result.inspectorActions.materialData);
        if (!sceneSystem.applyMaterialToObject(*result.selectedObject, path)) {
          std::cerr << "Failed to apply material " << path << "\n";
        } else {
          resourceBrowserState.activeMaterialPath = path;
        }
      }
    }

    if (result.sceneView.leftMouseClicked && result.sceneView.allowPick) {
      const Ray ray = buildPickRay(result.sceneView, view, projection);
      if (ray.valid) {
        float bestT = std::numeric_limits<float>::max();
        std::optional<LveGameObject::id_t> hitId{};
        std::optional<int> hitNodeIndex{};
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
            const auto &nodes = obj.model->getNodes();
            const auto &subMeshes = obj.model->getSubMeshes();
            if (!nodes.empty() && !subMeshes.empty()) {
              std::vector<glm::mat4> localOverrides(nodes.size(), glm::mat4(1.f));
              if (obj.nodeOverrides.size() == nodes.size()) {
                for (std::size_t i = 0; i < nodes.size(); ++i) {
                  const auto &override = obj.nodeOverrides[i];
                  if (override.enabled) {
                    localOverrides[i] = override.transform.mat4();
                  }
                }
              }
              std::vector<glm::mat4> nodeGlobals;
              obj.model->computeNodeGlobals(localOverrides, nodeGlobals);

              const glm::mat4 objectTransform = obj.transform.mat4();
              for (std::size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex) {
                const auto &node = nodes[nodeIndex];
                if (node.meshes.empty()) continue;

                bool hasNodeBounds = false;
                glm::vec3 nodeMin;
                glm::vec3 nodeMax;
                const glm::mat4 nodeTransform = objectTransform * nodeGlobals[nodeIndex];
                for (int meshIndex : node.meshes) {
                  if (meshIndex < 0 || static_cast<std::size_t>(meshIndex) >= subMeshes.size()) {
                    continue;
                  }
                  const auto &subMesh = subMeshes[static_cast<std::size_t>(meshIndex)];
                  if (!subMesh.hasBounds) continue;
                  glm::vec3 worldMin;
                  glm::vec3 worldMax;
                  transformAabb(nodeTransform, subMesh.boundsMin, subMesh.boundsMax, worldMin, worldMax);
                  if (!hasNodeBounds) {
                    nodeMin = worldMin;
                    nodeMax = worldMax;
                    hasNodeBounds = true;
                  } else {
                    nodeMin = glm::min(nodeMin, worldMin);
                    nodeMax = glm::max(nodeMax, worldMax);
                  }
                }

                if (!hasNodeBounds) continue;
                float tHit = 0.f;
                if (intersectAabbLocal(ray.origin, ray.direction, nodeMin, nodeMax, tHit)) {
                  tHitWorld = tHit;
                  hit = true;
                  if (tHitWorld < bestT) {
                    bestT = tHitWorld;
                    hitId = obj.getId();
                    hitNodeIndex = static_cast<int>(nodeIndex);
                  }
                }
              }
            } else {
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
          }

          if (hit && tHitWorld < bestT) {
            bestT = tHitWorld;
            hitId = obj.getId();
          }
        }
        if (hitId) {
          setSelectedId(hitId);
          if (hitNodeIndex.has_value()) {
            setSelectedNodeIndex(*hitNodeIndex);
          }
        }
      }
    }

    glm::vec3 spawnForward{0.f, 0.f, 1.f};
    glm::vec3 spawnOrigin = cameraPos;
    {
      glm::mat4 invView = glm::inverse(view);
      spawnOrigin = glm::vec3(invView[3]);
      glm::vec3 forward{invView[2][0], invView[2][1], invView[2][2]};
      if (glm::length(forward) > 0.0001f) {
        spawnForward = glm::normalize(forward);
      }
    }
    const glm::vec3 spawnPos = spawnOrigin + spawnForward * 2.f;
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
        {
          std::string instancePath;
          std::string error;
          const std::string rootPath = resourceBrowserState.browser.rootPath.empty()
            ? "Assets"
            : resourceBrowserState.browser.rootPath;
          const std::string sourceMaterial = resourceBrowserState.activeMaterialPath;
          if (createMaterialInstance(
                sceneSystem,
                sourceMaterial,
                obj.model.get(),
                obj.getId(),
                rootPath,
                instancePath,
                error)) {
            if (!sceneSystem.applyMaterialToObject(obj, instancePath)) {
              std::cerr << "Failed to apply material " << instancePath << "\n";
            }
          } else {
            if (!error.empty()) {
              std::cerr << "Failed to create material instance: " << error << "\n";
            }
            if (!sourceMaterial.empty()) {
              sceneSystem.applyMaterialToObject(obj, sourceMaterial);
            }
          }
        }
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
