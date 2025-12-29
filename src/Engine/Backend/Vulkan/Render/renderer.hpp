#pragma once

#include "Engine/Backend/Vulkan/Core/device.hpp"
#include "Engine/Backend/Vulkan/Core/window.hpp"
#include "Engine/Backend/Vulkan/Core/swap_chain.hpp"

// std
#include <cassert>
#include <memory>
#include <vector>

namespace lve{
    class LveRenderer{
    public:
        LveRenderer(LveWindow &window, LveDevice &device);
        ~LveRenderer();

        LveRenderer(const LveWindow &) = delete;
        LveRenderer &operator=(const LveWindow &) = delete;

        VkRenderPass getSwapChainRenderPass() const { return lveSwapChain->getRenderPass(); }
        float getAspectRatio() const { return lveSwapChain->extentAspectRatio(); }
        VkExtent2D getSwapChainExtent() const { return lveSwapChain->getSwapChainExtent(); }
        VkFormat getSwapChainImageFormat() const { return lveSwapChain->getSwapChainImageFormat(); }
        size_t getSwapChainImageCount() const { return lveSwapChain->imageCount(); }
        bool isFrameInProgress() const { return isFrameStarted; }

        VkCommandBuffer getCurrentCommandBuffer() const {
            assert(isFrameStarted && "Cannot get command buffer when frame not in progress");
            return commandBuffers[currentFrameIndex];
        }

        int getFrameindex() const{
            assert(isFrameStarted && "Cannot get frame index when frame not in progress");
            return currentFrameIndex;
        }

        VkCommandBuffer beginFrame();
        void endFrame();
        void beginSwapChainRenderPass(VkCommandBuffer commandBuffer);
        void endSwapChainRenderPass(VkCommandBuffer commandBuffer);
        bool wasSwapChainRecreated();

    private:
        void createCommandBuffers();
        void freeCommandBuffers();
        void recreateSwapChain();

        LveWindow& lveWindow;
        LveDevice& lveDevice;
        std::unique_ptr<LveSwapChain> lveSwapChain;
        std::vector<VkCommandBuffer> commandBuffers;

        uint32_t currentImageIndex;
        int currentFrameIndex{0};
        bool isFrameStarted{false};
        bool swapChainRecreated{false};
    };
} // namespace lve

