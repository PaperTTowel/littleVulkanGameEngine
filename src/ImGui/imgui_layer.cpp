#include "imgui_layer.hpp"

#include "Rendering/renderer.hpp"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#include <stdexcept>

namespace lve {

ImGuiLayer::ImGuiLayer(LveWindow &window, LveDevice &device) : window(window), device(device) {}

ImGuiLayer::~ImGuiLayer() { shutdown(); }

void ImGuiLayer::createDescriptorPool(uint32_t imageCount) {
  std::array<VkDescriptorPoolSize, 5> poolSizes{{
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_SAMPLER, 100},
      {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
  }};

  VkDescriptorPoolCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
  info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  info.maxSets = 1000 * static_cast<uint32_t>(poolSizes.size());
  info.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  info.pPoolSizes = poolSizes.data();
  if (vkCreateDescriptorPool(device.device(), &info, nullptr, &imguiPool) != VK_SUCCESS) {
    throw std::runtime_error("failed to create ImGui descriptor pool");
  }
}

void ImGuiLayer::init(VkRenderPass renderPass, uint32_t imageCount) {
  if (initialized) return;

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();

  createDescriptorPool(imageCount);
  ImGui_ImplGlfw_InitForVulkan(window.getGLFWwindow(), true);

  ImGui_ImplVulkan_InitInfo initInfo{};
  initInfo.ApiVersion = VK_API_VERSION_1_3;
  initInfo.Instance = device.getInstance();
  initInfo.PhysicalDevice = device.getPhysicalDevice();
  initInfo.Device = device.device();
  initInfo.QueueFamily = device.findPhysicalQueueFamilies().graphicsFamily;
  initInfo.Queue = device.graphicsQueue();
  initInfo.DescriptorPool = imguiPool;
  initInfo.MinImageCount = imageCount;
  initInfo.ImageCount = imageCount;
  initInfo.PipelineInfoMain.RenderPass = renderPass;
  initInfo.PipelineInfoMain.Subpass = 0;
  initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
  ImGui_ImplVulkan_Init(&initInfo);

  initialized = true;
}

void ImGuiLayer::newFrame() {
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
}

void ImGuiLayer::buildUI(
  float frameTime,
  const glm::vec3 &cameraPos,
  const glm::vec3 &cameraRot,
  bool &wireframeEnabled,
  bool &normalViewEnabled,
  bool &useOrthoCamera) {

  const float frameMsNow = frameTime * 1000.f;
  frameTimeHistory[frameTimeOffset % frameTimeHistory.size()] = frameMsNow;
  frameTimeOffset++;

  // throttle FPS/frame text update to ~1 Hz
  static float accumTime = 0.f;
  static float accumFrameMs = 0.f;
  static int accumCount = 0;
  static float displayFps = 0.f;
  static float displayFrameMs = 0.f;

  accumTime += frameTime;
  accumFrameMs += frameMsNow;
  accumCount++;
  if (accumTime >= 1.0f) {
    displayFps = accumCount > 0 ? (accumCount / accumTime) : 0.f;
    displayFrameMs = accumCount > 0 ? (accumFrameMs / accumCount) : 0.f;
    accumTime = 0.f;
    accumFrameMs = 0.f;
    accumCount = 0;
  }

  ImGui::Begin("Engine Stats");
  ImGui::Text("FPS: %.1f", displayFps);
  ImGui::Text("Frame: %.2f ms", displayFrameMs);
  ImGui::PlotLines(
    "Frame time (ms)",
    frameTimeHistory.data(),
    static_cast<int>(frameTimeHistory.size()),
    static_cast<int>(frameTimeOffset % frameTimeHistory.size()),
    nullptr,
    0.f,
    40.f,
    ImVec2(0, 80));

  ImGui::Separator();
  ImGui::Text("Camera Pos: [%.2f, %.2f, %.2f]", cameraPos.x, cameraPos.y, cameraPos.z);
  ImGui::Text("Camera Rot: [%.2f, %.2f, %.2f]", cameraRot.x, cameraRot.y, cameraRot.z);

  ImGui::Separator();
  ImGui::Checkbox("Wireframe", &wireframeEnabled);
  ImGui::Checkbox("Normal view (shader toggle)", &normalViewEnabled);
  ImGui::Checkbox("Ortho camera", &useOrthoCamera);

  ImGui::End();
}

void ImGuiLayer::render(VkCommandBuffer commandBuffer) {
  ImGui::Render();
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}

void ImGuiLayer::shutdown() {
  if (!initialized) return;

  vkDeviceWaitIdle(device.device());
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  if (imguiPool != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(device.device(), imguiPool, nullptr);
    imguiPool = VK_NULL_HANDLE;
  }

  initialized = false;
}

} // namespace lve
