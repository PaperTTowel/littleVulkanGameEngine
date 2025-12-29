#include "simple_render_system.hpp"

#include "Engine/Backend/Vulkan/Render/model.hpp"
#include "Engine/Backend/Vulkan/Render/texture.hpp"

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
    glm::vec4 baseColorFactor{1.f};
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

    for (auto *objPtr : frameInfo.gameObjects) {
      if (!objPtr) continue;
      auto &obj = *objPtr;
      if (obj.isSprite) continue;
      if (obj.model == nullptr) continue;
      auto *model = static_cast<LveModel*>(obj.model.get());
      if (!model) continue;

      const int frameIndex = frameInfo.frameIndex;
      const auto bufferInfo = obj.getBufferInfo(frameIndex);
      VkDescriptorBufferInfo vkBufferInfo{};
      vkBufferInfo.buffer = reinterpret_cast<VkBuffer>(bufferInfo.buffer);
      vkBufferInfo.offset = bufferInfo.offset;
      vkBufferInfo.range = bufferInfo.range;
      const LveTexture *overrideTexture = obj.material
        ? static_cast<const LveTexture*>(obj.material->getBaseColorTexture())
        : nullptr;
      const bool hasOverrideTexture = overrideTexture != nullptr;
      const glm::vec4 baseColorFactor = obj.material
        ? obj.material->getData().factors.baseColor
        : glm::vec4(1.f);

      model->bind(frameInfo.commandBuffer);

      const auto &nodes = obj.model->getNodes();
      if (nodes.empty()) {
        auto &descriptorHandle = obj.descriptorSets[frameIndex];
        VkDescriptorSet gameObjectDescriptorSet = reinterpret_cast<VkDescriptorSet>(descriptorHandle);
        const LveTexture *currentTexture = hasOverrideTexture
          ? overrideTexture
          : static_cast<const LveTexture*>(obj.diffuseMap.get());
        if (!currentTexture) {
          continue;
        }
        if (gameObjectDescriptorSet == VK_NULL_HANDLE ||
            obj.descriptorTextures[frameIndex] != currentTexture) {
          auto imageInfo = currentTexture->getImageInfo();
          LveDescriptorWriter writer(*renderSystemLayout, frameInfo.frameDescriptorPool);
          writer.writeBuffer(0, &vkBufferInfo)
            .writeImage(1, &imageInfo);
          if (gameObjectDescriptorSet == VK_NULL_HANDLE) {
            if (!writer.build(gameObjectDescriptorSet)) {
              throw std::runtime_error("failed to build game object descriptor set");
            }
          } else {
            writer.overwrite(gameObjectDescriptorSet);
          }
          descriptorHandle = reinterpret_cast<backend::DescriptorSetHandle>(gameObjectDescriptorSet);
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
          hasOverrideTexture ? 1 : obj.enableTextureType,
          obj.currentFrame,
          static_cast<int>(obj.objState),
          static_cast<int>(obj.directions));
        push.flags1 = glm::ivec4(normalViewEnabled ? 1 : 0, 0, 0, 0);
        push.baseColorFactor = baseColorFactor;

        vkCmdPushConstants(
          frameInfo.commandBuffer,
          pipelineLayout,
          VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
          0,
          sizeof(SimplePushConstantData),
          &push);

        model->draw(frameInfo.commandBuffer);
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
          const LveTexture *subMeshTexture = static_cast<const LveTexture*>(
            obj.model->getDiffuseTextureForSubMesh(subMesh));
          const LveTexture *currentTexture = hasOverrideTexture
            ? overrideTexture
            : (subMeshTexture ? subMeshTexture : static_cast<const LveTexture*>(obj.diffuseMap.get()));
          if (!currentTexture) {
            continue;
          }
          const int useTexture = hasOverrideTexture
            ? 1
            : ((obj.enableTextureType && subMeshTexture != nullptr) ? 1 : 0);
          auto &cache = obj.subMeshDescriptors[static_cast<std::size_t>(meshIndex)];
          auto &descriptorHandle = cache.sets[frameIndex];
          VkDescriptorSet descriptorSet = reinterpret_cast<VkDescriptorSet>(descriptorHandle);
          if (descriptorSet == VK_NULL_HANDLE || cache.textures[frameIndex] != currentTexture) {
            auto imageInfo = currentTexture->getImageInfo();
            LveDescriptorWriter writer(*renderSystemLayout, frameInfo.frameDescriptorPool);
            writer.writeBuffer(0, &vkBufferInfo)
              .writeImage(1, &imageInfo);
            if (descriptorSet == VK_NULL_HANDLE) {
              if (!writer.build(descriptorSet)) {
                throw std::runtime_error("failed to build submesh descriptor set");
              }
            } else {
              writer.overwrite(descriptorSet);
            }
            descriptorHandle = reinterpret_cast<backend::DescriptorSetHandle>(descriptorSet);
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
          push.baseColorFactor = baseColorFactor;

          vkCmdPushConstants(
            frameInfo.commandBuffer,
            pipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(SimplePushConstantData),
            &push);

          model->drawSubMesh(frameInfo.commandBuffer, subMesh);
        }
      }
    }
  }
} // namespace lve




