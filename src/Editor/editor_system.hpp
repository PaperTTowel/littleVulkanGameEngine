#pragma once

#include "ImGui/imgui_layer.hpp"
#include "Editor/hierarchy_panel.hpp"
#include "Editor/scene_panel.hpp"
#include "Editor/resource_browser_panel.hpp"
#include "Editor/inspector_panel.hpp"

// std
#include <optional>

namespace lve {

  struct EditorFrameResult {
    editor::HierarchyActions hierarchyActions{};
    editor::ScenePanelActions sceneActions{};
    editor::ResourceBrowserActions resourceActions{};
    LveGameObject *selectedObject{nullptr};
  };

  class EditorSystem {
  public:
    EditorSystem(LveWindow &window, LveDevice &device);

    void init(VkRenderPass renderPass);
    void onRenderPassChanged(VkRenderPass renderPass);
    void shutdown();

    EditorFrameResult buildFrame(
      float frameTime,
      const glm::vec3 &cameraPos,
      const glm::vec3 &cameraRot,
      bool &wireframeEnabled,
      bool &normalViewEnabled,
      bool &useOrthoCamera,
      LveGameObjectManager &gameObjectManager,
      LveGameObject::id_t protectedId,
      SpriteAnimator *animator,
      const glm::mat4 &view,
      const glm::mat4 &projection,
      VkExtent2D viewportExtent,
      editor::ResourceBrowserState &resourceBrowserState);

    void render(VkCommandBuffer commandBuffer);

    std::optional<LveGameObject::id_t> getSelectedId() const { return hierarchyState.selectedId; }
    void setSelectedId(std::optional<LveGameObject::id_t> id) { hierarchyState.selectedId = id; }
    const editor::ScenePanelState &getScenePanelState() const { return scenePanelState; }

  private:
    ImGuiLayer imgui;
    editor::HierarchyPanelState hierarchyState;
    editor::ScenePanelState scenePanelState;
  };

} // namespace lve
