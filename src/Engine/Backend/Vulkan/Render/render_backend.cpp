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

  void VulkanRenderBackend::ensureOffscreenTargets(
    std::uint32_t sceneWidth,
    std::uint32_t sceneHeight,
    std::uint32_t gameWidth,
    std::uint32_t gameHeight) {
    renderContext.ensureOffscreenTargets(sceneWidth, sceneHeight, gameWidth, gameHeight);
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

  DescriptorSetHandle VulkanRenderBackend::getSceneViewDescriptor() const {
    return reinterpret_cast<DescriptorSetHandle>(renderContext.getSceneViewDescriptor());
  }

  DescriptorSetHandle VulkanRenderBackend::getGameViewDescriptor() const {
    return reinterpret_cast<DescriptorSetHandle>(renderContext.getGameViewDescriptor());
  }

  float VulkanRenderBackend::getAspectRatio() const {
    return renderer.getAspectRatio();
  }

  int VulkanRenderBackend::getFrameIndex() const {
    return renderer.getFrameindex();
  }

  void VulkanRenderBackend::setWireframe(bool enabled) {
    renderContext.simpleSystem().setWireframe(enabled);
  }

  void VulkanRenderBackend::setNormalView(bool enabled) {
    renderContext.simpleSystem().setNormalView(enabled);
  }

  void VulkanRenderBackend::renderSceneView(
    float frameTime,
    LveCamera &camera,
    std::vector<LveGameObject*> &objects,
    CommandBufferHandle commandBuffer) {
    VkCommandBuffer vkCommandBuffer = reinterpret_cast<VkCommandBuffer>(commandBuffer);
    if (!renderContext.beginSceneViewRenderPass(vkCommandBuffer)) {
      return;
    }

    FrameInfo frameInfo = renderContext.makeFrameInfo(
      frameTime,
      camera,
      objects,
      vkCommandBuffer);

    GlobalUbo ubo{};
    ubo.projection = camera.getProjection();
    ubo.view = camera.getView();
    ubo.inverseView = camera.getInverseView();
    renderContext.pointLightSystem().update(frameInfo, ubo);
    renderContext.updateGlobalUbo(frameInfo.frameIndex, ubo);

    renderContext.simpleSystem().renderGameObjects(frameInfo);
    renderContext.pointLightSystem().render(frameInfo);
    renderContext.spriteSystem().renderSprites(frameInfo);
    renderContext.endSceneViewRenderPass(vkCommandBuffer);
  }

  void VulkanRenderBackend::renderGameView(
    float frameTime,
    LveCamera &camera,
    std::vector<LveGameObject*> &objects,
    CommandBufferHandle commandBuffer) {
    VkCommandBuffer vkCommandBuffer = reinterpret_cast<VkCommandBuffer>(commandBuffer);
    if (!renderContext.beginGameViewRenderPass(vkCommandBuffer)) {
      return;
    }

    FrameInfo frameInfo = renderContext.makeFrameInfo(
      frameTime,
      camera,
      objects,
      vkCommandBuffer);

    GlobalUbo ubo{};
    ubo.projection = camera.getProjection();
    ubo.view = camera.getView();
    ubo.inverseView = camera.getInverseView();
    renderContext.pointLightSystem().update(frameInfo, ubo);
    renderContext.updateGlobalUbo(frameInfo.frameIndex, ubo);

    renderContext.simpleSystem().renderGameObjects(frameInfo);
    renderContext.pointLightSystem().render(frameInfo);
    renderContext.spriteSystem().renderSprites(frameInfo);
    renderContext.endGameViewRenderPass(vkCommandBuffer);
  }

} // namespace lve::backend
