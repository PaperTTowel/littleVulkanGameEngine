#pragma once

#include "Engine/camera.hpp"
#include "utils/game_object.hpp"
#include "Engine/Backend/pipeline.hpp"
#include "Rendering/frame_info.hpp"
#include "Engine/Backend/device.hpp"

// std
#include <memory>
#include <vector>

namespace lve {
  class SimpleRenderSystem {
  public:
    SimpleRenderSystem(LveDevice &device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
    ~SimpleRenderSystem();

    SimpleRenderSystem(const LveWindow &) = delete;
    SimpleRenderSystem &operator=(const LveWindow &) = delete;

    void renderGameObjects(FrameInfo &frameInfo);

  private:
    void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
    void createPipeline(VkRenderPass renderPass);

    LveDevice &lveDevice;
    std::unique_ptr<LvePipeline> lvePipeline;
    VkPipelineLayout pipelineLayout;

    std::unique_ptr<LveDescriptorSetLayout> renderSystemLayout;
  };
} // namespace lve