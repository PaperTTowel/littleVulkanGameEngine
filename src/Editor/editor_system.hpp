#pragma once

#include "Engine/Backend/editor_render_backend.hpp"
#include "Editor/UI/hierarchy_panel.hpp"
#include "Editor/UI/scene_panel.hpp"
#include "Editor/Workflow/resource_browser_panel.hpp"
#include "Editor/UI/inspector_panel.hpp"
#include "Editor/History/editor_history.hpp"
#include "Editor/viewport_info.hpp"

// std
#include <cstdint>
#include <optional>
#include <vector>

namespace lve {
  class SceneSystem;

  struct EditorFrameResult {
    editor::HierarchyActions hierarchyActions{};
    editor::ScenePanelActions sceneActions{};
    editor::ResourceBrowserActions resourceActions{};
    editor::FileDialogActions fileDialogActions{};
    editor::InspectorActions inspectorActions{};
    LveGameObject *selectedObject{nullptr};
    bool undoRequested{false};
    bool redoRequested{false};
    ViewportInfo sceneView{};
    ViewportInfo gameView{};
  };

  class EditorSystem {
  public:
    explicit EditorSystem(backend::EditorRenderBackend &renderBackend);

    void init(backend::RenderPassHandle renderPass, std::uint32_t imageCount);
    void onRenderPassChanged(backend::RenderPassHandle renderPass, std::uint32_t imageCount);
    void shutdown();

    EditorFrameResult update(
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
      void *gameViewTextureId);

    void render(backend::CommandBufferHandle commandBuffer);
    void renderPlatformWindows();

    std::optional<LveGameObject::id_t> getSelectedId() const { return hierarchyState.selectedId; }
    int getSelectedNodeIndex() const { return hierarchyState.selectedNodeIndex; }
    void setSelectedId(std::optional<LveGameObject::id_t> id) {
      hierarchyState.selectedId = id;
      hierarchyState.selectedNodeIndex = -1;
    }
    void setSelectedNodeIndex(int index) { hierarchyState.selectedNodeIndex = index; }
    const editor::ScenePanelState &getScenePanelState() const { return scenePanelState; }
    editor::EditorHistory &getHistory() { return history; }
    const editor::EditorHistory &getHistory() const { return history; }

  private:
    enum class FileDialogPurpose {
      Import,
      MaterialTexture
    };

    struct ImportOptionsState {
      bool show{false};
      bool openRequested{false};
      int mode{0};
      std::string pendingPath{};
      std::string error{};
    };

    backend::EditorRenderBackend &renderBackend;
    editor::HierarchyPanelState hierarchyState;
    editor::ScenePanelState scenePanelState;
    editor::InspectorState inspectorState;
    editor::EditorHistory history;
    int gizmoOperation{0};
    int gizmoMode{0};
    bool showEngineStats{true};
    bool showHierarchy{true};
    bool showScene{true};
    bool showInspector{true};
    bool showResourceBrowser{true};
    bool showFileDialog{false};
    bool showGameViewCameraWarning{true};
    FileDialogPurpose fileDialogPurpose{FileDialogPurpose::Import};
    ImportOptionsState importOptions;
    bool showSceneView{true};
    bool showGameView{true};
    editor::FileDialogState fileDialogState;
    editor::MaterialPickResult pendingMaterialPick{};
    editor::MaterialTextureSlot pendingMaterialPickSlot{editor::MaterialTextureSlot::BaseColor};

    void buildFrameUI(
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
      void *gameViewTextureId);
    bool applyHistoryActions(
      EditorFrameResult &result,
      SceneSystem &sceneSystem);
    void applyResourceActions(
      EditorFrameResult &result,
      SceneSystem &sceneSystem,
      SpriteAnimator *&animator,
      editor::ResourceBrowserState &resourceBrowserState);
    void applyInspectorActions(
      EditorFrameResult &result,
      SceneSystem &sceneSystem,
      SpriteAnimator *&animator,
      editor::ResourceBrowserState &resourceBrowserState);
    void handlePicking(
      EditorFrameResult &result,
      const std::vector<LveGameObject*> &objects,
      const glm::mat4 &view,
      const glm::mat4 &projection);
    void handleCreateDelete(
      EditorFrameResult &result,
      SceneSystem &sceneSystem,
      SpriteAnimator *&animator,
      const glm::mat4 &view,
      const glm::vec3 &cameraPos,
      editor::ResourceBrowserState &resourceBrowserState,
      LveGameObject::id_t protectedId,
      bool historyTriggered);
    void handleSceneActions(
      EditorFrameResult &result,
      SceneSystem &sceneSystem,
      SpriteAnimator *&animator,
      LveGameObject::id_t viewerId);
  };

} // namespace lve
