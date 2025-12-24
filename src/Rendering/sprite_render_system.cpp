#include "sprite_render_system.hpp"

// libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

// std
#include <array>
#include <cassert>
#include <stdexcept>

namespace lve {

  struct SpritePushConstantData {
    glm::mat4 modelMatrix{1.f};
    int useTexture;
    int currentFrame;
    int objectState;
    int direction;
    int debugView;
    int atlasCols;
    int atlasRows;
    int rowIndex;
  };

  SpriteRenderSystem::SpriteRenderSystem(LveDevice &device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout)
    : lveDevice{device}, renderPass{renderPass} {
    createPipelineLayout(globalSetLayout);
    createPipelines(renderPass);
  }

  SpriteRenderSystem::~SpriteRenderSystem() {
    if (pipelineLayout != VK_NULL_HANDLE) {
      vkDestroyPipelineLayout(lveDevice.device(), pipelineLayout, nullptr);
      pipelineLayout = VK_NULL_HANDLE;
    }
  }

  void SpriteRenderSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout) {
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(SpritePushConstantData);

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
      throw std::runtime_error("failed to create sprite pipeline layout!");
    }
  }

  void SpriteRenderSystem::createPipelines(VkRenderPass renderPass) {
    assert(pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout!");

    PipelineConfigInfo config{};
    LvePipeline::defaultPipelineConfigInfo(config);
    config.renderPass = renderPass;
    config.pipelineLayout = pipelineLayout;
    config.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;

    // enable alpha blending for sprites
    config.colorBlendAttachment.blendEnable = VK_TRUE;
    config.colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    config.colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    config.colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    config.colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    config.colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    config.colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    // sprites typically skip depth testing/writes so transparent regions don't occlude
    config.depthStencilInfo.depthTestEnable = VK_FALSE;
    config.depthStencilInfo.depthWriteEnable = VK_FALSE;

    spritePipeline = std::make_unique<LvePipeline>(
      lveDevice,
      "Shaders/sprite_shader.vert.spv",
      "Shaders/sprite_shader.frag.spv",
      config);
  }

  void SpriteRenderSystem::renderSprites(FrameInfo &frameInfo) {
    spritePipeline->bind(frameInfo.commandBuffer);

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
      if (!obj.isSprite) continue;
      if (obj.model == nullptr) continue;

      auto bufferInfo = obj.getBufferInfo(frameInfo.frameIndex);
      auto imageInfo = obj.diffuseMap->getImageInfo();
      VkDescriptorSet descriptorSet;
      LveDescriptorWriter(*renderSystemLayout, frameInfo.frameDescriptorPool)
        .writeBuffer(0, &bufferInfo)
        .writeImage(1, &imageInfo)
        .build(descriptorSet);

      vkCmdBindDescriptorSets(
        frameInfo.commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout,
        1,
        1,
        &descriptorSet,
        0,
        nullptr);

      glm::mat4 modelMat = obj.transform.mat4();
      if (billboardMode != BillboardMode::None) {
        // build billboard rotation using camera inverse view (world basis)
        glm::mat4 invView = frameInfo.camera.getInverseView();
        glm::vec3 camRight{invView[0][0], invView[0][1], invView[0][2]};
        glm::vec3 camUp{invView[1][0], invView[1][1], invView[1][2]};
        glm::vec3 camForward{invView[2][0], invView[2][1], invView[2][2]};
        // face camera: sprite forward should look toward camera -> opposite of camera forward
        camForward = -camForward;

        glm::vec3 right = glm::normalize(camRight);
        glm::vec3 up = (billboardMode == BillboardMode::Cylindrical) ? glm::vec3(0.f, 1.f, 0.f) : camUp;
        glm::vec3 forward = glm::normalize(camForward);

        // orthonormalize
        right = glm::normalize(glm::cross(forward, up));
        up = glm::normalize(glm::cross(right, forward));

        glm::mat4 rotation{1.f};
        rotation[0] = glm::vec4(right, 0.f);
        rotation[1] = glm::vec4(up, 0.f);
        rotation[2] = glm::vec4(forward, 0.f);

        glm::mat4 translate{1.f};
        translate[3] = glm::vec4(obj.transform.translation, 1.f);
        glm::mat4 scale = glm::scale(glm::mat4{1.f}, obj.transform.scale);
        modelMat = translate * rotation * scale;
      }

      SpritePushConstantData push{};
      push.modelMatrix = modelMat;
      push.useTexture = obj.enableTextureType;
      push.currentFrame = obj.currentFrame;
      push.objectState = static_cast<int>(obj.objState);
      push.direction = static_cast<int>(obj.directions);
      push.debugView = 0;
      push.atlasCols = obj.atlasColumns;
      push.atlasRows = obj.atlasRows;
      const int stateKey = static_cast<int>(obj.objState);
      auto stateIt = obj.spriteStates.find(stateKey);
      push.rowIndex = (stateIt != obj.spriteStates.end()) ? stateIt->second.row : 0;

      vkCmdPushConstants(
        frameInfo.commandBuffer,
        pipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(SpritePushConstantData),
        &push);

      obj.model->bind(frameInfo.commandBuffer);
      obj.model->draw(frameInfo.commandBuffer);
    }
  }
} // namespace lve
#include "Engine/camera.hpp"
