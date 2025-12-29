#pragma once

#include "Rendering/texture.hpp"

#include <glm/glm.hpp>

#include <functional>
#include <memory>
#include <string>

namespace lve {

  struct MaterialTexturePaths {
    std::string baseColor{};
    std::string normal{};
    std::string metallicRoughness{};
    std::string occlusion{};
    std::string emissive{};
  };

  struct MaterialFactors {
    glm::vec4 baseColor{1.f, 1.f, 1.f, 1.f};
    float metallic{1.f};
    float roughness{1.f};
    glm::vec3 emissive{0.f, 0.f, 0.f};
    float occlusionStrength{1.f};
    float normalScale{1.f};
  };

  struct MaterialData {
    int version{1};
    std::string name{};
    MaterialTexturePaths textures{};
    MaterialFactors factors{};
  };

  class LveMaterial {
  public:
    static std::shared_ptr<LveMaterial> loadFromFile(
      LveDevice &device,
      const std::string &path,
      std::string *outError = nullptr,
      const std::function<std::string(const std::string &)> &pathResolver = {});
    static bool saveToFile(
      const std::string &path,
      const MaterialData &data,
      std::string *outError = nullptr);

    bool applyData(
      LveDevice &device,
      const MaterialData &data,
      std::string *outError = nullptr,
      const std::function<std::string(const std::string &)> &pathResolver = {});
    void setPath(const std::string &newPath) { path = newPath; }

    const MaterialData &getData() const { return data; }
    const std::string &getPath() const { return path; }

    const std::shared_ptr<LveTexture> &getBaseColorTexture() const { return baseColorTexture; }
    const std::shared_ptr<LveTexture> &getNormalTexture() const { return normalTexture; }
    const std::shared_ptr<LveTexture> &getMetallicRoughnessTexture() const { return metallicRoughnessTexture; }
    const std::shared_ptr<LveTexture> &getOcclusionTexture() const { return occlusionTexture; }
    const std::shared_ptr<LveTexture> &getEmissiveTexture() const { return emissiveTexture; }

    bool hasBaseColorTexture() const { return baseColorTexture != nullptr; }

  private:
    MaterialData data{};
    std::string path{};

    std::shared_ptr<LveTexture> baseColorTexture{};
    std::shared_ptr<LveTexture> normalTexture{};
    std::shared_ptr<LveTexture> metallicRoughnessTexture{};
    std::shared_ptr<LveTexture> occlusionTexture{};
    std::shared_ptr<LveTexture> emissiveTexture{};
  };

} // namespace lve
