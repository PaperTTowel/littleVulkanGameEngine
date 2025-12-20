#include "hierarchy_panel.hpp"

#include <imgui.h>

#include <algorithm>
#include <string>
#include <vector>

namespace lve::editor {

  namespace {
    std::string makeLabel(const LveGameObject &obj) {
      const char *type = "Mesh";
      if (obj.pointLight) {
        type = "PointLight";
      } else if (obj.isSprite) {
        type = "Sprite";
      }
      std::string displayName = obj.name.empty()
        ? ("ID " + std::to_string(obj.getId()))
        : obj.name;
      return displayName + " (" + type + ") [ID " + std::to_string(obj.getId()) + "]";
    }
  } // namespace

  HierarchyActions BuildHierarchyPanel(
    LveGameObjectManager &manager,
    HierarchyPanelState &state,
    LveGameObject::id_t protectedId) {

    HierarchyActions actions{};
    ImGui::Begin("Hierarchy");

    // ensure selected id is still valid
    if (state.selectedId.has_value()) {
      if (!manager.gameObjects.count(*state.selectedId)) {
        state.selectedId.reset();
      }
    }

    std::vector<std::pair<LveGameObject::id_t, LveGameObject*>> sortedObjects;
    sortedObjects.reserve(manager.gameObjects.size());
    for (auto &kv : manager.gameObjects) {
      sortedObjects.emplace_back(kv.first, &kv.second);
    }
    std::sort(
      sortedObjects.begin(),
      sortedObjects.end(),
      [](auto &a, auto &b) { return a.first < b.first; });

    for (auto &entry : sortedObjects) {
      auto *obj = entry.second;
      const bool isSelected = state.selectedId && *state.selectedId == obj->getId();
      const std::string label = makeLabel(*obj);
      if (ImGui::Selectable(label.c_str(), isSelected)) {
        state.selectedId = obj->getId();
      }
    }

    ImGui::Separator();
    if (ImGui::Button("Add Sprite")) {
      actions.createRequest = HierarchyCreateRequest::Sprite;
    }
    ImGui::SameLine();
    if (ImGui::Button("Add Mesh")) {
      actions.createRequest = HierarchyCreateRequest::Mesh;
    }
    ImGui::SameLine();
    if (ImGui::Button("Add Point Light")) {
      actions.createRequest = HierarchyCreateRequest::PointLight;
    }

    const bool canDelete =
      state.selectedId.has_value() &&
      manager.gameObjects.count(*state.selectedId) &&
      *state.selectedId != protectedId;

    ImGui::BeginDisabled(!canDelete);
    if (ImGui::Button("Delete Selected")) {
      actions.deleteSelected = true;
    }
    ImGui::EndDisabled();

    ImGui::End();
    return actions;
  }

} // namespace lve::editor
