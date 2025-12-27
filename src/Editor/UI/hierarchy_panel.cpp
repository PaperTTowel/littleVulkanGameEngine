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

    std::string makeNodeLabel(const LveModel::Node &node, std::size_t index) {
      std::string label = node.name.empty()
        ? ("Node " + std::to_string(index))
        : node.name;
      label += " [#" + std::to_string(index) + "]";
      return label;
    }

    void drawNodeTree(
      const std::vector<LveModel::Node> &nodes,
      std::size_t nodeIndex,
      HierarchyPanelState &state,
      LveGameObject::id_t objectId) {
      const auto &node = nodes[nodeIndex];
      const bool nodeSelected =
        state.selectedId &&
        *state.selectedId == objectId &&
        state.selectedNodeIndex == static_cast<int>(nodeIndex);

      ImGuiTreeNodeFlags flags =
        ImGuiTreeNodeFlags_OpenOnArrow |
        ImGuiTreeNodeFlags_SpanFullWidth;
      if (node.children.empty()) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
      }
      if (nodeSelected) {
        flags |= ImGuiTreeNodeFlags_Selected;
      }

      const std::string nodeLabel = makeNodeLabel(node, nodeIndex);
      const bool open = ImGui::TreeNodeEx(nodeLabel.c_str(), flags);
      if (ImGui::IsItemClicked()) {
        state.selectedId = objectId;
        state.selectedNodeIndex = static_cast<int>(nodeIndex);
      }

      if (open && !node.children.empty()) {
        for (int childIndex : node.children) {
          if (childIndex < 0 || static_cast<std::size_t>(childIndex) >= nodes.size()) {
            continue;
          }
          drawNodeTree(nodes, static_cast<std::size_t>(childIndex), state, objectId);
        }
        ImGui::TreePop();
      }
    }
  } // namespace

  HierarchyActions BuildHierarchyPanel(
    LveGameObjectManager &manager,
    HierarchyPanelState &state,
    LveGameObject::id_t protectedId,
    bool *open) {

    HierarchyActions actions{};
    if (open) {
      if (!ImGui::Begin("Hierarchy", open)) {
        ImGui::End();
        return actions;
      }
    } else {
      ImGui::Begin("Hierarchy");
    }

    // ensure selected id is still valid
    if (state.selectedId.has_value()) {
      auto itSel = manager.gameObjects.find(*state.selectedId);
      if (itSel == manager.gameObjects.end()) {
        state.selectedId.reset();
        state.selectedNodeIndex = -1;
      } else {
        const auto &obj = itSel->second;
        if (!obj.model || obj.model->getNodes().empty()) {
          state.selectedNodeIndex = -1;
        } else if (state.selectedNodeIndex >= static_cast<int>(obj.model->getNodes().size())) {
          state.selectedNodeIndex = -1;
        }
      }
    } else {
      state.selectedNodeIndex = -1;
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
      const std::string label = makeLabel(*obj);
      const bool hasNodes = obj->model && !obj->model->getNodes().empty();
      const bool objectSelected =
        state.selectedId &&
        *state.selectedId == obj->getId() &&
        state.selectedNodeIndex < 0;

      ImGuiTreeNodeFlags flags =
        ImGuiTreeNodeFlags_OpenOnArrow |
        ImGuiTreeNodeFlags_SpanFullWidth;
      if (!hasNodes) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
      }
      if (objectSelected) {
        flags |= ImGuiTreeNodeFlags_Selected;
      }

      const bool open = ImGui::TreeNodeEx(label.c_str(), flags);
      if (ImGui::IsItemClicked()) {
        state.selectedId = obj->getId();
        state.selectedNodeIndex = -1;
      }

      if (hasNodes && open) {
        const auto &nodes = obj->model->getNodes();
        std::vector<std::size_t> rootIndices;
        rootIndices.reserve(nodes.size());
        for (std::size_t i = 0; i < nodes.size(); ++i) {
          const int parent = nodes[i].parent;
          if (parent < 0 || static_cast<std::size_t>(parent) >= nodes.size()) {
            rootIndices.push_back(i);
          }
        }
        if (rootIndices.empty()) {
          for (std::size_t i = 0; i < nodes.size(); ++i) {
            rootIndices.push_back(i);
          }
        }

        ImGui::PushID(static_cast<int>(obj->getId()));
        for (std::size_t rootIndex : rootIndices) {
          drawNodeTree(nodes, rootIndex, state, obj->getId());
        }
        ImGui::PopID();
        ImGui::TreePop();
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
