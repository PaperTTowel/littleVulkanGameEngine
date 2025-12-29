#pragma once

#include "Engine/Backend/Vulkan/Core/device.hpp"
#include "Engine/Backend/Vulkan/Core/window.hpp"
#include "Engine/Backend/Vulkan/Core/swap_chain.hpp"

#include <vulkan/vulkan.h>
#include <glm/vec3.hpp>
#include <array>

namespace lve {
class ImGuiLayer {
public:
  ImGuiLayer(LveWindow &window, LveDevice &device);
  ~ImGuiLayer();

  void init(VkRenderPass renderPass, uint32_t imageCount = LveSwapChain::MAX_FRAMES_IN_FLIGHT);
  void newFrame();
  void buildUI(float frameTime, const glm::vec3 &cameraPos, const glm::vec3 &cameraRot, bool &wireframeEnabled, bool &normalViewEnabled, bool &useOrthoCamera, bool &showEngineStats);
  void render(VkCommandBuffer commandBuffer);
  void renderPlatformWindows();
  void shutdown();

private:
  void createDescriptorPool(uint32_t imageCount);

  LveWindow &window;
  LveDevice &device;
  VkDescriptorPool imguiPool{VK_NULL_HANDLE};
  bool initialized{false};

  std::array<float, 120> frameTimeHistory{};
  size_t frameTimeOffset{0};
};
} // namespace lve

