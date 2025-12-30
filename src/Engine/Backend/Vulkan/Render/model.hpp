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

    LveModel(
      LveDevice &device,
      const backend::ModelData &data,
      std::vector<std::shared_ptr<LveTexture>> materialTextures);
    ~LveModel();

    LveModel(const LveModel &) = delete;
    LveModel &operator=(const LveModel &) = delete;

    static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();

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
    void createVertexBuffers(const std::vector<backend::ModelVertex> &vertices);
    void createIndexBuffers(const std::vector<uint32_t> &indices);

    void calculateBoundingBox(const std::vector<backend::ModelVertex> &vertices, const std::vector<uint32_t> &indices);

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
