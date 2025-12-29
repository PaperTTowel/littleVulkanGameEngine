#pragma once

#include "Engine/Backend/render_assets.hpp"
#include "Engine/Backend/Vulkan/Core/buffer.hpp"
#include "Engine/Backend/Vulkan/Core/device.hpp"

// libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

// std
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace lve {
  class LveTexture;

  class LveModel : public backend::RenderModel {
  public:
    using SubMesh = backend::ModelSubMesh;
    using TextureSource = backend::ModelTextureSource;
    using MaterialSource = backend::ModelMaterialSource;
    using MaterialPathInfo = backend::MaterialPathInfo;
    using Node = backend::ModelNode;
    using BoundingBox = backend::ModelBoundingBox;

    struct Vertex {
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

    struct Builder {
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
    void computeNodeGlobals(
      const std::vector<glm::mat4> &localOverrides,
      std::vector<glm::mat4> &outGlobals) const override;

    const std::vector<Node> &getNodes() const override { return nodes; }
    const std::vector<SubMesh> &getSubMeshes() const override { return subMeshes; }
    const std::vector<MaterialPathInfo> &getMaterialPathInfo() const override { return materialPathInfo; }
    std::string getDiffusePathForMaterialIndex(int materialIndex) const override;
    std::string getDiffusePathForSubMesh(const SubMesh &subMesh) const override;
    const backend::RenderTexture *getDiffuseTextureForSubMesh(const SubMesh &subMesh) const override;
    bool hasAnyDiffuseTexture() const override;

    const BoundingBox &getBoundingBox() const override { return boundingBox; }

  private:
    void createVertexBuffers(const std::vector<Vertex> &vertices);
    void createIndexBuffers(const std::vector<uint32_t> &indices);

    void calculateBoundingBox(const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices);
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
    std::vector<MaterialPathInfo> materialPathInfo;
  };
} // namespace lve
