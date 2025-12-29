#include "inspector_panel.hpp"

#include <imgui.h>
#include <ImGuizmo.h>
#include <backends/imgui_impl_vulkan.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <deque>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <cstdio>

namespace lve::editor {

  namespace {
    void retirePreviewTexture(std::shared_ptr<LveTexture> texture) {
      static std::deque<std::shared_ptr<LveTexture>> retired;
      if (texture) {
        retired.push_back(std::move(texture));
      }
      constexpr std::size_t kMaxRetired = 16;
      while (retired.size() > kMaxRetired) {
        retired.pop_front();
      }
    }

    struct TexturePreviewCache {
      std::shared_ptr<LveTexture> texture{};
      VkDescriptorSet descriptor{VK_NULL_HANDLE};
    };

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

    std::string toLowerCopy(const std::string &value) {
      std::string out;
      out.reserve(value.size());
      for (char ch : value) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
      }
      return out;
    }

    bool hasExtension(const std::filesystem::path &path, std::initializer_list<const char *> exts) {
      const std::string ext = toLowerCopy(path.extension().string());
      for (const char *candidate : exts) {
        if (ext == toLowerCopy(candidate)) {
          return true;
        }
      }
      return false;
    }

    bool isTextureFile(const std::string &path) {
      return hasExtension(std::filesystem::path(path),
        {".png", ".jpg", ".jpeg", ".tga", ".bmp", ".dds", ".hdr", ".tiff", ".ktx", ".ktx2"});
    }

    ImTextureID getPreviewTextureId(
      const std::shared_ptr<LveTexture> &texture,
      TexturePreviewCache &cache) {
      if (!texture) {
        cache.texture.reset();
        cache.descriptor = VK_NULL_HANDLE;
        return ImTextureID_Invalid;
      }
      if (cache.texture != texture || cache.descriptor == VK_NULL_HANDLE) {
        if (cache.texture && cache.texture != texture) {
          retirePreviewTexture(cache.texture);
        }
        cache.texture = texture;
        cache.descriptor = ImGui_ImplVulkan_AddTexture(
          texture->sampler(),
          texture->imageView(),
          texture->getImageLayout());
      }
      return static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(cache.descriptor));
    }

    ImVec2 calcPreviewSize(const std::shared_ptr<LveTexture> &texture, float maxSize) {
      if (!texture) return ImVec2{maxSize, maxSize};
      const VkExtent3D extent = texture->getExtent();
      if (extent.width == 0 || extent.height == 0) {
        return ImVec2{maxSize, maxSize};
      }
      const float w = static_cast<float>(extent.width);
      const float h = static_cast<float>(extent.height);
      if (w >= h) {
        return ImVec2{maxSize, maxSize * (h / w)};
      }
      return ImVec2{maxSize * (w / h), maxSize};
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
    int &selectedNodeIndex,
    const MaterialPickResult &materialPick) {
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
    static LveGameObject::id_t lastMaterialOwnerId = 0;
    static std::string lastMaterialPath{};
    static const LveMaterial *lastMaterialPtr = nullptr;
    static MaterialData materialDraft{};
    static std::string materialDraftPath{};
    static bool materialDirty = false;
    static bool autoPreview = true;
    static TexturePreviewCache baseColorPreview{};
    static TexturePreviewCache normalPreview{};
    static TexturePreviewCache metallicPreview{};
    static TexturePreviewCache occlusionPreview{};
    static TexturePreviewCache emissivePreview{};

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

    auto refreshMaterialDraft = [&](const std::string &defaultPath) {
      if (selected->material) {
        materialDraft = selected->material->getData();
      } else {
        materialDraft = MaterialData{};
      }
      if (materialDraft.name.empty()) {
        materialDraft.name = selected->name.empty()
          ? ("Material_" + std::to_string(selected->getId()))
          : selected->name;
      }
      materialDraftPath = selected->materialPath.empty()
        ? defaultPath
        : selected->materialPath;
      materialDirty = false;
      lastMaterialOwnerId = selected->getId();
      lastMaterialPath = selected->materialPath;
      lastMaterialPtr = selected->material.get();
    };

    auto applyMaterialPick = [&]() -> bool {
      if (!materialPick.available) {
        return false;
      }
      switch (materialPick.slot) {
        case MaterialTextureSlot::BaseColor:
          materialDraft.textures.baseColor = materialPick.path;
          break;
        case MaterialTextureSlot::Normal:
          materialDraft.textures.normal = materialPick.path;
          break;
        case MaterialTextureSlot::MetallicRoughness:
          materialDraft.textures.metallicRoughness = materialPick.path;
          break;
        case MaterialTextureSlot::Occlusion:
          materialDraft.textures.occlusion = materialPick.path;
          break;
        case MaterialTextureSlot::Emissive:
          materialDraft.textures.emissive = materialPick.path;
          break;
      }
      materialDirty = true;
      return true;
    };

    ImGui::Text("ID: %u", selected->getId());
    char nameBuf[128];
    std::snprintf(nameBuf, sizeof(nameBuf), "%s", selected->name.c_str());
    if (ImGui::InputText("Name##Object", nameBuf, sizeof(nameBuf))) {
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
    if (selected->model && !selected->isSprite && !selected->pointLight) {
      ImGui::Text("Material: %s", selected->materialPath.empty() ? "-" : selected->materialPath.c_str());
    }
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
      const std::string defaultMaterialPath =
        "Assets/materials/Material_" + std::to_string(selected->getId()) + ".mat";
      if (lastMaterialOwnerId != selected->getId() ||
          lastMaterialPath != selected->materialPath ||
          lastMaterialPtr != selected->material.get()) {
        refreshMaterialDraft(defaultMaterialPath);
      }
      bool materialChangedThisFrame = applyMaterialPick();
      const float previewSize = 64.0f;

      ImGui::Separator();
      ImGui::Text("Material");
      if (materialDirty) {
        ImGui::TextColored(ImVec4(1.f, 0.7f, 0.2f, 1.f), "Unsaved changes");
      }

      char pathBuf[256];
      std::snprintf(pathBuf, sizeof(pathBuf), "%s", materialDraftPath.c_str());
      if (ImGui::InputText("Material Path", pathBuf, sizeof(pathBuf))) {
        materialDraftPath = pathBuf;
      }

      char nameBuf[128];
      std::snprintf(nameBuf, sizeof(nameBuf), "%s", materialDraft.name.c_str());
      if (ImGui::InputText("Name##Material", nameBuf, sizeof(nameBuf))) {
        materialDraft.name = nameBuf;
        materialDirty = true;
      }

      if (ImGui::ColorEdit4("Base Color", &materialDraft.factors.baseColor.x)) {
        materialDirty = true;
        materialChangedThisFrame = true;
      }
      if (ImGui::SliderFloat("Metallic", &materialDraft.factors.metallic, 0.f, 1.f)) {
        materialDirty = true;
        materialChangedThisFrame = true;
      }
      if (ImGui::SliderFloat("Roughness", &materialDraft.factors.roughness, 0.f, 1.f)) {
        materialDirty = true;
        materialChangedThisFrame = true;
      }
      if (ImGui::ColorEdit3("Emissive", &materialDraft.factors.emissive.x)) {
        materialDirty = true;
        materialChangedThisFrame = true;
      }
      if (ImGui::SliderFloat("Occlusion", &materialDraft.factors.occlusionStrength, 0.f, 1.f)) {
        materialDirty = true;
        materialChangedThisFrame = true;
      }
      if (ImGui::SliderFloat("Normal Scale", &materialDraft.factors.normalScale, 0.f, 2.f)) {
        materialDirty = true;
        materialChangedThisFrame = true;
      }

      ImGui::Checkbox("Auto Preview", &autoPreview);

      auto editTexturePath = [&](const char *label,
                                 MaterialTextureSlot slot,
                                 std::string &value,
                                 const std::shared_ptr<LveTexture> &previewTexture,
                                 TexturePreviewCache &cache) {
        bool changed = false;
        ImGui::PushID(label);
        ImGui::TextUnformatted(label);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-140.0f);
        char buf[256];
        std::snprintf(buf, sizeof(buf), "%s", value.c_str());
        if (ImGui::InputText("##path", buf, sizeof(buf))) {
          value = buf;
          materialDirty = true;
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
          changed = true;
        }
        if (ImGui::BeginDragDropTarget()) {
          if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            if (payload->Data && payload->DataSize > 0) {
              std::string dropped(static_cast<const char *>(payload->Data));
              if (isTextureFile(dropped)) {
                value = dropped;
                materialDirty = true;
                changed = true;
              }
            }
          }
          ImGui::EndDragDropTarget();
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Pick")) {
          actions.materialPickRequested = true;
          actions.materialPickSlot = slot;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Clear")) {
          value.clear();
          materialDirty = true;
          changed = true;
        }
        if (previewTexture) {
          ImTextureID previewId = getPreviewTextureId(previewTexture, cache);
          if (previewId) {
            ImVec2 size = calcPreviewSize(previewTexture, previewSize);
            ImGui::Spacing();
            ImGui::Image(previewId, size);
            const VkExtent3D extent = previewTexture->getExtent();
            if (extent.width > 0 && extent.height > 0) {
              ImGui::TextDisabled("%ux%u", extent.width, extent.height);
            }
          }
        } else {
          ImGui::Spacing();
          ImGui::TextDisabled("No preview");
        }
        if (!value.empty()) {
          std::error_code ec;
          if (!std::filesystem::exists(value, ec)) {
            ImGui::TextColored(ImVec4(1.f, 0.4f, 0.4f, 1.f), "Missing file");
          }
        }
        ImGui::PopID();
        return changed;
      };

      const std::shared_ptr<LveTexture> baseColorTex =
        selected->material ? selected->material->getBaseColorTexture() : nullptr;
      const std::shared_ptr<LveTexture> normalTex =
        selected->material ? selected->material->getNormalTexture() : nullptr;
      const std::shared_ptr<LveTexture> metallicTex =
        selected->material ? selected->material->getMetallicRoughnessTexture() : nullptr;
      const std::shared_ptr<LveTexture> occlusionTex =
        selected->material ? selected->material->getOcclusionTexture() : nullptr;
      const std::shared_ptr<LveTexture> emissiveTex =
        selected->material ? selected->material->getEmissiveTexture() : nullptr;

      if (editTexturePath("Base Color Tex", MaterialTextureSlot::BaseColor, materialDraft.textures.baseColor, baseColorTex, baseColorPreview)) {
        materialChangedThisFrame = true;
      }
      if (editTexturePath("Normal Tex", MaterialTextureSlot::Normal, materialDraft.textures.normal, normalTex, normalPreview)) {
        materialChangedThisFrame = true;
      }
      if (editTexturePath("Metallic/Roughness Tex", MaterialTextureSlot::MetallicRoughness, materialDraft.textures.metallicRoughness, metallicTex, metallicPreview)) {
        materialChangedThisFrame = true;
      }
      if (editTexturePath("Occlusion Tex", MaterialTextureSlot::Occlusion, materialDraft.textures.occlusion, occlusionTex, occlusionPreview)) {
        materialChangedThisFrame = true;
      }
      if (editTexturePath("Emissive Tex", MaterialTextureSlot::Emissive, materialDraft.textures.emissive, emissiveTex, emissivePreview)) {
        materialChangedThisFrame = true;
      }

      ImGui::Spacing();
      ImGui::BeginDisabled(materialDraftPath.empty());
      if (ImGui::Button("Apply Path")) {
        actions.materialLoadRequested = true;
        actions.materialPath = materialDraftPath;
      }
      ImGui::SameLine();
      if (ImGui::Button("Save Material")) {
        actions.materialSaveRequested = true;
        actions.materialPath = materialDraftPath;
        actions.materialData = materialDraft;
        materialDirty = false;
      }
      if (!autoPreview) {
        ImGui::SameLine();
        if (ImGui::Button("Preview")) {
          actions.materialPreviewRequested = true;
          actions.materialPath = materialDraftPath;
          actions.materialData = materialDraft;
        }
      }
      ImGui::SameLine();
      if (ImGui::Button("Revert")) {
        refreshMaterialDraft(defaultMaterialPath);
      }
      ImGui::EndDisabled();
      ImGui::SameLine();
      if (ImGui::Button("Clear Material")) {
        actions.materialClearRequested = true;
      }

      if (autoPreview && materialChangedThisFrame && !materialDraftPath.empty()) {
        actions.materialPreviewRequested = true;
        actions.materialPath = materialDraftPath;
        actions.materialData = materialDraft;
      }

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
