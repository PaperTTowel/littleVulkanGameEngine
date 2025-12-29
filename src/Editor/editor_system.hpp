#pragma once

#include "ImGui/imgui_layer.hpp"
#include "Editor/UI/hierarchy_panel.hpp"
#include "Editor/UI/scene_panel.hpp"
#include "Editor/Workflow/resource_browser_panel.hpp"
#include "Editor/UI/inspector_panel.hpp"
#include "Editor/History/editor_history.hpp"

// std
#include <optional>

namespace lve {
  class LveDevice;
  class SceneSystem;

  struct ViewportInfo {
    float x{0.f};
    float y{0.f};
    uint32_t width{0};
    uint32_t height{0};
    bool visible{false};
    bool hovered{false};
    bool rightMouseDown{false};
    bool leftMouseClicked{false};
    bool allowPick{false};
    float mouseDeltaX{0.f};
    float mouseDeltaY{0.f};
    float mousePosX{0.f};
    float mousePosY{0.f};
  };

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
    EditorSystem(LveWindow &window, LveDevice &device);

    void init(VkRenderPass renderPass, uint32_t imageCount);
    void onRenderPassChanged(VkRenderPass renderPass, uint32_t imageCount);
    void shutdown();

    EditorFrameResult update(
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
      void *gameViewTextureId);

    void render(VkCommandBuffer commandBuffer);
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

    ImGuiLayer imgui;
    LveDevice &device;
    editor::HierarchyPanelState hierarchyState;
    editor::ScenePanelState scenePanelState;
    editor::EditorHistory history;
    int gizmoOperation{0};
    int gizmoMode{0};
    bool showEngineStats{true};
    bool showHierarchy{true};
    bool showScene{true};
    bool showInspector{true};
    bool showResourceBrowser{true};
    bool showFileDialog{false};
    FileDialogPurpose fileDialogPurpose{FileDialogPurpose::Import};
    ImportOptionsState importOptions;
    bool showSceneView{true};
    bool showGameView{true};
    editor::FileDialogState fileDialogState;
    editor::MaterialPickResult pendingMaterialPick{};
    editor::MaterialTextureSlot pendingMaterialPickSlot{editor::MaterialTextureSlot::BaseColor};
  };

} // namespace lve
