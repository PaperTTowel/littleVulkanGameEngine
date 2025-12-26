#include "inspector_panel.hpp"

#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
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
    int gizmoMode) {
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

    if (lastSelectedId != selected->getId()) {
      transformEditing = false;
      nameEditing = false;
      lastSelectedId = selected->getId();
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

    glm::mat4 model = selected->transform.mat4();
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
    }

    if (gizmoWasUsing && !gizmoUsing) {
      transformCommitted = true;
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

    ImGui::End();
    return actions;
  }

} // namespace lve::editor
