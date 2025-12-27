#include "simple_render_system.hpp"

// libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

// std
#include <array>
#include <cassert>
#include <stdexcept>

#include <iostream>

namespace lve {

  struct SimplePushConstantData {
    int useTexture;
    int currentFrame;
    int objectState;
    int direction;
    int debugView; // 1 = show normals, 0 = normal shading
  };

  SimpleRenderSystem::SimpleRenderSystem(LveDevice &device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout)
    : lveDevice{device}, renderPass{renderPass} {
    createPipelineLayout(globalSetLayout);
    createPipelines(renderPass);
  }

  SimpleRenderSystem::~SimpleRenderSystem() {
    vkDestroyPipelineLayout(lveDevice.device(), pipelineLayout, nullptr);
  }

  void SimpleRenderSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout) {
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(SimplePushConstantData);

    renderSystemLayout =
      LveDescriptorSetLayout::Builder(lveDevice)
        .addBinding(
        0,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
        .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
        .build();

    std::vector<VkDescriptorSetLayout> descriptorSetLayouts{
      globalSetLayout,
      renderSystemLayout->getDescriptorSetLayout()};

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    if (vkCreatePipelineLayout(lveDevice.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
      throw std::runtime_error("failed to create pipeline layout!");
    }
  }

  void SimpleRenderSystem::createPipelines(VkRenderPass renderPass) {
    assert(pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout!");

    PipelineConfigInfo fillConfig{};
    LvePipeline::defaultPipelineConfigInfo(fillConfig);
    fillConfig.renderPass = renderPass;
    fillConfig.pipelineLayout = pipelineLayout;
    fillPipeline = std::make_unique<LvePipeline>(
      lveDevice,
      "Shaders/simple_shader.vert.spv",
      "Shaders/simple_shader.frag.spv",
      fillConfig);

    PipelineConfigInfo wireConfig{};
    LvePipeline::defaultPipelineConfigInfo(wireConfig);
    wireConfig.renderPass = renderPass;
    wireConfig.pipelineLayout = pipelineLayout;
    wireConfig.rasterizationInfo.polygonMode = VK_POLYGON_MODE_LINE;
    wireConfig.rasterizationInfo.lineWidth = 1.0f;
    wireframePipeline = std::make_unique<LvePipeline>(
      lveDevice,
      "Shaders/simple_shader.vert.spv",
      "Shaders/simple_shader.frag.spv",
      wireConfig);
  }

  void SimpleRenderSystem::setWireframe(bool enabled) {
    wireframeEnabled = enabled;
  }

  void SimpleRenderSystem::renderGameObjects(FrameInfo &frameInfo) {
    LvePipeline* activePipeline =
      (wireframeEnabled && wireframePipeline) ? wireframePipeline.get() : fillPipeline.get();
    activePipeline->bind(frameInfo.commandBuffer);

    vkCmdBindDescriptorSets(
      frameInfo.commandBuffer,
      VK_PIPELINE_BIND_POINT_GRAPHICS,
      pipelineLayout,
      0,
      1,
      &frameInfo.globalDescriptorSet,
      0,
      nullptr);

    for (auto &kv : frameInfo.gameObjects) {
      auto &obj = kv.second;
      if (obj.isSprite) continue;
      if (obj.model == nullptr) continue;

      const int frameIndex = frameInfo.frameIndex;
      VkDescriptorSet &gameObjectDescriptorSet = obj.descriptorSets[frameIndex];
      const LveTexture *currentTexture = obj.diffuseMap.get();
      if (gameObjectDescriptorSet == VK_NULL_HANDLE || obj.descriptorTextures[frameIndex] != currentTexture) {
        auto bufferInfo = obj.getBufferInfo(frameIndex);
        auto imageInfo = obj.diffuseMap->getImageInfo();
        LveDescriptorWriter writer(*renderSystemLayout, frameInfo.frameDescriptorPool);
        writer.writeBuffer(0, &bufferInfo)
          .writeImage(1, &imageInfo);
        if (gameObjectDescriptorSet == VK_NULL_HANDLE) {
          if (!writer.build(gameObjectDescriptorSet)) {
            throw std::runtime_error("failed to build game object descriptor set");
          }
        } else {
          writer.overwrite(gameObjectDescriptorSet);
        }
        obj.descriptorTextures[frameIndex] = currentTexture;
      }

      vkCmdBindDescriptorSets(
        frameInfo.commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout,
        1, // starting set (0 is the globalDescriptorSet, 1 is the set specific to this system)
        1, // set count
        &gameObjectDescriptorSet,
        0,
        nullptr);
      
      SimplePushConstantData push{};
      push.useTexture = obj.enableTextureType;
      push.currentFrame = obj.currentFrame;
      push.objectState = static_cast<int>(obj.objState);
      push.direction = static_cast<int>(obj.directions);
      push.debugView = normalViewEnabled ? 1 : 0;

      vkCmdPushConstants(
        frameInfo.commandBuffer,
        pipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(SimplePushConstantData),
        &push);

      obj.model->bind(frameInfo.commandBuffer);
      obj.model->draw(frameInfo.commandBuffer);
    }
  }
} // namespace lve




