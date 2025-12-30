#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace lve::backend {

  struct ModelVertex {
    glm::vec3 position{};
    glm::vec3 color{};
    glm::vec3 normal{};
    glm::vec2 uv{};

    bool operator==(const ModelVertex &other) const {
      return position == other.position && color == other.color && normal == other.normal && uv == other.uv;
    }
  };

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

  struct ModelData {
    std::vector<ModelVertex> vertices{};
    std::vector<std::uint32_t> indices{};
    std::vector<ModelSubMesh> subMeshes{};
    std::vector<ModelNode> nodes{};
    std::vector<ModelMaterialSource> materials{};
  };

} // namespace lve::backend
