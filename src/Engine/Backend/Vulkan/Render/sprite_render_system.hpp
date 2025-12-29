#pragma once

#include "Engine/camera.hpp"
#include "utils/game_object.hpp"
#include "Engine/Backend/Vulkan/Core/pipeline.hpp"
#include "Engine/Backend/Vulkan/Render/frame_info.hpp"
#include "Engine/Backend/Vulkan/Core/device.hpp"

// std
#include <memory>
#include <vector>

namespace lve {
  class SpriteRenderSystem {
  public:
    SpriteRenderSystem(LveDevice &device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
    ~SpriteRenderSystem();

    SpriteRenderSystem(const SpriteRenderSystem &) = delete;
    SpriteRenderSystem &operator=(const SpriteRenderSystem &) = delete;

    void renderSprites(FrameInfo &frameInfo);
    void setBillboardMode(BillboardMode mode) { billboardMode = mode; }

  private:
    void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
    void createPipelines(VkRenderPass renderPass);

    LveDevice &lveDevice;
    VkRenderPass renderPass;
    std::unique_ptr<LvePipeline> spritePipeline;
    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};

    std::unique_ptr<LveDescriptorSetLayout> renderSystemLayout;
    BillboardMode billboardMode{BillboardMode::None};
  };
} // namespace lve


