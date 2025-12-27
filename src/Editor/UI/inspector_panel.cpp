#include "inspector_panel.hpp"

#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>
#include <cstdio>

namespace lve::editor {

  namespace {
    ObjectState stateFromName(const std::string &name) {
      if (name == "walking" || name == "walk") return ObjectState::WALKING;
      return ObjectState::IDLE;
    }

    std::string stateToName(const std::string &objStateName, ObjectState fallback) {
      if (!objStateName.empty()) return objStateName;
      return (fallback == ObjectState::WALKING) ? "walking" : "idle";
    }

    const char *typeLabel(const LveGameObject &obj) {
      if (obj.pointLight) return "Light";
      if (obj.isSprite) return "Sprite";
      if (obj.model) return "Mesh";
      return "Unknown";
    }

    bool nodeOverrideEquals(const NodeTransformOverride &a, const NodeTransformOverride &b) {
      return a.enabled == b.enabled &&
        a.transform.translation == b.transform.translation &&
        a.transform.rotation == b.transform.rotation &&
        a.transform.scale == b.transform.scale;
    }

    bool nodeOverridesEqual(
      const std::vector<NodeTransformOverride> &a,
      const std::vector<NodeTransformOverride> &b) {
      if (a.size() != b.size()) return false;
      for (std::size_t i = 0; i < a.size(); ++i) {
        if (!nodeOverrideEquals(a[i], b[i])) return false;
      }
      return true;
    }
  } // namespace

  InspectorActions BuildInspectorPanel(
    LveGameObject *selected,
    SpriteAnimator *animator,
    const glm::mat4 &view,
    const glm::mat4 &projection,
    VkExtent2D viewportExtent,
    bool *open,
    const GizmoContext &gizmoContext,
    int gizmoOperation,
    int gizmoMode,
    int &selectedNodeIndex) {
    InspectorActions actions{};
    if (open) {
      if (!ImGui::Begin("Inspector", open)) {
        ImGui::End();
        return actions;
      }
    } else {
      ImGui::Begin("Inspector");
    }
    if (!selected) {
      ImGui::Text("No selection");
      ImGui::End();
      return actions;
    }

    TransformSnapshot beforeTransform{
      selected->transform.translation,
      selected->transform.rotation,
      selected->transform.scale};
    std::string beforeName = selected->name;

    static LveGameObject::id_t lastSelectedId = 0;
    static bool transformEditing = false;
    static TransformSnapshot transformEditStart{};
    static bool nameEditing = false;
    static std::string nameEditStart{};
    static bool nodeOverrideEditing = false;
    static std::vector<NodeTransformOverride> nodeOverrideEditStart{};
    static const LveModel *lastSelectedModel = nullptr;

    if (lastSelectedId != selected->getId()) {
      transformEditing = false;
      nameEditing = false;
      nodeOverrideEditing = false;
      nodeOverrideEditStart.clear();
      lastSelectedId = selected->getId();
      lastSelectedModel = selected->model.get();
      selectedNodeIndex = -1;
    } else if (lastSelectedModel != selected->model.get()) {
      nodeOverrideEditing = false;
      nodeOverrideEditStart.clear();
      lastSelectedModel = selected->model.get();
      selectedNodeIndex = -1;
    }

    ImGui::Text("ID: %u", selected->getId());
    char nameBuf[128];
    std::snprintf(nameBuf, sizeof(nameBuf), "%s", selected->name.c_str());
    if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
      selected->name = nameBuf;
    }
    if (ImGui::IsItemActivated()) {
      nameEditing = true;
      nameEditStart = beforeName;
    }
    const bool nameCommitted = ImGui::IsItemDeactivatedAfterEdit();
    if (nameCommitted && nameEditing) {
      nameEditing = false;
      if (selected->name != nameEditStart) {
        actions.nameChanged = true;
        actions.beforeName = nameEditStart;
        actions.afterName = selected->name;
      }
    }
    ImGui::Text("Type: %s", typeLabel(*selected));
    ImGui::Separator();

    // Transform
    ImGui::Text("Transform");
    bool transformEdited = false;
    bool transformCommitted = false;
    bool nodeOverrideCommitted = false;
    const bool posChanged = ImGui::DragFloat3("Position", &selected->transform.translation.x, 0.05f);
    if (posChanged) transformEdited = true;
    if (ImGui::IsItemActivated() && !transformEditing) {
      transformEditing = true;
      transformEditStart = beforeTransform;
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
      transformCommitted = true;
    }

    const bool rotChanged = ImGui::DragFloat3("Rotation (rad)", &selected->transform.rotation.x, 0.05f);
    if (rotChanged) transformEdited = true;
    if (ImGui::IsItemActivated() && !transformEditing) {
      transformEditing = true;
      transformEditStart = beforeTransform;
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
      transformCommitted = true;
    }

    const bool scaleChanged = ImGui::DragFloat3("Scale", &selected->transform.scale.x, 0.05f, 0.001f, 100.f);
    if (scaleChanged) transformEdited = true;
    if (ImGui::IsItemActivated() && !transformEditing) {
      transformEditing = true;
      transformEditStart = beforeTransform;
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
      transformCommitted = true;
    }

    ImGuizmo::OPERATION op = static_cast<ImGuizmo::OPERATION>(gizmoOperation);
    ImGuizmo::MODE mode = static_cast<ImGuizmo::MODE>(gizmoMode);

    if (gizmoContext.valid && gizmoContext.drawList) {
      ImGuizmo::BeginFrame();
      ImGuizmo::SetDrawlist(static_cast<ImDrawList *>(gizmoContext.drawList));
      ImGuizmo::SetRect(gizmoContext.x, gizmoContext.y, gizmoContext.width, gizmoContext.height);
      ImGuizmo::SetOrthographic(false);
    }

    const glm::mat4 objectTransform = selected->transform.mat4();
    const bool hasNodes = selected->model && !selected->model->getNodes().empty();
    if (hasNodes) {
      const auto &nodes = selected->model->getNodes();
      if (selected->nodeOverrides.size() != nodes.size()) {
        selected->nodeOverrides.clear();
        selected->nodeOverrides.resize(nodes.size());
      }
    }
    std::vector<NodeTransformOverride> nodeOverridesBeforeFrame{};
    if (hasNodes) {
      nodeOverridesBeforeFrame = selected->nodeOverrides;
    }

    auto beginNodeOverrideEdit = [&]() {
      if (!nodeOverrideEditing) {
        nodeOverrideEditing = true;
        nodeOverrideEditStart = nodeOverridesBeforeFrame;
      }
    };
    const bool editNode = hasNodes && selectedNodeIndex >= 0;
    glm::mat4 model = objectTransform;
    std::vector<glm::mat4> nodeGlobals;
    int activeNodeIndex = -1;
    if (editNode) {
      const auto &nodes = selected->model->getNodes();
      if (selectedNodeIndex >= static_cast<int>(nodes.size())) {
        selectedNodeIndex = -1;
      } else {
        std::vector<glm::mat4> localOverrides(nodes.size(), glm::mat4(1.f));
        if (selected->nodeOverrides.size() == nodes.size()) {
          for (std::size_t i = 0; i < nodes.size(); ++i) {
            const auto &override = selected->nodeOverrides[i];
            if (override.enabled) {
              localOverrides[i] = override.transform.mat4();
            }
          }
        }
        selected->model->computeNodeGlobals(localOverrides, nodeGlobals);
        activeNodeIndex = selectedNodeIndex;
        if (activeNodeIndex >= 0 && static_cast<std::size_t>(activeNodeIndex) < nodeGlobals.size()) {
          model = objectTransform * nodeGlobals[static_cast<std::size_t>(activeNodeIndex)];
        }
      }
    }
    // Convert Vulkan projection to ImGuizmo-friendly OpenGL clip (-1..1 depth) and flip Y
    glm::mat4 gizmoProj = projection;
    gizmoProj[1][1] *= -1.f; // flip Y
    // bias depth from [0,1] to [-1,1]
    glm::mat4 bias{1.f};
    bias[2][2] = 0.5f;
    bias[3][2] = 0.5f;
    gizmoProj = bias * gizmoProj;

    float modelArr[16];
    std::memcpy(modelArr, glm::value_ptr(model), sizeof(modelArr));

    static bool gizmoWasUsing = false;
    static bool gizmoWasEditingNode = false;
    bool gizmoUsing = false;
    if (gizmoContext.valid && gizmoContext.drawList) {
      ImGuizmo::Manipulate(
        glm::value_ptr(view),
        glm::value_ptr(gizmoProj),
        op,
        mode,
        modelArr,
        nullptr,
        nullptr);

      gizmoUsing = ImGuizmo::IsUsing();
      if (gizmoUsing) {
        float trans[3], rotDeg[3], scale[3];
        if (editNode && activeNodeIndex >= 0 && selected->model) {
          beginNodeOverrideEdit();
          glm::mat4 newWorld = glm::make_mat4(modelArr);
          const auto &nodes = selected->model->getNodes();
          const int parentIndex = nodes[static_cast<std::size_t>(activeNodeIndex)].parent;
          glm::mat4 parentEff{1.f};
          if (parentIndex >= 0 && static_cast<std::size_t>(parentIndex) < nodeGlobals.size()) {
            parentEff = nodeGlobals[static_cast<std::size_t>(parentIndex)];
          }
          const glm::mat4 localBase = nodes[static_cast<std::size_t>(activeNodeIndex)].localTransform;
          glm::mat4 localOverride = glm::inverse(localBase) * glm::inverse(parentEff) * glm::inverse(objectTransform) * newWorld;

          float overrideArr[16];
          std::memcpy(overrideArr, glm::value_ptr(localOverride), sizeof(overrideArr));
          ImGuizmo::DecomposeMatrixToComponents(overrideArr, trans, rotDeg, scale);

          if (selected->nodeOverrides.size() == nodes.size()) {
            auto &override = selected->nodeOverrides[static_cast<std::size_t>(activeNodeIndex)];
            override.enabled = true;
            override.transform.translation = glm::vec3{trans[0], trans[1], trans[2]};
            override.transform.rotation = glm::radians(glm::vec3{rotDeg[0], rotDeg[1], rotDeg[2]});
            override.transform.scale = glm::vec3{scale[0], scale[1], scale[2]};
          }
        } else {
          ImGuizmo::DecomposeMatrixToComponents(modelArr, trans, rotDeg, scale);
          selected->transform.translation = glm::vec3{trans[0], trans[1], trans[2]};
          selected->transform.rotation = glm::radians(glm::vec3{rotDeg[0], rotDeg[1], rotDeg[2]});
          selected->transform.scale = glm::vec3{scale[0], scale[1], scale[2]};
          transformEdited = true;
          if (!transformEditing) {
            transformEditing = true;
            transformEditStart = beforeTransform;
          }
        }
        gizmoWasEditingNode = editNode && activeNodeIndex >= 0 && selected->model;
      }
    }

    if (gizmoWasUsing && !gizmoUsing) {
      if (gizmoWasEditingNode) {
        nodeOverrideCommitted = true;
      } else {
        transformCommitted = true;
      }
      gizmoWasEditingNode = false;
    }
    gizmoWasUsing = gizmoUsing;
    if (transformEdited) {
      selected->transformDirty = true;
    }
    if (transformCommitted && transformEditing) {
      actions.transformChanged = true;
      actions.transformCommitted = true;
      actions.beforeTransform = transformEditStart;
      actions.afterTransform = {
        selected->transform.translation,
        selected->transform.rotation,
        selected->transform.scale};
      transformEditing = false;
    }
    if (nodeOverrideCommitted && nodeOverrideEditing) {
      if (!nodeOverridesEqual(nodeOverrideEditStart, selected->nodeOverrides)) {
        actions.nodeOverridesChanged = true;
        actions.nodeOverridesCommitted = true;
        actions.beforeNodeOverrides = nodeOverrideEditStart;
        actions.afterNodeOverrides = selected->nodeOverrides;
      }
      nodeOverrideEditing = false;
    }

    if (selected->pointLight) {
      ImGui::Separator();
      ImGui::Text("Light");
      ImGui::ColorEdit3("Color", &selected->color.x);
      ImGui::DragFloat("Intensity", &selected->pointLight->lightIntensity, 0.1f, 0.0f, 100.f);
    }

    if (selected->isSprite && animator) {
      ImGui::Separator();
      ImGui::Text("Sprite");
      auto &meta = animator->getMetadata();

      // collect state names
      std::vector<std::string> stateNames;
      stateNames.reserve(meta.states.size());
      for (const auto &kv : meta.states) {
        stateNames.push_back(kv.first);
      }
      std::sort(stateNames.begin(), stateNames.end());

      std::string currentName = stateToName(selected->spriteStateName, selected->objState);
      int currentIndex = 0;
      for (int i = 0; i < static_cast<int>(stateNames.size()); ++i) {
        if (stateNames[i] == currentName) {
          currentIndex = i;
          break;
        }
      }

      if (!stateNames.empty()) {
        std::vector<const char*> labels;
        labels.reserve(stateNames.size());
        for (auto &s : stateNames) labels.push_back(s.c_str());
        if (ImGui::Combo("State", &currentIndex, labels.data(), static_cast<int>(labels.size()))) {
          const std::string &chosen = stateNames[currentIndex];
          selected->objState = stateFromName(chosen);
          selected->spriteStateName = chosen;
          animator->applySpriteState(*selected, selected->objState);
        }
      }

      int mode = static_cast<int>(selected->billboardMode);
      const char* modeLabels[] = { "None", "Cylindrical", "Spherical" };
      if (ImGui::Combo("Billboard", &mode, modeLabels, IM_ARRAYSIZE(modeLabels))) {
        selected->billboardMode = static_cast<BillboardMode>(mode);
      }
    }

    if (selected->model && !selected->isSprite && !selected->pointLight) {
      ImGui::Separator();
      ImGui::Text("Mesh Nodes");
      const auto &nodes = selected->model->getNodes();
      if (selected->nodeOverrides.size() != nodes.size()) {
        selected->nodeOverrides.clear();
        selected->nodeOverrides.resize(nodes.size());
      }

      if (nodes.empty()) {
        ImGui::Text("No nodes in model");
      } else {
        std::vector<std::string> nodeLabels;
        nodeLabels.reserve(nodes.size() + 1);
        nodeLabels.emplace_back("Object");
        for (std::size_t i = 0; i < nodes.size(); ++i) {
          const auto &node = nodes[i];
          std::string label = node.name.empty()
            ? ("Node " + std::to_string(i))
            : node.name;
          label += " [#" + std::to_string(i) + "]";
          nodeLabels.push_back(std::move(label));
        }
        std::vector<const char*> nodeLabelPtrs;
        nodeLabelPtrs.reserve(nodeLabels.size());
        for (const auto &label : nodeLabels) {
          nodeLabelPtrs.push_back(label.c_str());
        }

        int nodeSelection = selectedNodeIndex + 1;
        if (nodeSelection < 0 || nodeSelection >= static_cast<int>(nodeLabelPtrs.size())) {
          nodeSelection = 0;
        }
        if (ImGui::Combo("Node", &nodeSelection, nodeLabelPtrs.data(), static_cast<int>(nodeLabelPtrs.size()))) {
          selectedNodeIndex = nodeSelection - 1;
        }

        if (selectedNodeIndex >= 0) {
          auto &override = selected->nodeOverrides[static_cast<std::size_t>(selectedNodeIndex)];
          const bool enabledChanged = ImGui::Checkbox("Override Enabled", &override.enabled);
          if (ImGui::IsItemActivated()) {
            beginNodeOverrideEdit();
          }
          if (ImGui::IsItemDeactivatedAfterEdit()) {
            nodeOverrideCommitted = true;
          }
          if (enabledChanged) {
            beginNodeOverrideEdit();
          }
          bool overrideChanged = false;
          overrideChanged |= ImGui::DragFloat3("Offset Position", &override.transform.translation.x, 0.05f);
          if (ImGui::IsItemActivated()) {
            beginNodeOverrideEdit();
          }
          if (ImGui::IsItemDeactivatedAfterEdit()) {
            nodeOverrideCommitted = true;
          }
          overrideChanged |= ImGui::DragFloat3("Offset Rotation (rad)", &override.transform.rotation.x, 0.05f);
          if (ImGui::IsItemActivated()) {
            beginNodeOverrideEdit();
          }
          if (ImGui::IsItemDeactivatedAfterEdit()) {
            nodeOverrideCommitted = true;
          }
          overrideChanged |= ImGui::DragFloat3("Offset Scale", &override.transform.scale.x, 0.05f, 0.001f, 100.f);
          if (ImGui::IsItemActivated()) {
            beginNodeOverrideEdit();
          }
          if (ImGui::IsItemDeactivatedAfterEdit()) {
            nodeOverrideCommitted = true;
          }
          if (overrideChanged) {
            override.enabled = true;
            beginNodeOverrideEdit();
          }
          if (ImGui::Button("Reset Override")) {
            beginNodeOverrideEdit();
            override.enabled = false;
            override.transform = TransformComponent{};
            nodeOverrideCommitted = true;
          }
        }
      }
    }

    ImGui::End();
    return actions;
  }

} // namespace lve::editor
