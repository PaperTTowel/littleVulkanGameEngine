#pragma once

#include "Engine/Backend/render_types.hpp"

#include <cstddef>
#include <memory>

namespace lve::backend {

  class ObjectBufferPool {
  public:
    virtual ~ObjectBufferPool() = default;

    virtual BufferInfo getBufferInfo(int frameIndex, std::size_t index) const = 0;
    virtual void writeToIndex(const void *data, std::size_t index) = 0;
    virtual void flush() = 0;
  };

  using ObjectBufferPoolPtr = std::unique_ptr<ObjectBufferPool>;

} // namespace lve::backend
