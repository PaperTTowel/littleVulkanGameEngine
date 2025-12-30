#include "Engine/Backend/Factory/runtime_backend_factory.hpp"

#include "Engine/Backend/Vulkan/runtime_backend.hpp"

namespace lve::backend {
  std::unique_ptr<RuntimeBackend> createRuntimeBackend(const RuntimeBackendConfig &config) {
    switch (config.api) {
      case BackendApi::Vulkan:
        return std::make_unique<VulkanRuntimeBackend>(config.width, config.height, config.title);
      default:
        return {};
    }
  }
} // namespace lve::backend
