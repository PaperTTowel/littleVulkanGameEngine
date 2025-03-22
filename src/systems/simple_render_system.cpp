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
    glm::mat4 modelMatrix{1.f};
    glm::mat4 normalMatrix{1.f};
    int useTexture; // 0 = 꺼짐, 1 = 스프라이트 형식의 텍스쳐(애니메이션 활성화), 2 = 스프라이트 형식이 아닌 텍스쳐
    int currentFrame;
    int objectState;
    int direction;
  };

  SimpleRenderSystem::SimpleRenderSystem(LveDevice &device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout) : lveDevice{device} {
    createPipelineLayout(globalSetLayout);
    createPipeline(renderPass);
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

  void SimpleRenderSystem::createPipeline(VkRenderPass renderPass) {
    assert(pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout!");

    PipelineConfigInfo pipelineConfig{};
    LvePipeline::defaultPipelineConfigInfo(pipelineConfig);
    pipelineConfig.renderPass = renderPass;
    pipelineConfig.pipelineLayout = pipelineLayout;
    lvePipeline = std::make_unique<LvePipeline>(
      lveDevice,
      "shaders/simple_shader.vert.spv",
      "shaders/simple_shader.frag.spv",
      pipelineConfig);
  }

  void SimpleRenderSystem::renderGameObjects(FrameInfo &frameInfo) {
    lvePipeline->bind(frameInfo.commandBuffer);

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
      if (obj.model == nullptr) continue;

      // writing descriptor set each frame can slow performance
      // would be more efficient to implement some sort of caching
      auto bufferInfo = obj.getBufferInfo(frameInfo.frameIndex);
      auto imageInfo = obj.diffuseMap->getImageInfo();
      VkDescriptorSet gameObjectDescriptorSet;
      LveDescriptorWriter(*renderSystemLayout, frameInfo.frameDescriptorPool)
        .writeBuffer(0, &bufferInfo)
        .writeImage(1, &imageInfo)
        .build(gameObjectDescriptorSet);

      vkCmdBindDescriptorSets(
        frameInfo.commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout,
        1, // starting set (0 is the globalDescriptorSet, 1 is the set specific to this system)
        1, // set count
        &gameObjectDescriptorSet,
        0,
        nullptr);
      
      // GLSL로 전달
      SimplePushConstantData push{};
      push.modelMatrix = obj.transform.mat4();
      push.normalMatrix = obj.transform.normalMatrix();
      push.useTexture = obj.enableTextureType;
      push.currentFrame = obj.currentFrame;
      push.objectState = static_cast<int>(obj.objState);
      push.direction = static_cast<int>(obj.directions);

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