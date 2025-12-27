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
    glm::ivec4 flags0{0}; // useTexture, currentFrame, objectState, direction
    glm::ivec4 flags1{0}; // debugView, padding
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
      const auto bufferInfo = obj.getBufferInfo(frameIndex);

      obj.model->bind(frameInfo.commandBuffer);

      const auto &nodes = obj.model->getNodes();
      if (nodes.empty()) {
        VkDescriptorSet &gameObjectDescriptorSet = obj.descriptorSets[frameIndex];
        const LveTexture *currentTexture = obj.diffuseMap.get();
        if (gameObjectDescriptorSet == VK_NULL_HANDLE || obj.descriptorTextures[frameIndex] != currentTexture) {
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
        push.modelMatrix = obj.transform.mat4();
        push.flags0 = glm::ivec4(
          obj.enableTextureType,
          obj.currentFrame,
          static_cast<int>(obj.objState),
          static_cast<int>(obj.directions));
        push.flags1 = glm::ivec4(normalViewEnabled ? 1 : 0, 0, 0, 0);

        vkCmdPushConstants(
          frameInfo.commandBuffer,
          pipelineLayout,
          VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
          0,
          sizeof(SimplePushConstantData),
          &push);

        obj.model->draw(frameInfo.commandBuffer);
        continue;
      }

      std::vector<glm::mat4> localOverrides(nodes.size(), glm::mat4(1.f));
      if (obj.nodeOverrides.size() == nodes.size()) {
        for (std::size_t i = 0; i < nodes.size(); ++i) {
          const auto &override = obj.nodeOverrides[i];
          if (override.enabled) {
            localOverrides[i] = override.transform.mat4();
          }
        }
      }

      std::vector<glm::mat4> nodeGlobals;
      obj.model->computeNodeGlobals(localOverrides, nodeGlobals);

      const glm::mat4 objectTransform = obj.transform.mat4();
      const auto &subMeshes = obj.model->getSubMeshes();
      if (obj.subMeshDescriptors.size() != subMeshes.size()) {
        obj.subMeshDescriptors.assign(subMeshes.size(), {});
      }
      for (std::size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex) {
        const auto &node = nodes[nodeIndex];
        if (node.meshes.empty()) {
          continue;
        }

        const glm::mat4 modelMatrix = objectTransform * nodeGlobals[nodeIndex];

        for (int meshIndex : node.meshes) {
          if (meshIndex < 0 || static_cast<std::size_t>(meshIndex) >= subMeshes.size()) {
            continue;
          }
          const auto &subMesh = subMeshes[static_cast<std::size_t>(meshIndex)];
          const LveTexture *subMeshTexture = obj.model->getDiffuseTextureForSubMesh(subMesh);
          const LveTexture *currentTexture = subMeshTexture ? subMeshTexture : obj.diffuseMap.get();
          const int useTexture =
            (obj.enableTextureType && subMeshTexture != nullptr) ? 1 : 0;
          auto &cache = obj.subMeshDescriptors[static_cast<std::size_t>(meshIndex)];
          VkDescriptorSet &descriptorSet = cache.sets[frameIndex];
          if (descriptorSet == VK_NULL_HANDLE || cache.textures[frameIndex] != currentTexture) {
            auto imageInfo = currentTexture->getImageInfo();
            LveDescriptorWriter writer(*renderSystemLayout, frameInfo.frameDescriptorPool);
            writer.writeBuffer(0, &bufferInfo)
              .writeImage(1, &imageInfo);
            if (descriptorSet == VK_NULL_HANDLE) {
              if (!writer.build(descriptorSet)) {
                throw std::runtime_error("failed to build submesh descriptor set");
              }
            } else {
              writer.overwrite(descriptorSet);
            }
            cache.textures[frameIndex] = currentTexture;
          }

          vkCmdBindDescriptorSets(
            frameInfo.commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout,
            1,
            1,
            &descriptorSet,
            0,
            nullptr);

          SimplePushConstantData push{};
          push.modelMatrix = modelMatrix;
          push.flags0 = glm::ivec4(
            useTexture,
            obj.currentFrame,
            static_cast<int>(obj.objState),
            static_cast<int>(obj.directions));
          push.flags1 = glm::ivec4(normalViewEnabled ? 1 : 0, 0, 0, 0);

          vkCmdPushConstants(
            frameInfo.commandBuffer,
            pipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(SimplePushConstantData),
            &push);

          obj.model->drawSubMesh(frameInfo.commandBuffer, subMesh);
        }
      }
    }
  }
} // namespace lve




