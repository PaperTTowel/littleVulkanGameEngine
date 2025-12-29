#pragma once

#include "Engine/Backend/render_types.hpp"

#include <cstddef>
#include <vector>

namespace lve {
  class LveCamera;
  class LveGameObject;
}

namespace lve::backend {
  class RenderBackend {
  public:
    virtual ~RenderBackend() = default;

    virtual CommandBufferHandle beginFrame() = 0;
    virtual void endFrame() = 0;

    virtual void beginSwapChainRenderPass(CommandBufferHandle commandBuffer) = 0;
    virtual void endSwapChainRenderPass(CommandBufferHandle commandBuffer) = 0;

    virtual void ensureOffscreenTargets(
      std::uint32_t sceneWidth,
      std::uint32_t sceneHeight,
      std::uint32_t gameWidth,
      std::uint32_t gameHeight) = 0;

    virtual bool wasSwapChainRecreated() const = 0;
    virtual RenderPassHandle getSwapChainRenderPass() const = 0;
    virtual std::size_t getSwapChainImageCount() const = 0;
    virtual DescriptorSetHandle getSceneViewDescriptor() const = 0;
    virtual DescriptorSetHandle getGameViewDescriptor() const = 0;
    virtual float getAspectRatio() const = 0;
    virtual int getFrameIndex() const = 0;

    virtual void setWireframe(bool enabled) = 0;
    virtual void setNormalView(bool enabled) = 0;

    virtual void renderSceneView(
      float frameTime,
      LveCamera &camera,
      std::vector<LveGameObject*> &objects,
      CommandBufferHandle commandBuffer) = 0;
    virtual void renderGameView(
      float frameTime,
      LveCamera &camera,
      std::vector<LveGameObject*> &objects,
      CommandBufferHandle commandBuffer) = 0;
  };
} // namespace lve::backend
