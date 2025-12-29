#pragma once

#include <cstddef>
#include <cstdint>

namespace lve::backend {
  constexpr int kMaxFramesInFlight = 2;

  struct RenderExtent {
    std::uint32_t width{0};
    std::uint32_t height{0};
  };

  struct BufferInfo {
    std::uintptr_t buffer{0};
    std::uint64_t offset{0};
    std::uint64_t range{0};
  };

  using RenderPassHandle = void *;
  using CommandBufferHandle = void *;
  using DescriptorSetHandle = void *;
} // namespace lve::backend
