#include "Engine/Backend/Vulkan/Render/object_buffer_pool.hpp"

#include "Engine/Backend/render_types.hpp"

#include <numeric>

namespace lve::backend {

  VulkanObjectBufferPool::VulkanObjectBufferPool(
    LveDevice &device,
    std::size_t objectCount,
    std::size_t objectSize) {
    const auto atomSize = static_cast<std::size_t>(device.properties.limits.nonCoherentAtomSize);
    const auto minAlignment = static_cast<std::size_t>(device.properties.limits.minUniformBufferOffsetAlignment);
    const auto alignment = static_cast<int>(std::lcm(atomSize, minAlignment));

    buffers.resize(kMaxFramesInFlight);
    for (std::size_t i = 0; i < buffers.size(); ++i) {
      buffers[i] = std::make_unique<LveBuffer>(
        device,
        static_cast<uint32_t>(objectSize),
        static_cast<uint32_t>(objectCount),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        alignment);
      buffers[i]->map();
    }
  }

  BufferInfo VulkanObjectBufferPool::getBufferInfo(int frameIndex, std::size_t index) const {
    if (frameIndex < 0 || static_cast<std::size_t>(frameIndex) >= buffers.size()) {
      return {};
    }
    auto info = buffers[static_cast<std::size_t>(frameIndex)]->descriptorInfoForIndex(static_cast<int>(index));
    BufferInfo result{};
    result.buffer = reinterpret_cast<std::uintptr_t>(info.buffer);
    result.offset = static_cast<std::uint64_t>(info.offset);
    result.range = static_cast<std::uint64_t>(info.range);
    return result;
  }

  void VulkanObjectBufferPool::writeToIndex(const void *data, std::size_t index) {
    for (auto &buffer : buffers) {
      buffer->writeToIndex(const_cast<void *>(data), static_cast<int>(index));
    }
  }

  void VulkanObjectBufferPool::flush() {
    for (auto &buffer : buffers) {
      buffer->flush();
    }
  }

} // namespace lve::backend
