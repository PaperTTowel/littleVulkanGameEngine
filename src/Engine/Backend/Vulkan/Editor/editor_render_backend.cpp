#include "Engine/Backend/Vulkan/Editor/editor_render_backend.hpp"

#include "Engine/Backend/Vulkan/Render/texture.hpp"
#include "Engine/IO/image_io.hpp"

#include <backends/imgui_impl_vulkan.h>
#include <vulkan/vulkan.h>

namespace lve::backend {

  VulkanEditorRenderBackend::VulkanEditorRenderBackend(LveWindow &window, LveDevice &device)
    : imgui{window, device}, device{device} {}

  void VulkanEditorRenderBackend::init(RenderPassHandle renderPass, std::uint32_t imageCount) {
    imgui.init(reinterpret_cast<VkRenderPass>(renderPass), imageCount);
  }

  void VulkanEditorRenderBackend::onRenderPassChanged(
    RenderPassHandle renderPass,
    std::uint32_t imageCount) {
    imgui.shutdown();
    imgui.init(reinterpret_cast<VkRenderPass>(renderPass), imageCount);
  }

  void VulkanEditorRenderBackend::shutdown() {
    imgui.shutdown();
    previewCache.clear();
  }

  void VulkanEditorRenderBackend::newFrame() {
    imgui.newFrame();
  }

  void VulkanEditorRenderBackend::buildUI(
    float frameTime,
    const glm::vec3 &cameraPos,
    const glm::vec3 &cameraRot,
    bool &wireframeEnabled,
    bool &normalViewEnabled,
    bool &useOrthoCamera,
    bool &showEngineStats) {
    imgui.buildUI(
      frameTime,
      cameraPos,
      cameraRot,
      wireframeEnabled,
      normalViewEnabled,
      useOrthoCamera,
      showEngineStats);
  }

  void VulkanEditorRenderBackend::render(CommandBufferHandle commandBuffer) {
    imgui.render(reinterpret_cast<VkCommandBuffer>(commandBuffer));
  }

  void VulkanEditorRenderBackend::renderPlatformWindows() {
    imgui.renderPlatformWindows();
  }

  void VulkanEditorRenderBackend::waitIdle() {
    vkDeviceWaitIdle(device.device());
  }

  DescriptorSetHandle VulkanEditorRenderBackend::getTexturePreview(
    const std::string &path,
    RenderExtent &outExtent) {
    outExtent = RenderExtent{};
    if (path.empty()) {
      return nullptr;
    }

    auto it = previewCache.find(path);
    if (it != previewCache.end()) {
      outExtent = it->second.extent;
      return it->second.descriptor;
    }

    ImageData image{};
    if (!loadImageDataFromFile(path, image, nullptr)) {
      return nullptr;
    }
    auto texture = LveTexture::createTextureFromRgba(
      device,
      image.pixels.data(),
      image.width,
      image.height);

    PreviewEntry entry{};
    entry.texture = std::shared_ptr<LveTexture>(std::move(texture));
    const VkExtent3D extent = entry.texture->getExtent();
    entry.extent = RenderExtent{extent.width, extent.height};
    entry.descriptor = reinterpret_cast<DescriptorSetHandle>(
      ImGui_ImplVulkan_AddTexture(
        entry.texture->sampler(),
        entry.texture->imageView(),
        entry.texture->getImageLayout()));

    const DescriptorSetHandle handle = entry.descriptor;
    outExtent = entry.extent;
    previewCache.emplace(path, std::move(entry));
    return handle;
  }

} // namespace lve::backend
