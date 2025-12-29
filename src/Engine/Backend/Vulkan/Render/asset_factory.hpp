#pragma once

#include "Engine/Backend/render_assets.hpp"
#include "Engine/Backend/Vulkan/Core/device.hpp"

#include <memory>
#include <string>

namespace lve::backend {

  class VulkanRenderAssetFactory final : public RenderAssetFactory {
  public:
    explicit VulkanRenderAssetFactory(LveDevice &device);

    std::shared_ptr<RenderModel> loadModel(const std::string &path) override;
    std::shared_ptr<RenderMaterial> loadMaterial(
      const std::string &path,
      std::string *outError = nullptr,
      const std::function<std::string(const std::string &)> &pathResolver = {}) override;
    std::shared_ptr<RenderMaterial> createMaterial() override;
    bool saveMaterial(
      const std::string &path,
      const MaterialData &data,
      std::string *outError = nullptr) override;
    std::shared_ptr<RenderTexture> loadTexture(const std::string &path) override;
    std::shared_ptr<RenderTexture> getDefaultTexture() override;

  private:
    LveDevice &device;
    std::shared_ptr<RenderTexture> defaultTexture;
  };

} // namespace lve::backend
