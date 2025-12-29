#pragma once

#include "utils/game_object.hpp"
#include "utils/sprite_animator.hpp"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

namespace lve::editor {

  struct GizmoContext {
    void *drawList{nullptr};
    float x{0.f};
    float y{0.f};
    float width{0.f};
    float height{0.f};
    bool valid{false};
  };

  struct TransformSnapshot {
    glm::vec3 translation{};
    glm::vec3 rotation{};
    glm::vec3 scale{1.f};
  };

  struct TexturePreviewCache {
    std::shared_ptr<LveTexture> texture{};
    VkDescriptorSet descriptor{VK_NULL_HANDLE};
  };

  struct InspectorState {
    LveGameObject::id_t lastSelectedId{0};
    bool transformEditing{false};
    TransformSnapshot transformEditStart{};
    bool nameEditing{false};
    std::string nameEditStart{};
    bool nodeOverrideEditing{false};
    std::vector<NodeTransformOverride> nodeOverrideEditStart{};
    bool gizmoWasUsing{false};
    bool gizmoWasEditingNode{false};
    const LveModel *lastSelectedModel{nullptr};
    LveGameObject::id_t lastMaterialOwnerId{0};
    std::string lastMaterialPath{};
    const LveMaterial *lastMaterialPtr{nullptr};
    MaterialData materialDraft{};
    std::string materialDraftPath{};
    bool materialDirty{false};
    bool autoPreview{true};
    TexturePreviewCache baseColorPreview{};
    TexturePreviewCache normalPreview{};
    TexturePreviewCache metallicPreview{};
    TexturePreviewCache occlusionPreview{};
    TexturePreviewCache emissivePreview{};
  };

  enum class MaterialTextureSlot {
    BaseColor,
    Normal,
    MetallicRoughness,
    Occlusion,
    Emissive
  };

  struct InspectorActions {
    bool transformChanged{false};
    bool transformCommitted{false};
    TransformSnapshot beforeTransform{};
    TransformSnapshot afterTransform{};
    bool nameChanged{false};
    std::string beforeName{};
    std::string afterName{};
    bool nodeOverridesChanged{false};
    bool nodeOverridesCommitted{false};
    std::vector<NodeTransformOverride> beforeNodeOverrides{};
    std::vector<NodeTransformOverride> afterNodeOverrides{};
    bool materialSaveRequested{false};
    bool materialLoadRequested{false};
    bool materialClearRequested{false};
    std::string materialPath{};
    MaterialData materialData{};
    bool materialPickRequested{false};
    MaterialTextureSlot materialPickSlot{MaterialTextureSlot::BaseColor};
    bool materialPreviewRequested{false};
  };

  struct MaterialPickResult {
    bool available{false};
    MaterialTextureSlot slot{MaterialTextureSlot::BaseColor};
    std::string path{};
  };

  InspectorActions BuildInspectorPanel(
    LveGameObject *selected,
    SpriteAnimator *animator,
    InspectorState &state,
    const glm::mat4 &view,
    const glm::mat4 &projection,
    VkExtent2D viewportExtent,
    bool *open,
    const GizmoContext &gizmoContext,
    int gizmoOperation,
    int gizmoMode,
    int &selectedNodeIndex,
    const MaterialPickResult &materialPick);

} // namespace lve::editor
