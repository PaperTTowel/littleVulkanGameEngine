#pragma once

#include "Engine/Backend/descriptors.hpp"
#include "Engine/Backend/device.hpp"
#include "Rendering/frame_info.hpp"
#include "Rendering/point_light_system.hpp"
#include "Rendering/renderer.hpp"
#include "Rendering/simple_render_system.hpp"
#include "Rendering/sprite_render_system.hpp"

// std
#include <memory>
#include <vector>

namespace lve {
  class RenderContext {
  public:
    RenderContext(LveDevice &device, LveRenderer &renderer);

    VkCommandBuffer beginFrame();
    void endFrame();
    void beginSwapChainRenderPass(VkCommandBuffer commandBuffer);
    void endSwapChainRenderPass(VkCommandBuffer commandBuffer);

    bool wasSwapChainRecreated();
    VkRenderPass getSwapChainRenderPass() const;

    FrameInfo makeFrameInfo(float frameTime, LveCamera &camera, LveGameObject::Map &gameObjects, VkCommandBuffer commandBuffer);
    void updateGlobalUbo(int frameIndex, const GlobalUbo &ubo);

    SimpleRenderSystem &simpleSystem() { return *simpleRenderSystem; }
    SpriteRenderSystem &spriteSystem() { return *spriteRenderSystem; }
    PointLightSystem &pointLightSystem() { return *pointLightSystemPtr; }

  private:
    void createBuffersAndDescriptors();
    void createRenderSystems();

    LveDevice &lveDevice;
    LveRenderer &lveRenderer;

    std::unique_ptr<LveDescriptorPool> globalPool{};
    std::vector<std::unique_ptr<LveDescriptorPool>> framePools;
    std::vector<std::unique_ptr<LveBuffer>> uboBuffers;
    std::unique_ptr<LveDescriptorSetLayout> globalSetLayout;
    std::vector<VkDescriptorSet> globalDescriptorSets;

    std::unique_ptr<SimpleRenderSystem> simpleRenderSystem;
    std::unique_ptr<SpriteRenderSystem> spriteRenderSystem;
    std::unique_ptr<PointLightSystem> pointLightSystemPtr;

    bool swapChainRecreated{false};
  };
} // namespace lve
