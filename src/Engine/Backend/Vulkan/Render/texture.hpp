#pragma once

#include "Engine/Backend/render_assets.hpp"
#include "Engine/Backend/Vulkan/Core/device.hpp"

// libs
#include <vulkan/vulkan.h>

// std
#include <memory>

namespace lve {
class LveTexture : public backend::RenderTexture {
 public:
  LveTexture(LveDevice &device, const unsigned char *rgbaPixels, int width, int height);
  LveTexture(
      LveDevice &device,
      VkFormat format,
      VkExtent3D extent,
      VkImageUsageFlags usage,
      VkSampleCountFlagBits sampleCount);
  ~LveTexture();

  // delete copy constructors
  LveTexture(const LveTexture &) = delete;
  LveTexture &operator=(const LveTexture &) = delete;

  VkImageView imageView() const { return mTextureImageView; }
  VkSampler sampler() const { return mTextureSampler; }
  VkImage getImage() const { return mTextureImage; }
  VkImageView getImageView() const { return mTextureImageView; }
  VkDescriptorImageInfo getImageInfo() const { return mDescriptor; }
  VkImageLayout getImageLayout() const { return mTextureLayout; }
  VkExtent3D getExtent() const { return mExtent; }
  VkFormat getFormat() const { return mFormat; }

  void updateDescriptor();
  void transitionLayout(
      VkCommandBuffer commandBuffer, VkImageLayout oldLayout, VkImageLayout newLayout);

  static std::unique_ptr<LveTexture> createTextureFromRgba(
      LveDevice &device, const unsigned char *rgbaPixels, int width, int height);

 private:
  void createTextureImageFromPixels(const unsigned char *pixels, int texWidth, int texHeight);
  void createTextureImageView(VkImageViewType viewType);
  void createTextureSampler();

  VkDescriptorImageInfo mDescriptor{};

  LveDevice &mDevice;
  VkImage mTextureImage = nullptr;
  VkDeviceMemory mTextureImageMemory = nullptr;
  VkImageView mTextureImageView = nullptr;
  VkSampler mTextureSampler = nullptr;
  VkFormat mFormat;
  VkImageLayout mTextureLayout;
  uint32_t mMipLevels{1};
  uint32_t mLayerCount{1};
  VkExtent3D mExtent{};
};

}  // namespace lve

