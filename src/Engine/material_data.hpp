#pragma once

#include <glm/glm.hpp>

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

} // namespace lve
