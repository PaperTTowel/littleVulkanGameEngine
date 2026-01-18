#include "editor_system.hpp"

#include "Editor/History/editor_snapshot.hpp"
#include "Editor/Tools/editor_picking.hpp"
#include "Editor/Workflow/editor_import.hpp"
#include "Engine/IO/material_io.hpp"
#include "Engine/scene_system.hpp"

#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace lve {

  namespace {
    const char *kGameViewCameraWarning = u8"\uC9C0\uC815 Game View \uCE74\uBA54\uB77C\uAC00 \uC0DD\uC131\uB418\uC9C0 \uC54A\uC558\uC2B5\uB2C8\uB2E4.\n"
      u8"\uCE90\uB9AD\uD130 \uACE0\uC815\uC2DC\uC810 \uCE74\uBA54\uB77C\uB85C \uC790\uB3D9\uC73C\uB85C \uC720\uC9C0\uB429\uB2C8\uB2E4";
  } // namespace

  namespace fs = std::filesystem;

  EditorSystem::EditorSystem(backend::EditorRenderBackend &renderBackend)
    : renderBackend{renderBackend} {
    gizmoOperation = static_cast<int>(ImGuizmo::TRANSLATE);
    gizmoMode = static_cast<int>(ImGuizmo::LOCAL);
  }

  void EditorSystem::init(backend::RenderPassHandle renderPass, std::uint32_t imageCount) {
    renderBackend.init(renderPass, imageCount);
  }

  void EditorSystem::onRenderPassChanged(backend::RenderPassHandle renderPass, std::uint32_t imageCount) {
    renderBackend.onRenderPassChanged(renderPass, imageCount);
  }

  void EditorSystem::shutdown() {
    renderBackend.shutdown();
  }

  EditorFrameResult EditorSystem::update(
    float frameTime,
    const glm::vec3 &cameraPos,
    const glm::vec3 &cameraRot,
    bool &wireframeEnabled,
    bool &normalViewEnabled,
    bool &useOrthoCamera,
    SceneSystem &sceneSystem,
    LveGameObject::id_t protectedId,
    LveGameObject::id_t viewerId,
    SpriteAnimator *&animator,
    const glm::mat4 &view,
    const glm::mat4 &projection,
    backend::RenderExtent viewportExtent,
    editor::ResourceBrowserState &resourceBrowserState,
    void *sceneViewTextureId,
    void *gameViewTextureId) {

    EditorFrameResult result{};

    std::vector<LveGameObject*> objects;
    sceneSystem.collectObjects(objects);

    buildFrameUI(
      result,
      frameTime,
      cameraPos,
      cameraRot,
      wireframeEnabled,
      normalViewEnabled,
      useOrthoCamera,
      sceneSystem,
      objects,
      protectedId,
      animator,
      view,
      projection,
      viewportExtent,
      resourceBrowserState,
      sceneViewTextureId,
      gameViewTextureId);

    const bool historyTriggered = applyHistoryActions(result, sceneSystem);

    applyResourceActions(result, sceneSystem, animator, resourceBrowserState);
    applyInspectorActions(result, sceneSystem, animator, resourceBrowserState);
    handlePicking(result, objects, view, projection);
    handleCreateDelete(
      result,
      sceneSystem,
      animator,
      view,
      cameraPos,
      resourceBrowserState,
      protectedId,
      historyTriggered);
    handleSceneActions(result, sceneSystem, animator, viewerId);

    return result;
  }

  void EditorSystem::buildFrameUI(
    EditorFrameResult &result,
    float frameTime,
    const glm::vec3 &cameraPos,
    const glm::vec3 &cameraRot,
    bool &wireframeEnabled,
    bool &normalViewEnabled,
    bool &useOrthoCamera,
    SceneSystem &sceneSystem,
    const std::vector<LveGameObject*> &objects,
    LveGameObject::id_t protectedId,
    SpriteAnimator *&animator,
    const glm::mat4 &view,
    const glm::mat4 &projection,
    backend::RenderExtent viewportExtent,
    editor::ResourceBrowserState &resourceBrowserState,
    void *sceneViewTextureId,
    void *gameViewTextureId) {

    renderBackend.newFrame();
    renderBackend.buildUI(
      frameTime,
      cameraPos,
      cameraRot,
      wireframeEnabled,
      normalViewEnabled,
      useOrthoCamera,
      showEngineStats);
    const bool showCameraWarning =
      showGameViewCameraWarning && sceneSystem.findActiveCamera() == nullptr;

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
        ImGui::MenuItem("Game View Camera Warning", nullptr, &showGameViewCameraWarning);
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
        ImVec2 contentPos = ImGui::GetCursorScreenPos();
        result.gameView.width = static_cast<uint32_t>(avail.x > 0 ? avail.x : 0);
        result.gameView.height = static_cast<uint32_t>(avail.y > 0 ? avail.y : 0);
        result.gameView.visible = true;
        if (gameViewTextureId && result.gameView.width > 0 && result.gameView.height > 0) {
          ImGui::Image(gameViewTextureId, avail);
        } else {
          ImGui::TextUnformatted("Game view not ready");
        }
        if (showCameraWarning) {
          ImDrawList *drawList = ImGui::GetWindowDrawList();
          ImVec2 textSize = ImGui::CalcTextSize(kGameViewCameraWarning);
          const ImVec2 padding{8.f, 6.f};
          const ImVec2 origin{contentPos.x + 12.f, contentPos.y + 12.f};
          const ImVec2 bgMin{origin.x, origin.y};
          const ImVec2 bgMax{origin.x + textSize.x + padding.x * 2.f, origin.y + textSize.y + padding.y * 2.f};
          drawList->AddRectFilled(bgMin, bgMax, IM_COL32(20, 20, 20, 200), 4.f);
          drawList->AddRect(bgMin, bgMax, IM_COL32(255, 200, 120, 200), 4.f);
          drawList->AddText(
            ImVec2(origin.x + padding.x, origin.y + padding.y),
            IM_COL32(255, 200, 120, 255),
            kGameViewCameraWarning);
        }
      }
      ImGui::End();
    }

    if (showHierarchy) {
      result.hierarchyActions = editor::BuildHierarchyPanel(
        objects,
        hierarchyState,
        protectedId,
        &showHierarchy);
    }

    if (showScene) {
      auto sceneActions = editor::BuildScenePanel(scenePanelState, &showScene);
      result.sceneActions.saveRequested |= sceneActions.saveRequested;
      result.sceneActions.loadRequested |= sceneActions.loadRequested;
    }

    if (hierarchyState.selectedId) {
      result.selectedObject = sceneSystem.findObject(*hierarchyState.selectedId);
    }

    if (showInspector) {
      const editor::MaterialPickResult materialPick = pendingMaterialPick;
      result.inspectorActions = editor::BuildInspectorPanel(
        result.selectedObject,
        animator,
        inspectorState,
        renderBackend,
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
      result.resourceActions = editor::BuildResourceBrowserPanel(
        resourceBrowserState,
        result.selectedObject,
        &showResourceBrowser);
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
        pendingMaterialPick.path = editor::workflow::ToAssetPath(result.fileDialogActions.selectedPath, rootPath);
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
            targetDir = fs::path(root) / editor::workflow::PickImportSubdir(srcPath);
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
            if (!editor::workflow::CopyIntoAssets(sourcePath, resourceBrowserState.browser.rootPath, importedPath, importOptions.error)) {
              doImport = false;
            } else {
              finalPath = importedPath.generic_string();
              resourceBrowserState.browser.currentPath = importedPath.parent_path().generic_string();
              resourceBrowserState.browser.pendingRefresh = true;
              sceneSystem.getAssetDatabase().registerAsset(finalPath);
            }
          } else {
            if (!editor::workflow::CreateLinkStub(sourcePath, resourceBrowserState.browser.rootPath, importedPath, importOptions.error)) {
              doImport = false;
            } else {
              finalPath = importedPath.generic_string();
              resourceBrowserState.browser.currentPath = importedPath.parent_path().generic_string();
              resourceBrowserState.browser.pendingRefresh = true;
              sceneSystem.getAssetDatabase().registerAsset(finalPath, sourcePath.generic_string());
            }
          }

          if (doImport) {
            if (editor::workflow::IsMeshFile(finalPath)) {
              resourceBrowserState.activeMeshPath = finalPath;
            } else if (editor::workflow::IsSpriteMetaFile(finalPath)) {
              resourceBrowserState.activeSpriteMetaPath = finalPath;
            } else if (editor::workflow::IsMaterialFile(finalPath)) {
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
  }

  bool EditorSystem::applyHistoryActions(
    EditorFrameResult &result,
    SceneSystem &sceneSystem) {
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
            auto *obj = sceneSystem.findObject(selectedId);
            if (!obj) return;
            obj->transform.translation = before.translation;
            obj->transform.rotation = before.rotation;
            obj->transform.scale = before.scale;
            obj->transformDirty = true;
          },
          [&, selectedId, after]() {
            auto *obj = sceneSystem.findObject(selectedId);
            if (!obj) return;
            obj->transform.translation = after.translation;
            obj->transform.rotation = after.rotation;
            obj->transform.scale = after.scale;
            obj->transformDirty = true;
          }});
      }
      if (result.inspectorActions.nameChanged) {
        const std::string beforeName = result.inspectorActions.beforeName;
        const std::string afterName = result.inspectorActions.afterName;
        history.push({
          "Rename",
          [&, selectedId, beforeName]() {
            auto *obj = sceneSystem.findObject(selectedId);
            if (!obj) return;
            obj->name = beforeName;
          },
          [&, selectedId, afterName]() {
            auto *obj = sceneSystem.findObject(selectedId);
            if (!obj) return;
            obj->name = afterName;
          }});
      }
      if (result.inspectorActions.nodeOverridesChanged &&
          result.inspectorActions.nodeOverridesCommitted) {
        const auto before = result.inspectorActions.beforeNodeOverrides;
        const auto after = result.inspectorActions.afterNodeOverrides;
        history.push({
          "Node Override",
          [&, selectedId, before]() {
            auto *obj = sceneSystem.findObject(selectedId);
            if (!obj || !obj->model) return;
            sceneSystem.ensureNodeOverrides(*obj);
            auto &target = obj->nodeOverrides;
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
            auto *obj = sceneSystem.findObject(selectedId);
            if (!obj || !obj->model) return;
            sceneSystem.ensureNodeOverrides(*obj);
            auto &target = obj->nodeOverrides;
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

    return historyTriggered;
  }

  void EditorSystem::applyResourceActions(
    EditorFrameResult &result,
    SceneSystem &sceneSystem,
    SpriteAnimator *&animator,
    editor::ResourceBrowserState &resourceBrowserState) {
    if (result.resourceActions.setActiveSpriteMeta) {
      if (sceneSystem.setActiveSpriteMetadata(resourceBrowserState.activeSpriteMetaPath)) {
        animator = sceneSystem.getSpriteAnimator();
      }
    }

    if (result.resourceActions.setActiveMesh &&
        !resourceBrowserState.activeMeshPath.empty()) {
      sceneSystem.setActiveMeshPath(resourceBrowserState.activeMeshPath);
    }

    if (result.resourceActions.setActiveMaterial &&
        !resourceBrowserState.activeMaterialPath.empty()) {
      sceneSystem.setActiveMaterialPath(resourceBrowserState.activeMaterialPath);
      sceneSystem.loadMaterialCached(resourceBrowserState.activeMaterialPath);
    }

    if (result.resourceActions.applySpriteMetaToSelection &&
        result.selectedObject &&
        result.selectedObject->isSprite) {
      if (sceneSystem.setActiveSpriteMetadata(resourceBrowserState.activeSpriteMetaPath)) {
        result.selectedObject->spriteMetaPath = resourceBrowserState.activeSpriteMetaPath;
        if (animator) {
          const std::string &stateName = result.selectedObject->spriteStateName;
          if (!stateName.empty()) {
            animator->applySpriteState(*result.selectedObject, stateName);
          } else {
            animator->applySpriteState(*result.selectedObject, result.selectedObject->objState);
          }
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
  }

  void EditorSystem::applyInspectorActions(
    EditorFrameResult &result,
    SceneSystem &sceneSystem,
    SpriteAnimator *&animator,
    editor::ResourceBrowserState &resourceBrowserState) {
    if (result.inspectorActions.cameraActiveChanged &&
        result.selectedObject &&
        result.selectedObject->camera) {
      sceneSystem.setActiveCamera(
        result.selectedObject->getId(),
        result.inspectorActions.cameraActive);
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
      if (!saveMaterialToFile(path, result.inspectorActions.materialData, &error)) {
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
    (void)animator;
  }

  void EditorSystem::handlePicking(
    EditorFrameResult &result,
    const std::vector<LveGameObject*> &objects,
    const glm::mat4 &view,
    const glm::mat4 &projection) {
    if (result.sceneView.leftMouseClicked && result.sceneView.allowPick) {
      const editor::tools::Ray ray = editor::tools::BuildPickRay(result.sceneView, view, projection);
      if (ray.valid) {
        float bestT = std::numeric_limits<float>::max();
        std::optional<LveGameObject::id_t> hitId{};
        std::optional<int> hitNodeIndex{};
        const glm::mat4 invView = glm::inverse(view);
        const glm::vec3 camRight = glm::vec3(invView[0]);
        const glm::vec3 camUp = glm::vec3(invView[1]);

        for (auto *obj : objects) {
          if (!obj) continue;
          if (!obj->model && !obj->pointLight && !obj->isSprite) continue;
          float tHitWorld = std::numeric_limits<float>::max();
          bool hit = false;
          if (obj->pointLight) {
            float tHit = 0.f;
            if (editor::tools::IntersectSphere(ray, obj->transform.translation, obj->transform.scale.x, tHit)) {
              tHitWorld = tHit;
              hit = true;
            }
          } else if (obj->isSprite) {
            float tHit = 0.f;
            glm::vec2 halfSize{
              std::abs(obj->transform.scale.x) * 0.5f,
              std::abs(obj->transform.scale.y) * 0.5f};
            if (editor::tools::IntersectBillboardQuad(
                  ray,
                  obj->transform.translation,
                  camRight,
                  camUp,
                  halfSize,
                  tHit)) {
              tHitWorld = tHit - 0.01f; // slight bias toward sprites
              hit = true;
            }
          } else if (obj->model) {
            const auto &nodes = obj->model->getNodes();
            const auto &subMeshes = obj->model->getSubMeshes();
            if (!nodes.empty() && !subMeshes.empty()) {
              std::vector<glm::mat4> localOverrides(nodes.size(), glm::mat4(1.f));
              if (obj->nodeOverrides.size() == nodes.size()) {
                for (std::size_t i = 0; i < nodes.size(); ++i) {
                  const auto &override = obj->nodeOverrides[i];
                  if (override.enabled) {
                    localOverrides[i] = override.transform.mat4();
                  }
                }
              }
              std::vector<glm::mat4> nodeGlobals;
              obj->model->computeNodeGlobals(localOverrides, nodeGlobals);

              const glm::mat4 objectTransform = obj->transform.mat4();
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
                  editor::tools::TransformAabb(nodeTransform, subMesh.boundsMin, subMesh.boundsMax, worldMin, worldMax);
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
                if (editor::tools::IntersectAabbLocal(ray.origin, ray.direction, nodeMin, nodeMax, tHit)) {
                  tHitWorld = tHit;
                  hit = true;
                  if (tHitWorld < bestT) {
                    bestT = tHitWorld;
                    hitId = obj->getId();
                    hitNodeIndex = static_cast<int>(nodeIndex);
                  }
                }
              }
            } else {
              const auto &bbox = obj->model->getBoundingBox();
              glm::mat4 modelMat = obj->transform.mat4();
              glm::mat4 invModel = glm::inverse(modelMat);
              glm::vec3 localOrigin = glm::vec3(invModel * glm::vec4(ray.origin, 1.f));
              glm::vec3 localDir = glm::normalize(glm::mat3(invModel) * ray.direction);
              float tLocal = 0.f;
              if (editor::tools::IntersectAabbLocal(localOrigin, localDir, bbox.min, bbox.max, tLocal)) {
                glm::vec3 hitLocal = localOrigin + localDir * tLocal;
                glm::vec3 hitWorld = glm::vec3(modelMat * glm::vec4(hitLocal, 1.f));
                tHitWorld = glm::length(hitWorld - ray.origin);
                hit = true;
              }
            }
          }

          if (hit && tHitWorld < bestT) {
            bestT = tHitWorld;
            hitId = obj->getId();
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
  }

  void EditorSystem::handleCreateDelete(
    EditorFrameResult &result,
    SceneSystem &sceneSystem,
    SpriteAnimator *&animator,
    const glm::mat4 &view,
    const glm::vec3 &cameraPos,
    editor::ResourceBrowserState &resourceBrowserState,
    LveGameObject::id_t protectedId,
    bool historyTriggered) {
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
          const editor::GameObjectSnapshot snapshot = editor::CaptureSnapshot(obj);
          history.push({
            "Create Sprite",
            [&, id = obj.getId()]() {
              sceneSystem.destroyObject(id);
            },
            [&, snapshot]() {
              editor::RestoreSnapshot(sceneSystem, animator, snapshot);
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
          if (editor::workflow::CreateMaterialInstance(
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
          const editor::GameObjectSnapshot snapshot = editor::CaptureSnapshot(obj);
          history.push({
            "Create Mesh",
            [&, id = obj.getId()]() {
              sceneSystem.destroyObject(id);
            },
            [&, snapshot]() {
              editor::RestoreSnapshot(sceneSystem, animator, snapshot);
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
          const editor::GameObjectSnapshot snapshot = editor::CaptureSnapshot(obj);
          history.push({
            "Create Light",
            [&, id = obj.getId()]() {
              sceneSystem.destroyObject(id);
            },
            [&, snapshot]() {
              editor::RestoreSnapshot(sceneSystem, animator, snapshot);
              setSelectedId(snapshot.id);
            }});
        }
        break;
      }
      case editor::HierarchyCreateRequest::Camera: {
        auto &obj = sceneSystem.createCameraObject(spawnPos);
        sceneSystem.setActiveCamera(obj.getId(), true);
        setSelectedId(obj.getId());
        obj.transformDirty = true;
        if (!historyTriggered) {
          const editor::GameObjectSnapshot snapshot = editor::CaptureSnapshot(obj);
          history.push({
            "Create Camera",
            [&, id = obj.getId()]() {
              sceneSystem.destroyObject(id);
            },
            [&, snapshot]() {
              editor::RestoreSnapshot(sceneSystem, animator, snapshot);
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
        editor::GameObjectSnapshot snapshot{};
        bool hasSnapshot = false;
        auto *obj = sceneSystem.findObject(*selectedId);
        if (obj) {
          snapshot = editor::CaptureSnapshot(*obj);
          hasSnapshot = true;
        }
        if (sceneSystem.destroyObject(*selectedId)) {
          setSelectedId(std::nullopt);
          if (!historyTriggered && hasSnapshot) {
            history.push({
              "Delete Object",
              [&, snapshot]() {
                editor::RestoreSnapshot(sceneSystem, animator, snapshot);
                setSelectedId(snapshot.id);
              },
              [&, id = *selectedId]() {
                sceneSystem.destroyObject(id);
                setSelectedId(std::nullopt);
              }});
          }
        }
      }
    }
  }

  void EditorSystem::handleSceneActions(
    EditorFrameResult &result,
    SceneSystem &sceneSystem,
    SpriteAnimator *&animator,
    LveGameObject::id_t viewerId) {
    if (result.sceneActions.saveRequested) {
      sceneSystem.saveSceneToFile(getScenePanelState().path);
    }
    if (result.sceneActions.loadRequested) {
      renderBackend.waitIdle();
      sceneSystem.loadSceneFromFile(getScenePanelState().path, viewerId);
      animator = sceneSystem.getSpriteAnimator();
      history.clear();
      setSelectedId(std::nullopt);
    }
  }

  void EditorSystem::render(backend::CommandBufferHandle commandBuffer) {
    renderBackend.render(commandBuffer);
  }

  void EditorSystem::renderPlatformWindows() {
    renderBackend.renderPlatformWindows();
  }

} // namespace lve
