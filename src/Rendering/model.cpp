#include "Rendering/model.hpp"

#include "utils/utils.hpp"
#include "Rendering/texture.hpp"

// libs
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

// std
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <unordered_map>

#ifndef ENGINE_DIR
#define ENGINE_DIR ""
#endif

namespace std {
  template <>
  struct hash<lve::LveModel::Vertex> {
    size_t operator()(lve::LveModel::Vertex const &vertex) const {
      size_t seed = 0;
      lve::hashCombine(seed, vertex.position, vertex.color, vertex.normal, vertex.uv);
      return seed;
    }
  };
}  // namespace std

namespace lve {

  namespace {
    glm::mat4 toGlmMat4(const aiMatrix4x4 &m) {
      glm::mat4 out{1.0f};
      out[0][0] = m.a1; out[1][0] = m.a2; out[2][0] = m.a3; out[3][0] = m.a4;
      out[0][1] = m.b1; out[1][1] = m.b2; out[2][1] = m.b3; out[3][1] = m.b4;
      out[0][2] = m.c1; out[1][2] = m.c2; out[2][2] = m.c3; out[3][2] = m.c4;
      out[0][3] = m.d1; out[1][3] = m.d2; out[2][3] = m.d3; out[3][3] = m.d4;
      return out;
    }

    std::string resolveTexturePath(const std::filesystem::path &baseDir, const std::string &relative) {
      if (relative.empty()) {
        return {};
      }
      std::filesystem::path path{relative};
      if (path.is_relative()) {
        path = baseDir / path;
      }
      return path.generic_string();
    }

    LveModel::TextureSource loadEmbeddedTexture(const aiScene *scene, int index) {
      LveModel::TextureSource source{};
      if (!scene || index < 0 || index >= static_cast<int>(scene->mNumTextures)) {
        return source;
      }

      const aiTexture *tex = scene->mTextures[index];
      if (!tex) {
        return source;
      }

      if (tex->mHeight == 0) {
        source.kind = LveModel::TextureSource::Kind::EmbeddedCompressed;
        const auto *dataPtr = reinterpret_cast<const unsigned char *>(tex->pcData);
        source.data.assign(dataPtr, dataPtr + tex->mWidth);
      } else {
        source.kind = LveModel::TextureSource::Kind::EmbeddedRaw;
        source.width = static_cast<int>(tex->mWidth);
        source.height = static_cast<int>(tex->mHeight);
        source.data.resize(static_cast<std::size_t>(source.width) * source.height * 4);
        for (int y = 0; y < source.height; ++y) {
          for (int x = 0; x < source.width; ++x) {
            const aiTexel &texel = tex->pcData[y * source.width + x];
            const std::size_t idx = (static_cast<std::size_t>(y) * source.width + x) * 4;
            source.data[idx + 0] = texel.r;
            source.data[idx + 1] = texel.g;
            source.data[idx + 2] = texel.b;
            source.data[idx + 3] = texel.a;
          }
        }
      }
      return source;
    }

    LveModel::TextureSource loadTextureSource(
      const aiScene *scene,
      const aiMaterial *material,
      aiTextureType textureType,
      const std::filesystem::path &baseDir) {
      LveModel::TextureSource source{};
      if (!material || material->GetTextureCount(textureType) == 0) {
        return source;
      }

      aiString path;
      if (material->GetTexture(textureType, 0, &path) != AI_SUCCESS) {
        return source;
      }

      const std::string texPath = path.C_Str();
      if (texPath.empty()) {
        return source;
      }

      if (texPath[0] == '*') {
        const int texIndex = std::atoi(texPath.c_str() + 1);
        return loadEmbeddedTexture(scene, texIndex);
      }

      source.kind = LveModel::TextureSource::Kind::File;
      source.path = resolveTexturePath(baseDir, texPath);
      return source;
    }

    void processMesh(
      const aiMesh *mesh,
      const aiMaterial *material,
      LveModel::Builder &builder,
      std::unordered_map<LveModel::Vertex, uint32_t> &uniqueVertices,
      LveModel::SubMesh &outSubMesh) {
      const uint32_t indexStart = static_cast<uint32_t>(builder.indices.size());
      glm::vec3 boundsMin(std::numeric_limits<float>::max());
      glm::vec3 boundsMax(std::numeric_limits<float>::lowest());
      bool hasBounds = false;
      for (unsigned int faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
        const aiFace &face = mesh->mFaces[faceIndex];
        if (face.mNumIndices != 3) {
          continue;
        }
        for (unsigned int i = 0; i < face.mNumIndices; ++i) {
          unsigned int index = face.mIndices[i];
          LveModel::Vertex vertex{};

          if (mesh->HasPositions()) {
            const aiVector3D &pos = mesh->mVertices[index];
            vertex.position = {pos.x, pos.y, pos.z};
            boundsMin = glm::min(boundsMin, vertex.position);
            boundsMax = glm::max(boundsMax, vertex.position);
            hasBounds = true;
          }

          if (mesh->HasNormals()) {
            const aiVector3D &n = mesh->mNormals[index];
            vertex.normal = {n.x, n.y, n.z};
          }

          if (mesh->HasTextureCoords(0)) {
            const aiVector3D &uv = mesh->mTextureCoords[0][index];
            vertex.uv = {uv.x, uv.y};
          }

          if (mesh->HasVertexColors(0)) {
            const aiColor4D &c = mesh->mColors[0][index];
            vertex.color = {c.r, c.g, c.b};
          } else if (material) {
            aiColor4D diffuse;
            if (aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &diffuse) == AI_SUCCESS) {
              vertex.color = {diffuse.r, diffuse.g, diffuse.b};
            } else {
              vertex.color = {1.0f, 1.0f, 1.0f};
            }
          } else {
            vertex.color = {1.0f, 1.0f, 1.0f};
          }

          if (uniqueVertices.count(vertex) == 0) {
            uniqueVertices[vertex] = static_cast<uint32_t>(builder.vertices.size());
            builder.vertices.push_back(vertex);
          }
          builder.indices.push_back(uniqueVertices[vertex]);
        }
      }
      outSubMesh.firstIndex = indexStart;
      outSubMesh.indexCount = static_cast<uint32_t>(builder.indices.size()) - indexStart;
      outSubMesh.materialIndex = static_cast<int>(mesh->mMaterialIndex);
      outSubMesh.hasBounds = hasBounds;
      if (hasBounds) {
        outSubMesh.boundsMin = boundsMin;
        outSubMesh.boundsMax = boundsMax;
      }
    }

    void processNode(
      const aiNode *node,
      int parentIndex,
      const std::vector<int> &meshIndexToSubmesh,
      LveModel::Builder &builder) {
      LveModel::Node newNode{};
      newNode.name = node->mName.C_Str();
      newNode.parent = parentIndex;
      newNode.localTransform = toGlmMat4(node->mTransformation);
      newNode.meshes.reserve(node->mNumMeshes);
      for (unsigned int meshIndex = 0; meshIndex < node->mNumMeshes; ++meshIndex) {
        const unsigned int sceneMeshIndex = node->mMeshes[meshIndex];
        if (sceneMeshIndex < meshIndexToSubmesh.size()) {
          const int subMeshIndex = meshIndexToSubmesh[sceneMeshIndex];
          if (subMeshIndex >= 0) {
            newNode.meshes.push_back(subMeshIndex);
          }
        }
      }

      const int nodeIndex = static_cast<int>(builder.nodes.size());
      builder.nodes.push_back(std::move(newNode));
      if (parentIndex >= 0) {
        builder.nodes[static_cast<std::size_t>(parentIndex)].children.push_back(nodeIndex);
      }

      for (unsigned int childIndex = 0; childIndex < node->mNumChildren; ++childIndex) {
        processNode(node->mChildren[childIndex], nodeIndex, meshIndexToSubmesh, builder);
      }
    }
  } // namespace

  LveModel::LveModel(LveDevice &device, const LveModel::Builder &builder)
    : lveDevice{device},
      subMeshes{builder.subMeshes},
      nodes{builder.nodes} {
    createVertexBuffers(builder.vertices);
    createIndexBuffers(builder.indices);
    loadMaterials(builder.materials);
    calculateBoundingBox(builder.vertices, builder.indices);
  }

  LveModel::~LveModel() {}

  std::unique_ptr<LveModel> LveModel::createModelFromFile(
      LveDevice &device, const std::string &filepath) {
    Builder builder{};
    builder.loadModel(ENGINE_DIR + filepath);
    return std::make_unique<LveModel>(device, builder);
  }

  void LveModel::createVertexBuffers(const std::vector<Vertex> &vertices) {
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

  const LveTexture *LveModel::getDiffuseTextureForSubMesh(const SubMesh &subMesh) const {
    if (subMesh.materialIndex < 0 ||
        static_cast<std::size_t>(subMesh.materialIndex) >= materialDiffuseTextures.size()) {
      return nullptr;
    }
    const auto &tex = materialDiffuseTextures[static_cast<std::size_t>(subMesh.materialIndex)];
    return tex ? tex.get() : nullptr;
  }

  bool LveModel::hasAnyDiffuseTexture() const {
    for (const auto &tex : materialDiffuseTextures) {
      if (tex) return true;
    }
    return false;
  }

  void LveModel::loadMaterials(const std::vector<MaterialSource> &materials) {
    materialDiffuseTextures.clear();
    materialDiffuseTextures.resize(materials.size());

    std::unordered_map<std::string, std::shared_ptr<LveTexture>> fileCache;
    for (std::size_t i = 0; i < materials.size(); ++i) {
      const auto &source = materials[i].diffuse;
      std::shared_ptr<LveTexture> texture{};

      switch (source.kind) {
        case TextureSource::Kind::File: {
          if (!source.path.empty()) {
            auto it = fileCache.find(source.path);
            if (it != fileCache.end()) {
              texture = it->second;
            } else {
              auto uniqueTex = LveTexture::createTextureFromFile(lveDevice, source.path);
              texture = std::shared_ptr<LveTexture>(std::move(uniqueTex));
              fileCache[source.path] = texture;
            }
          }
          break;
        }
        case TextureSource::Kind::EmbeddedCompressed: {
          if (!source.data.empty()) {
            auto uniqueTex = LveTexture::createTextureFromMemory(
              lveDevice,
              source.data.data(),
              source.data.size());
            texture = std::shared_ptr<LveTexture>(std::move(uniqueTex));
          }
          break;
        }
        case TextureSource::Kind::EmbeddedRaw: {
          if (!source.data.empty() && source.width > 0 && source.height > 0) {
            auto uniqueTex = LveTexture::createTextureFromRgba(
              lveDevice,
              source.data.data(),
              source.width,
              source.height);
            texture = std::shared_ptr<LveTexture>(std::move(uniqueTex));
          }
          break;
        }
        case TextureSource::Kind::None:
        default:
          break;
      }

      materialDiffuseTextures[i] = texture;
    }
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
    const std::vector<Vertex>& vertices,
    const std::vector<uint32_t>& indices) {
    glm::vec3 min(std::numeric_limits<float>::max());
    glm::vec3 max(std::numeric_limits<float>::lowest());

    if (nodes.empty() || subMeshes.empty() || indices.empty()) {
      for (const auto& vertex : vertices) {
        min = glm::min(min, vertex.position);
        max = glm::max(max, vertex.position);
      }
      boundingBox.min = min;
      boundingBox.max = max;
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
          min = glm::min(min, pos3);
          max = glm::max(max, pos3);
        }
      }
    }

    boundingBox.min = min;
    boundingBox.max = max;
  }

  std::vector<VkVertexInputBindingDescription> LveModel::Vertex::getBindingDescriptions() {
    std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);
    bindingDescriptions[0].binding = 0;
    bindingDescriptions[0].stride = sizeof(Vertex);
    bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescriptions;
  }

  std::vector<VkVertexInputAttributeDescription> LveModel::Vertex::getAttributeDescriptions() {
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};

    attributeDescriptions.push_back({0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)});
    attributeDescriptions.push_back({1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color)});
    attributeDescriptions.push_back({2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)});
    attributeDescriptions.push_back({3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)});

    return attributeDescriptions;
  }

  void LveModel::Builder::loadModel(const std::string &filepath) {
    Assimp::Importer importer;
    const aiScene *scene = importer.ReadFile(
      filepath,
      aiProcess_Triangulate |
        aiProcess_GenNormals |
        aiProcess_JoinIdenticalVertices |
        aiProcess_FlipUVs);
    if (!scene || !scene->mRootNode) {
      throw std::runtime_error(importer.GetErrorString());
    }

    vertices.clear();
    indices.clear();
    subMeshes.clear();
    nodes.clear();
    materials.clear();

    const std::filesystem::path baseDir = std::filesystem::path(filepath).parent_path();
    materials.resize(scene->mNumMaterials);
    for (unsigned int materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex) {
      const aiMaterial *material = scene->mMaterials[materialIndex];
      MaterialSource materialSource{};
      materialSource.diffuse = loadTextureSource(scene, material, aiTextureType_BASE_COLOR, baseDir);
      if (materialSource.diffuse.kind == TextureSource::Kind::None) {
        materialSource.diffuse = loadTextureSource(scene, material, aiTextureType_DIFFUSE, baseDir);
      }
      materials[materialIndex] = std::move(materialSource);
    }

    std::unordered_map<Vertex, uint32_t> uniqueVertices{};
    std::vector<int> meshIndexToSubmesh(scene->mNumMeshes, -1);
    for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
      const aiMesh *mesh = scene->mMeshes[meshIndex];
      const aiMaterial *material = nullptr;
      if (mesh->mMaterialIndex < scene->mNumMaterials) {
        material = scene->mMaterials[mesh->mMaterialIndex];
      }
      SubMesh subMesh{};
      processMesh(mesh, material, *this, uniqueVertices, subMesh);
      meshIndexToSubmesh[meshIndex] = static_cast<int>(subMeshes.size());
      subMeshes.push_back(subMesh);
    }

    processNode(scene->mRootNode, -1, meshIndexToSubmesh, *this);
  }

}  // namespace lve


