#pragma once

// libs
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
// std
#include <vector>
#include <optional>

struct Ray {
  glm::vec3 origin;
  glm::vec3 direction;
};

struct RaycastHit {
  glm::vec3 point;
  float distance;
  glm::vec3 normal;
};

namespace lve{
  struct Triangle{ glm::vec3 v0, v1, v2; };
  
  class CollisionSystem{
    public:
    void setMapTriangles(const std::vector<Triangle>& tris);

    std::optional<RaycastHit> raycast(const Ray& ray, float maxDistance = 1000.f) const;

    private:
    std::vector<Triangle> triangles;
  };
} // namespace lve