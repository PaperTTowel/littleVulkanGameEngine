#include "point_light_system.hpp"

// libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

// std
#include <array>
#include <cassert>
#include <algorithm>
#include <vector>
#include <stdexcept>

namespace lve{

    struct PointLightPushConstants{
        glm::vec4 position{};
        glm::vec4 color{};
        float radius;
    };

    PointLightSystem::PointLightSystem(LveDevice& device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout) : lveDevice{device}{
        createPipelineLayout(globalSetLayout);
        createPipeline(renderPass);
    }

    PointLightSystem::~PointLightSystem(){
        vkDestroyPipelineLayout(lveDevice.device(), pipelineLayout, nullptr);
    }

    void PointLightSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout){
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(PointLightPushConstants);

        std::vector<VkDescriptorSetLayout> descriptorSetLayouts{globalSetLayout};

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
        pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
        if(vkCreatePipelineLayout(lveDevice.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS){
            throw std::runtime_error("failed to create pipeline layout!");
        }
    }

    void PointLightSystem::update(FrameInfo& frameInfo, GlobalUbo &ubo){
        auto rotateLight = glm::rotate(
            glm::mat4(1.f),
            frameInfo.frameTime,
            {0.f, -1.f, 0.f}
        );

        int lightIndex = 0;
        for (auto *objPtr : frameInfo.gameObjects) {
            if (!objPtr) continue;
            auto &obj = *objPtr;
            if (obj.pointLight == nullptr) continue;

            assert(lightIndex < MAX_LIGHTS && "Point lights exceed maximum specified");

            // update light position
            obj.transform.translation = glm::vec3(rotateLight * glm::vec4(obj.transform.translation, 1.f));

            // copy light to ubo
            ubo.pointLights[lightIndex].position = glm::vec4(obj.transform.translation, 1.f);
            ubo.pointLights[lightIndex].color = glm::vec4(obj.color, obj.pointLight->lightIntensity);

            lightIndex += 1;
        }
        ubo.numLights = lightIndex;
    }

    void PointLightSystem::createPipeline(VkRenderPass renderPass){
        assert(pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout!");

        PipelineConfigInfo pipelineConfig{};
        LvePipeline::defaultPipelineConfigInfo(pipelineConfig);
        pipelineConfig.colorBlendAttachment.blendEnable = VK_TRUE;
        pipelineConfig.colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        pipelineConfig.colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        pipelineConfig.colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        pipelineConfig.colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        pipelineConfig.colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        pipelineConfig.colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        pipelineConfig.attributeDescriptions.clear();
        pipelineConfig.bindingDescriptions.clear();
        pipelineConfig.renderPass = renderPass;
        pipelineConfig.pipelineLayout = pipelineLayout;
        lvePipeline = std::make_unique<LvePipeline>(
            lveDevice,
            "Shaders/point_light.vert.spv",
            "Shaders/point_light.frag.spv",
            pipelineConfig);
    }

    void PointLightSystem::render(FrameInfo &frameInfo){
        // sort lights by distance (back-to-front)
        std::vector<LveGameObject*> sorted;
        sorted.reserve(frameInfo.gameObjects.size());
        for (auto *objPtr : frameInfo.gameObjects) {
            if (!objPtr) continue;
            if (objPtr->pointLight == nullptr) continue;
            sorted.push_back(objPtr);
        }
        std::sort(sorted.begin(), sorted.end(),
            [&](const auto *a, const auto *b) {
                const auto offsetA = frameInfo.camera.getPosition() - a->transform.translation;
                const auto offsetB = frameInfo.camera.getPosition() - b->transform.translation;
                float disSquaredA = glm::dot(offsetA, offsetA);
                float disSquaredB = glm::dot(offsetB, offsetB);
                if (disSquaredA != disSquaredB) return disSquaredA < disSquaredB;
                return a->getId() < b->getId();
            });

        lvePipeline->bind(frameInfo.commandBuffer);

        vkCmdBindDescriptorSets(
            frameInfo.commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout,
            0,
            1,
            &frameInfo.globalDescriptorSet,
            0,
            nullptr
        );

        // iterate through sorted lights in reverse order
        for (auto it = sorted.rbegin(); it != sorted.rend(); ++it) {
            auto *objPtr = *it;
            if (!objPtr) continue;
            auto &obj = *objPtr;

            PointLightPushConstants push{};
            push.position = glm::vec4(obj.transform.translation, 1.f);
            push.color = glm::vec4(obj.color, obj.pointLight->lightIntensity);
            push.radius = obj.transform.scale.x;

            vkCmdPushConstants(
                frameInfo.commandBuffer,
                pipelineLayout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                sizeof(PointLightPushConstants),
                &push
            );
            vkCmdDraw(frameInfo.commandBuffer, 6, 1, 0, 0);
        }
    }
} // namespace lve
