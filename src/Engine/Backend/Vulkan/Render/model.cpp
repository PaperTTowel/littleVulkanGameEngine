#include "Engine/Backend/Vulkan/Render/model.hpp"

#include "Engine/Backend/Vulkan/Render/texture.hpp"

// std
#include <cassert>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>

namespace lve {

  LveModel::LveModel(
    LveDevice &device,
    const backend::ModelData &data,
    std::vector<std::shared_ptr<LveTexture>> materialTextures)
    : lveDevice{device}
    , subMeshes{data.subMeshes}
    , nodes{data.nodes}
    , materialDiffuseTextures{std::move(materialTextures)} {
    createVertexBuffers(data.vertices);
    createIndexBuffers(data.indices);

    if (materialDiffuseTextures.size() < data.materials.size()) {
      materialDiffuseTextures.resize(data.materials.size());
    }

    materialPathInfo.clear();
    materialPathInfo.reserve(data.materials.size());
    for (const auto &material : data.materials) {
      MaterialPathInfo info{};
      info.diffuseKind = material.diffuse.kind;
      if (material.diffuse.kind == TextureSource::Kind::File) {
        info.diffusePath = material.diffuse.path;
      }
      materialPathInfo.push_back(std::move(info));
    }

    calculateBoundingBox(data.vertices, data.indices);
  }

  LveModel::~LveModel() {}

  void LveModel::createVertexBuffers(const std::vector<backend::ModelVertex> &vertices) {
    vertexCount = static_cast<uint32_t>(vertices.size());
    assert(vertexCount >= 3 && "Vertex count must be at least 3");
    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertexCount;
    uint32_t vertexSize = sizeof(vertices[0]);

    LveBuffer stagingBuffer{
        lveDevice,
        vertexSize,
        vertexCount,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    };

    stagingBuffer.map();
    stagingBuffer.writeToBuffer((void *)vertices.data());

    vertexBuffer = std::make_unique<LveBuffer>(
        lveDevice,
        vertexSize,
        vertexCount,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    lveDevice.copyBuffer(stagingBuffer.getBuffer(), vertexBuffer->getBuffer(), bufferSize);
  }

  void LveModel::createIndexBuffers(const std::vector<uint32_t> &indices) {
    indexCount = static_cast<uint32_t>(indices.size());
    hasIndexBuffer = indexCount > 0;

    if (!hasIndexBuffer) {
      return;
    }

    VkDeviceSize bufferSize = sizeof(indices[0]) * indexCount;
    uint32_t indexSize = sizeof(indices[0]);

    LveBuffer stagingBuffer{
        lveDevice,
        indexSize,
        indexCount,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    };

    stagingBuffer.map();
    stagingBuffer.writeToBuffer((void *)indices.data());

    indexBuffer = std::make_unique<LveBuffer>(
        lveDevice,
        indexSize,
        indexCount,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    lveDevice.copyBuffer(stagingBuffer.getBuffer(), indexBuffer->getBuffer(), bufferSize);
  }

  void LveModel::draw(VkCommandBuffer commandBuffer) {
    if (hasIndexBuffer) {
      vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
    } else {
      vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0);
    }
  }

  void LveModel::drawSubMesh(VkCommandBuffer commandBuffer, const SubMesh &subMesh) {
    if (!hasIndexBuffer || subMesh.indexCount == 0) {
      return;
    }
    vkCmdDrawIndexed(commandBuffer, subMesh.indexCount, 1, subMesh.firstIndex, 0, 0);
  }

  void LveModel::computeNodeGlobals(
    const std::vector<glm::mat4> &localOverrides,
    std::vector<glm::mat4> &outGlobals) const {
    outGlobals.clear();
    outGlobals.resize(nodes.size(), glm::mat4(1.f));
    for (std::size_t i = 0; i < nodes.size(); ++i) {
      glm::mat4 local = nodes[i].localTransform;
      if (i < localOverrides.size()) {
        local = local * localOverrides[i];
      }
      if (nodes[i].parent >= 0) {
        outGlobals[i] = outGlobals[static_cast<std::size_t>(nodes[i].parent)] * local;
      } else {
        outGlobals[i] = local;
      }
    }
  }

  const backend::RenderTexture *LveModel::getDiffuseTextureForSubMesh(const SubMesh &subMesh) const {
    if (subMesh.materialIndex < 0 ||
        static_cast<std::size_t>(subMesh.materialIndex) >= materialDiffuseTextures.size()) {
      return nullptr;
    }
    const auto &tex = materialDiffuseTextures[static_cast<std::size_t>(subMesh.materialIndex)];
    return tex ? tex.get() : nullptr;
  }

  std::string LveModel::getDiffusePathForMaterialIndex(int materialIndex) const {
    if (materialIndex < 0 ||
        static_cast<std::size_t>(materialIndex) >= materialPathInfo.size()) {
      return {};
    }
    const auto &info = materialPathInfo[static_cast<std::size_t>(materialIndex)];
    if (info.diffuseKind != TextureSource::Kind::File) {
      return {};
    }
    return info.diffusePath;
  }

  std::string LveModel::getDiffusePathForSubMesh(const SubMesh &subMesh) const {
    return getDiffusePathForMaterialIndex(subMesh.materialIndex);
  }

  bool LveModel::hasAnyDiffuseTexture() const {
    for (const auto &tex : materialDiffuseTextures) {
      if (tex) return true;
    }
    return false;
  }

  void LveModel::bind(VkCommandBuffer commandBuffer) {
    VkBuffer buffers[] = {vertexBuffer->getBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);

    if (hasIndexBuffer) {
      vkCmdBindIndexBuffer(commandBuffer, indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
    }
  }

  void LveModel::calculateBoundingBox(
    const std::vector<backend::ModelVertex>& vertices,
    const std::vector<uint32_t>& indices) {
    glm::vec3 boundsMin((std::numeric_limits<float>::max)());
    glm::vec3 boundsMax((std::numeric_limits<float>::lowest)());

    if (nodes.empty() || subMeshes.empty() || indices.empty()) {
      for (const auto& vertex : vertices) {
        boundsMin = (glm::min)(boundsMin, vertex.position);
        boundsMax = (glm::max)(boundsMax, vertex.position);
      }
      boundingBox.min = boundsMin;
      boundingBox.max = boundsMax;
      return;
    }

    std::vector<glm::mat4> nodeGlobals;
    computeNodeGlobals({}, nodeGlobals);
    for (std::size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex) {
      const auto &node = nodes[nodeIndex];
      if (node.meshes.empty()) continue;
      const glm::mat4 &nodeTransform = nodeGlobals[nodeIndex];
      for (int meshIndex : node.meshes) {
        if (meshIndex < 0 || static_cast<std::size_t>(meshIndex) >= subMeshes.size()) {
          continue;
        }
        const auto &subMesh = subMeshes[static_cast<std::size_t>(meshIndex)];
        const uint32_t end = subMesh.firstIndex + subMesh.indexCount;
        for (uint32_t i = subMesh.firstIndex; i < end && i < indices.size(); ++i) {
          const uint32_t vertexIndex = indices[i];
          if (vertexIndex >= vertices.size()) continue;
          const glm::vec4 pos = nodeTransform * glm::vec4(vertices[vertexIndex].position, 1.f);
          const glm::vec3 pos3{pos.x, pos.y, pos.z};
          boundsMin = (glm::min)(boundsMin, pos3);
          boundsMax = (glm::max)(boundsMax, pos3);
        }
      }
    }

    boundingBox.min = boundsMin;
    boundingBox.max = boundsMax;
  }

  std::vector<VkVertexInputBindingDescription> LveModel::getBindingDescriptions() {
    std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);
    bindingDescriptions[0].binding = 0;
    bindingDescriptions[0].stride = sizeof(backend::ModelVertex);
    bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescriptions;
  }

  std::vector<VkVertexInputAttributeDescription> LveModel::getAttributeDescriptions() {
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};

    attributeDescriptions.push_back({0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(backend::ModelVertex, position)});
    attributeDescriptions.push_back({1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(backend::ModelVertex, color)});
    attributeDescriptions.push_back({2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(backend::ModelVertex, normal)});
    attributeDescriptions.push_back({3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(backend::ModelVertex, uv)});

    return attributeDescriptions;
  }

}  // namespace lve
