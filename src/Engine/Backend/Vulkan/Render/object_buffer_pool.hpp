#pragma once

#include "Engine/Backend/object_buffer.hpp"
#include "Engine/Backend/Vulkan/Core/buffer.hpp"
#include "Engine/Backend/Vulkan/Core/device.hpp"

#include <memory>
#include <vector>

namespace lve::backend {

  class VulkanObjectBufferPool final : public ObjectBufferPool {
  public:
    VulkanObjectBufferPool(LveDevice &device, std::size_t objectCount, std::size_t objectSize);

    BufferInfo getBufferInfo(int frameIndex, std::size_t index) const override;
    void writeToIndex(const void *data, std::size_t index) override;
    void flush() override;

  private:
    std::vector<std::unique_ptr<LveBuffer>> buffers;
  };

} // namespace lve::backend
