#pragma once

#include "Engine/Backend/runtime_backend.hpp"

#include <memory>
#include <string>

namespace lve::backend {
  enum class BackendApi {
    Vulkan
  };

  struct RuntimeBackendConfig {
    BackendApi api{BackendApi::Vulkan};
    int width{0};
    int height{0};
    std::string title{};
  };

  std::unique_ptr<RuntimeBackend> createRuntimeBackend(const RuntimeBackendConfig &config);
} // namespace lve::backend
