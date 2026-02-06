#include "Engine/Backend/Vulkan/Render/render_backend.hpp"

#include "Engine/Backend/Vulkan/Render/frame_info.hpp"

#include <vulkan/vulkan.h>

namespace lve::backend {

  VulkanRenderBackend::VulkanRenderBackend(LveWindow &window, LveDevice &device)
    : renderer{window, device}, renderContext{device, renderer} {}

  CommandBufferHandle VulkanRenderBackend::beginFrame() {
    return reinterpret_cast<CommandBufferHandle>(renderContext.beginFrame());
  }

  void VulkanRenderBackend::endFrame() {
    renderContext.endFrame();
    if (renderContext.wasSwapChainRecreated()) {
      renderContext.clearSwapChainRecreated();
    }
  }

  void VulkanRenderBackend::beginSwapChainRenderPass(CommandBufferHandle commandBuffer) {
    renderContext.beginSwapChainRenderPass(reinterpret_cast<VkCommandBuffer>(commandBuffer));
  }

  void VulkanRenderBackend::endSwapChainRenderPass(CommandBufferHandle commandBuffer) {
    renderContext.endSwapChainRenderPass(reinterpret_cast<VkCommandBuffer>(commandBuffer));
  }

  bool VulkanRenderBackend::wasSwapChainRecreated() const {
    return renderContext.wasSwapChainRecreated();
  }

  RenderPassHandle VulkanRenderBackend::getSwapChainRenderPass() const {
    return reinterpret_cast<RenderPassHandle>(renderContext.getSwapChainRenderPass());
  }

  std::size_t VulkanRenderBackend::getSwapChainImageCount() const {
    return renderContext.getSwapChainImageCount();
  }

  float VulkanRenderBackend::getAspectRatio() const {
    return renderer.getAspectRatio();
  }

  int VulkanRenderBackend::getFrameIndex() const {
    return renderer.getFrameindex();
  }

  void VulkanRenderBackend::setWireframe(bool enabled) {
    (void)enabled;
  }

  void VulkanRenderBackend::setNormalView(bool enabled) {
    (void)enabled;
  }

  void VulkanRenderBackend::renderMainView(
    float frameTime,
    LveCamera &camera,
    std::vector<LveGameObject*> &objects,
    CommandBufferHandle commandBuffer) {
    VkCommandBuffer vkCommandBuffer = reinterpret_cast<VkCommandBuffer>(commandBuffer);
    FrameInfo frameInfo = renderContext.makeFrameInfo(
      frameTime,
      camera,
      objects,
      vkCommandBuffer);

    GlobalUbo ubo{};
    ubo.projection = camera.getProjection();
    ubo.view = camera.getView();
    ubo.inverseView = camera.getInverseView();
    renderContext.updateGlobalUbo(frameInfo.frameIndex, ubo);

    renderContext.spriteSystem().renderSprites(frameInfo);
  }

} // namespace lve::backend
