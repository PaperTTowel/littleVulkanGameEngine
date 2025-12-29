#pragma once

#include "Engine/camera.hpp"
#include "Engine/Backend/descriptors.hpp"
#include "utils/game_object.hpp"

// lib
#include <vulkan/vulkan.h>
#include <vector>

namespace lve{

    #define MAX_LIGHTS 10

    struct PointLight{
        glm::vec4 position{}; // ignore w
        glm::vec4 color{};    // w is intensity
    };

    struct GlobalUbo{
        glm::mat4 projection{1.f};
        glm::mat4 view{1.f};
        glm::mat4 inverseView{1.f};
        glm::vec4 ambientLightColor{1.f, 1.f, 1.f, .02f};  // w is intensity
        PointLight pointLights[MAX_LIGHTS];
        int numLights;
    };

    struct FrameInfo{
        int frameIndex;
        float frameTime;
        VkCommandBuffer commandBuffer;
        LveCamera &camera;
        VkDescriptorSet globalDescriptorSet;
        LveDescriptorPool &frameDescriptorPool;  // pool for cached per-object descriptors
        std::vector<LveGameObject*> &gameObjects;
    };
} // namespace lve
