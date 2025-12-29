#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace lve::backend {

  struct ModelSubMesh {
    std::uint32_t firstIndex{0};
    std::uint32_t indexCount{0};
    int materialIndex{-1};
    glm::vec3 boundsMin{0.f};
    glm::vec3 boundsMax{0.f};
    bool hasBounds{false};
  };

  struct ModelTextureSource {
    enum class Kind { None, File, EmbeddedCompressed, EmbeddedRaw };
    Kind kind{Kind::None};
    std::string path{};
    std::vector<unsigned char> data{};
    int width{0};
    int height{0};
  };

  struct ModelMaterialSource {
    ModelTextureSource diffuse{};
  };

  struct MaterialPathInfo {
    ModelTextureSource::Kind diffuseKind{ModelTextureSource::Kind::None};
    std::string diffusePath{};
  };

  struct ModelNode {
    std::string name{};
    int parent{-1};
    std::vector<int> children{};
    std::vector<int> meshes{};
    glm::mat4 localTransform{1.f};
  };

  struct ModelBoundingBox {
    glm::vec3 min{};
    glm::vec3 max{};

    glm::vec3 center() const { return (min + max) * 0.5f; }
    glm::vec3 halfSize() const { return (max - min) * 0.5f; }
  };

} // namespace lve::backend
