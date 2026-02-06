#pragma once

#include "Engine/Backend/editor_render_backend.hpp"
#include "ImGui/imgui_layer.hpp"

#include "Engine/Backend/Vulkan/Render/texture.hpp"

#include <memory>
#include <string>
#include <unordered_map>

namespace lve::backend {
  class VulkanEditorRenderBackend final : public EditorRenderBackend {
  public:
    VulkanEditorRenderBackend(LveWindow &window, LveDevice &device);

    void init(RenderPassHandle renderPass, std::uint32_t imageCount) override;
    void onRenderPassChanged(RenderPassHandle renderPass, std::uint32_t imageCount) override;
    void shutdown() override;
    void newFrame() override;
    void buildUI(
      float frameTime,
      const glm::vec3 &cameraPos,
      const glm::vec3 &cameraRot,
      const std::string &tileDebugText,
      bool &wireframeEnabled,
      bool &normalViewEnabled,
      bool &useOrthoCamera,
      lve::game::PlayerTuning &playerTuning,
      bool &showEngineStats) override;
    void render(CommandBufferHandle commandBuffer) override;
    void renderPlatformWindows() override;
    void waitIdle() override;
    DescriptorSetHandle getTexturePreview(
      const std::string &path,
      RenderExtent &outExtent) override;

  private:
    struct PreviewEntry {
      std::shared_ptr<LveTexture> texture{};
      DescriptorSetHandle descriptor{nullptr};
      RenderExtent extent{};
    };

    ImGuiLayer imgui;
    LveDevice &device;
    std::unordered_map<std::string, PreviewEntry> previewCache;
  };
} // namespace lve::backend
