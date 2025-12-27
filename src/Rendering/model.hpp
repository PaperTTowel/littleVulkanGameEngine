#pragma once

#include "Engine/Backend/buffer.hpp"
#include "Engine/Backend/device.hpp"

// libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

// std
#include <memory>
#include <vector>

namespace lve{
  class LveTexture;

  class LveModel{
  public:
    struct Vertex{
      glm::vec3 position{};
      glm::vec3 color{};
      glm::vec3 normal{};
      glm::vec2 uv{};

      static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
      static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();

      bool operator==(const Vertex &other) const {
        return position == other.position && color == other.color && normal == other.normal && uv == other.uv;
      }
    };

    struct SubMesh {
      uint32_t firstIndex{0};
      uint32_t indexCount{0};
      int materialIndex{-1};
      glm::vec3 boundsMin{0.f};
      glm::vec3 boundsMax{0.f};
      bool hasBounds{false};
    };

    struct TextureSource {
      enum class Kind { None, File, EmbeddedCompressed, EmbeddedRaw };
      Kind kind{Kind::None};
      std::string path{};
      std::vector<unsigned char> data{};
      int width{0};
      int height{0};
    };

    struct MaterialSource {
      TextureSource diffuse{};
    };

    struct Node {
      std::string name{};
      int parent{-1};
      std::vector<int> children{};
      std::vector<int> meshes{};
      glm::mat4 localTransform{1.f};
    };

    struct Builder{
      std::vector<Vertex> vertices{};
      std::vector<uint32_t> indices{};
      std::vector<SubMesh> subMeshes{};
      std::vector<Node> nodes{};
      std::vector<MaterialSource> materials{};

      void loadModel(const std::string &filepath);
    };

    LveModel(LveDevice &device, const LveModel::Builder &builder);
    ~LveModel();

    LveModel(const LveModel &) = delete;
    LveModel &operator=(const LveModel &) = delete;

    static std::unique_ptr<LveModel> createModelFromFile(LveDevice &device, const std::string &filepath);

    void bind(VkCommandBuffer commandBuffer);
    void draw(VkCommandBuffer commandBuffer);
    void drawSubMesh(VkCommandBuffer commandBuffer, const SubMesh &subMesh);
    void computeNodeGlobals(const std::vector<glm::mat4> &localOverrides, std::vector<glm::mat4> &outGlobals) const;

    const std::vector<Node> &getNodes() const { return nodes; }
    const std::vector<SubMesh> &getSubMeshes() const { return subMeshes; }
    const LveTexture *getDiffuseTextureForSubMesh(const SubMesh &subMesh) const;
    bool hasAnyDiffuseTexture() const;

    // physics_engine.cpp에서 충돌 크기를 사용하기 위해 사용
    struct BoundingBox {
      glm::vec3 min;
      glm::vec3 max;

      glm::vec3 center() const { return (min + max) * 0.5f; }
      glm::vec3 halfSize() const { return (max - min) * 0.5f; }
    };

    const BoundingBox& getBoundingBox() const { return boundingBox; }

  private:
    void createVertexBuffers(const std::vector<Vertex> &vertices);
    void createIndexBuffers(const std::vector<uint32_t> &indices);

    void calculateBoundingBox(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
    void loadMaterials(const std::vector<MaterialSource> &materials);

    LveDevice &lveDevice;

    std::unique_ptr<LveBuffer> vertexBuffer;
    uint32_t vertexCount;

    bool hasIndexBuffer = false;
    std::unique_ptr<LveBuffer> indexBuffer;
    uint32_t indexCount;

    BoundingBox boundingBox;
    std::vector<SubMesh> subMeshes;
    std::vector<Node> nodes;
    std::vector<std::shared_ptr<LveTexture>> materialDiffuseTextures;
  };
} // namespace lve
