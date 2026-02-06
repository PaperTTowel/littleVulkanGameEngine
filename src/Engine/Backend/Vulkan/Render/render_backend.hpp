#pragma once

#include "Engine/Backend/render_backend.hpp"
#include "Engine/Backend/Vulkan/Render/render_context.hpp"
#include "Engine/Backend/Vulkan/Render/renderer.hpp"

namespace lve::backend {
  class VulkanRenderBackend final : public RenderBackend {
  public:
    VulkanRenderBackend(LveWindow &window, LveDevice &device);

    CommandBufferHandle beginFrame() override;
    void endFrame() override;

    void beginSwapChainRenderPass(CommandBufferHandle commandBuffer) override;
    void endSwapChainRenderPass(CommandBufferHandle commandBuffer) override;

    bool wasSwapChainRecreated() const override;
    RenderPassHandle getSwapChainRenderPass() const override;
    std::size_t getSwapChainImageCount() const override;
    float getAspectRatio() const override;
    int getFrameIndex() const override;

    void setWireframe(bool enabled) override;
    void setNormalView(bool enabled) override;

    void renderMainView(
      float frameTime,
      LveCamera &camera,
      std::vector<LveGameObject*> &objects,
      CommandBufferHandle commandBuffer) override;

  private:
    LveRenderer renderer;
    RenderContext renderContext;
  };
} // namespace lve::backend
