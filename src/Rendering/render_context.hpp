#pragma once

#include "Engine/Backend/descriptors.hpp"
#include "Engine/Backend/device.hpp"
#include "Engine/Backend/buffer.hpp"
#include "Rendering/frame_info.hpp"
#include "Rendering/point_light_system.hpp"
#include "Rendering/renderer.hpp"
#include "Rendering/simple_render_system.hpp"
#include "Rendering/sprite_render_system.hpp"

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
    bool beginSceneViewRenderPass(VkCommandBuffer commandBuffer);
    void endSceneViewRenderPass(VkCommandBuffer commandBuffer);
    bool beginGameViewRenderPass(VkCommandBuffer commandBuffer);
    void endGameViewRenderPass(VkCommandBuffer commandBuffer);
    void ensureOffscreenTargets(uint32_t sceneWidth, uint32_t sceneHeight, uint32_t gameWidth, uint32_t gameHeight);

    bool wasSwapChainRecreated();
    VkRenderPass getSwapChainRenderPass() const;
    size_t getSwapChainImageCount() const { return lveRenderer.getSwapChainImageCount(); }
    VkDescriptorSet getSceneViewDescriptor() const;
    VkDescriptorSet getGameViewDescriptor() const;
    VkExtent2D getSceneViewExtent() const;
    VkExtent2D getGameViewExtent() const;

    FrameInfo makeFrameInfo(float frameTime, LveCamera &camera, std::vector<LveGameObject*> &gameObjects, VkCommandBuffer commandBuffer);
    void updateGlobalUbo(int frameIndex, const GlobalUbo &ubo);

    SimpleRenderSystem &simpleSystem() { return *simpleRenderSystem; }
    SpriteRenderSystem &spriteSystem() { return *spriteRenderSystem; }
    PointLightSystem &pointLightSystem() { return *pointLightSystemPtr; }

  private:
    struct OffscreenTarget {
      VkExtent2D extent{};
      VkImage colorImage{VK_NULL_HANDLE};
      VkDeviceMemory colorMemory{VK_NULL_HANDLE};
      VkImageView colorView{VK_NULL_HANDLE};
      VkImage depthImage{VK_NULL_HANDLE};
      VkDeviceMemory depthMemory{VK_NULL_HANDLE};
      VkImageView depthView{VK_NULL_HANDLE};
      VkFramebuffer framebuffer{VK_NULL_HANDLE};
      VkSampler sampler{VK_NULL_HANDLE};
      VkDescriptorSet imguiDescriptor{VK_NULL_HANDLE};
    };

    void createBuffersAndDescriptors();
    void createOffscreenRenderPass();
    void destroyOffscreenRenderPass();
    void destroyOffscreenTarget(OffscreenTarget &target);
    void createOffscreenTarget(OffscreenTarget &target, VkExtent2D extent);
    void beginOffscreenRenderPass(VkCommandBuffer commandBuffer, const OffscreenTarget &target);
    void createRenderSystems();

    LveDevice &lveDevice;
    LveRenderer &lveRenderer;

    std::unique_ptr<LveDescriptorPool> globalPool{};
    std::unique_ptr<LveDescriptorPool> objectDescriptorPool{};
    std::vector<std::unique_ptr<LveBuffer>> uboBuffers;
    std::unique_ptr<LveDescriptorSetLayout> globalSetLayout;
    std::vector<VkDescriptorSet> globalDescriptorSets;

    std::unique_ptr<SimpleRenderSystem> simpleRenderSystem;
    std::unique_ptr<SpriteRenderSystem> spriteRenderSystem;
    std::unique_ptr<PointLightSystem> pointLightSystemPtr;

    VkRenderPass offscreenRenderPass{VK_NULL_HANDLE};
    VkFormat offscreenColorFormat{VK_FORMAT_UNDEFINED};
    VkFormat offscreenDepthFormat{VK_FORMAT_UNDEFINED};
    OffscreenTarget sceneViewTarget{};
    OffscreenTarget gameViewTarget{};
    bool swapChainRecreated{false};
  };
} // namespace lve
