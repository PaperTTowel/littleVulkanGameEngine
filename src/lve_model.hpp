#pragma once

#include "lve_buffer.hpp"
#include "lve_device.hpp"

// libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

// std
#include <memory>
#include <vector>

namespace lve{
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

    struct Builder{
      std::vector<Vertex> vertices{};
      std::vector<uint32_t> indices{};

      void loadModel(const std::string &filepath);
    };

    LveModel(LveDevice &device, const LveModel::Builder &builder);
    ~LveModel();

    LveModel(const LveModel &) = delete;
    LveModel &operator=(const LveModel &) = delete;

    static std::unique_ptr<LveModel> createModelFromFile(LveDevice &device, const std::string &filepath);

    void bind(VkCommandBuffer commandBuffer);
    void draw(VkCommandBuffer commandBuffer);

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

    void calculateBoundingBox(const std::vector<Vertex>& vertices);

    LveDevice &lveDevice;

    std::unique_ptr<LveBuffer> vertexBuffer;
    uint32_t vertexCount;

    bool hasIndexBuffer = false;
    std::unique_ptr<LveBuffer> indexBuffer;
    uint32_t indexCount;

    BoundingBox boundingBox;
  };
} // namespace lve