#include "editor_system.hpp"

namespace lve {

  EditorSystem::EditorSystem(LveWindow &window, LveDevice &device)
    : imgui{window, device} {}

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

  EditorFrameResult EditorSystem::buildFrame(
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
    editor::ResourceBrowserState &resourceBrowserState) {

    EditorFrameResult result{};

    imgui.newFrame();
    imgui.buildUI(
      frameTime,
      cameraPos,
      cameraRot,
      wireframeEnabled,
      normalViewEnabled,
      useOrthoCamera);

    result.hierarchyActions = editor::BuildHierarchyPanel(gameObjectManager, hierarchyState, protectedId);
    result.sceneActions = editor::BuildScenePanel(scenePanelState);

    if (hierarchyState.selectedId) {
      auto itSel = gameObjectManager.gameObjects.find(*hierarchyState.selectedId);
      if (itSel != gameObjectManager.gameObjects.end()) {
        result.selectedObject = &itSel->second;
      }
    }

    editor::BuildInspectorPanel(
      result.selectedObject,
      animator,
      view,
      projection,
      viewportExtent);

    result.resourceActions = editor::BuildResourceBrowserPanel(resourceBrowserState, result.selectedObject);

    return result;
  }

  void EditorSystem::render(VkCommandBuffer commandBuffer) {
    imgui.render(commandBuffer);
  }

} // namespace lve
