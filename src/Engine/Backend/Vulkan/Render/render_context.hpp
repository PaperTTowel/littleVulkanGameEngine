#pragma once

#include "Engine/Backend/Vulkan/Core/descriptors.hpp"
#include "Engine/Backend/Vulkan/Core/device.hpp"
#include "Engine/Backend/Vulkan/Core/buffer.hpp"
#include "Engine/Backend/Vulkan/Render/frame_info.hpp"
#include "Engine/Backend/Vulkan/Render/renderer.hpp"
#include "Engine/Backend/Vulkan/Render/sprite_render_system.hpp"

// std
#include <cstdint>
#include <memory>
#include <vector>

namespace lve {
  class RenderContext {
  public:
    RenderContext(LveDevice &device, LveRenderer &renderer);
    ~RenderContext();

    VkCommandBuffer beginFrame();
    void endFrame();
    void beginSwapChainRenderPass(VkCommandBuffer commandBuffer);
    void endSwapChainRenderPass(VkCommandBuffer commandBuffer);

    bool wasSwapChainRecreated() const;
    void clearSwapChainRecreated();
    VkRenderPass getSwapChainRenderPass() const;
    size_t getSwapChainImageCount() const { return lveRenderer.getSwapChainImageCount(); }

    FrameInfo makeFrameInfo(float frameTime, LveCamera &camera, std::vector<LveGameObject*> &gameObjects, VkCommandBuffer commandBuffer);
    void updateGlobalUbo(int frameIndex, const GlobalUbo &ubo);

    SpriteRenderSystem &spriteSystem() { return *spriteRenderSystem; }

  private:
    void createBuffersAndDescriptors();
    void createRenderSystems();

    LveDevice &lveDevice;
    LveRenderer &lveRenderer;

    std::unique_ptr<LveDescriptorPool> globalPool{};
    std::unique_ptr<LveDescriptorPool> objectDescriptorPool{};
    std::vector<std::unique_ptr<LveBuffer>> uboBuffers;
    std::unique_ptr<LveDescriptorSetLayout> globalSetLayout;
    std::vector<VkDescriptorSet> globalDescriptorSets;

    std::unique_ptr<SpriteRenderSystem> spriteRenderSystem;

    bool swapChainRecreated{false};
  };
} // namespace lve


