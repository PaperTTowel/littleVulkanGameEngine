#pragma once

#include "Engine/Backend/render_assets.hpp"
#include "Engine/Backend/Vulkan/Core/device.hpp"
#include "Engine/Backend/Vulkan/Render/texture.hpp"
#include "Engine/material_data.hpp"

#include <functional>
#include <memory>
#include <string>

namespace lve {

  class LveMaterial : public backend::RenderMaterial {
  public:
    explicit LveMaterial(LveDevice &device);

    static std::shared_ptr<LveMaterial> loadFromFile(
      LveDevice &device,
      const std::string &path,
      std::string *outError = nullptr,
      const std::function<std::string(const std::string &)> &pathResolver = {});

    bool applyData(
      const MaterialData &data,
      std::string *outError = nullptr,
      const std::function<std::string(const std::string &)> &pathResolver = {}) override;
    void setPath(const std::string &newPath) override { path = newPath; }

    const MaterialData &getData() const override { return data; }
    const std::string &getPath() const override { return path; }

    const backend::RenderTexture *getBaseColorTexture() const override { return baseColorTexture.get(); }
    bool hasBaseColorTexture() const override { return baseColorTexture != nullptr; }
    const backend::RenderTexture *getNormalTexture() const override { return normalTexture.get(); }
    const backend::RenderTexture *getMetallicRoughnessTexture() const override { return metallicRoughnessTexture.get(); }
    const backend::RenderTexture *getOcclusionTexture() const override { return occlusionTexture.get(); }
    const backend::RenderTexture *getEmissiveTexture() const override { return emissiveTexture.get(); }

  private:
    LveDevice &device;
    MaterialData data{};
    std::string path{};

    std::shared_ptr<LveTexture> baseColorTexture{};
    std::shared_ptr<LveTexture> normalTexture{};
    std::shared_ptr<LveTexture> metallicRoughnessTexture{};
    std::shared_ptr<LveTexture> occlusionTexture{};
    std::shared_ptr<LveTexture> emissiveTexture{};
  };

} // namespace lve
