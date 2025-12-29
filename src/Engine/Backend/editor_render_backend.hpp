#pragma once

#include "Engine/Backend/render_types.hpp"

#include <string>
#include <cstdint>
#include <glm/vec3.hpp>

namespace lve::backend {
  class EditorRenderBackend {
  public:
    virtual ~EditorRenderBackend() = default;

    virtual void init(RenderPassHandle renderPass, std::uint32_t imageCount) = 0;
    virtual void onRenderPassChanged(RenderPassHandle renderPass, std::uint32_t imageCount) = 0;
    virtual void shutdown() = 0;
    virtual void newFrame() = 0;
    virtual void buildUI(
      float frameTime,
      const glm::vec3 &cameraPos,
      const glm::vec3 &cameraRot,
      bool &wireframeEnabled,
      bool &normalViewEnabled,
      bool &useOrthoCamera,
      bool &showEngineStats) = 0;
    virtual void render(CommandBufferHandle commandBuffer) = 0;
    virtual void renderPlatformWindows() = 0;
    virtual void waitIdle() = 0;
    virtual DescriptorSetHandle getTexturePreview(
      const std::string &path,
      RenderExtent &outExtent) = 0;
  };
} // namespace lve::backend
