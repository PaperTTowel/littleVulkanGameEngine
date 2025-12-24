#include "render_context.hpp"

// std
#include <stdexcept>

namespace lve {

  RenderContext::RenderContext(LveDevice &device, LveRenderer &renderer)
    : lveDevice{device},
      lveRenderer{renderer} {
    createBuffersAndDescriptors();
    createRenderSystems();
  }

  void RenderContext::createBuffersAndDescriptors() {
    globalPool = LveDescriptorPool::Builder(lveDevice)
      .setMaxSets(LveSwapChain::MAX_FRAMES_IN_FLIGHT)
      .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, LveSwapChain::MAX_FRAMES_IN_FLIGHT)
      .build();

    framePools.resize(LveSwapChain::MAX_FRAMES_IN_FLIGHT);
    auto framePoolBuilder = LveDescriptorPool::Builder(lveDevice)
      .setMaxSets(1000)
      .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000)
      .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000)
      .setPoolFlags(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT);
    for (int i = 0; i < framePools.size(); i++) {
      framePools[i] = framePoolBuilder.build();
    }

    uboBuffers.resize(LveSwapChain::MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < uboBuffers.size(); i++) {
      uboBuffers[i] = std::make_unique<LveBuffer>(
        lveDevice,
        sizeof(GlobalUbo),
        1,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
      uboBuffers[i]->map();
    }

    globalSetLayout = LveDescriptorSetLayout::Builder(lveDevice)
      .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
      .build();

    globalDescriptorSets.resize(LveSwapChain::MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < globalDescriptorSets.size(); i++) {
      auto bufferInfo = uboBuffers[i]->descriptorInfo();
      if (!LveDescriptorWriter(*globalSetLayout, *globalPool)
              .writeBuffer(0, &bufferInfo)
              .build(globalDescriptorSets[i])) {
        throw std::runtime_error("failed to build global descriptor set");
      }
    }
  }

  void RenderContext::createRenderSystems() {
    simpleRenderSystem = std::make_unique<SimpleRenderSystem>(
      lveDevice,
      lveRenderer.getSwapChainRenderPass(),
      globalSetLayout->getDescriptorSetLayout());
    spriteRenderSystem = std::make_unique<SpriteRenderSystem>(
      lveDevice,
      lveRenderer.getSwapChainRenderPass(),
      globalSetLayout->getDescriptorSetLayout());
    pointLightSystemPtr = std::make_unique<PointLightSystem>(
      lveDevice,
      lveRenderer.getSwapChainRenderPass(),
      globalSetLayout->getDescriptorSetLayout());
  }

  VkCommandBuffer RenderContext::beginFrame() {
    auto commandBuffer = lveRenderer.beginFrame();
    if (lveRenderer.wasSwapChainRecreated()) {
      createRenderSystems();
      swapChainRecreated = true;
    }
    return commandBuffer;
  }

  void RenderContext::endFrame() {
    lveRenderer.endFrame();
  }

  void RenderContext::beginSwapChainRenderPass(VkCommandBuffer commandBuffer) {
    lveRenderer.beginSwapChainRenderPass(commandBuffer);
  }

  void RenderContext::endSwapChainRenderPass(VkCommandBuffer commandBuffer) {
    lveRenderer.endSwapChainRenderPass(commandBuffer);
  }

  bool RenderContext::wasSwapChainRecreated() {
    if (swapChainRecreated) {
      swapChainRecreated = false;
      return true;
    }
    return false;
  }

  VkRenderPass RenderContext::getSwapChainRenderPass() const {
    return lveRenderer.getSwapChainRenderPass();
  }

  FrameInfo RenderContext::makeFrameInfo(float frameTime, LveCamera &camera, LveGameObject::Map &gameObjects, VkCommandBuffer commandBuffer) {
    int frameIndex = lveRenderer.getFrameindex();
    framePools[frameIndex]->resetPool();
    return FrameInfo{
      frameIndex,
      frameTime,
      commandBuffer,
      camera,
      globalDescriptorSets[frameIndex],
      *framePools[frameIndex],
      gameObjects};
  }

  void RenderContext::updateGlobalUbo(int frameIndex, const GlobalUbo &ubo) {
    uboBuffers[frameIndex]->writeToBuffer((void *)&ubo);
    uboBuffers[frameIndex]->flush();
  }

} // namespace lve
