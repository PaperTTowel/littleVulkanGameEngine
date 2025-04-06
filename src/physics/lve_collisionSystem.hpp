#pragma once

#include "lve_collision.hpp"

// std
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
  class CollisionSystem{
    public:
    void setMapTriangles(const std::vector<Triangle>& tris);

    std::optional<RaycastHit> raycast(const Ray& ray, float maxDistance = 1000.f) const;

    private:
    std::vector<Triangle> triangles;
  };
} // namespace lve