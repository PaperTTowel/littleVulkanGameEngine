#include "lve_collisionSystem.hpp"

// std
#include <limits>

namespace lve{
  void CollisionSystem::setMapTriangles(const std::vector<Triangle>& tris) {
    triangles = std::move(tris);
  }

  // Möller–Trumbore ray-triangle intersection
  static std::optional<RaycastHit> intersectRayWithTriangle(
      const Ray &ray, const Triangle &tri, float maxDistance
    ){
    constexpr float RAY_EPSILON = 1e-6f;
    glm::vec3 edge1 = tri.v1 - tri.v0;
    glm::vec3 edge2 = tri.v2 - tri.v0;
    glm::vec3 h = glm::cross(ray.direction, edge2);
    float a = glm::dot(edge1, h);

    // 평면에 가까운 삼각형 건너뛰기
    if (a > -RAY_EPSILON && a < RAY_EPSILON) return std::nullopt;

    float f = 1.0f / a;
    glm::vec3 s = ray.origin - tri.v0;
    float u = f * glm::dot(s, h);
    if(u < 0.0f || u > 1.0f) return std::nullopt;

    glm::vec3 q = glm::cross(s, edge1);
    float v = f * glm::dot(ray.direction, q);
    if(v < 0.0f || u + v > 1.0f) return std::nullopt;

    float t = f * glm::dot(edge2, q);
    if(t > RAY_EPSILON && t < maxDistance){
      RaycastHit hit;
      hit.point = ray.origin + ray.direction * t;
      hit.distance = t;
      hit.normal = glm::normalize(glm::cross(edge1, edge2));
      return hit;
    }

    return std::nullopt;
  }

  std::optional<RaycastHit> CollisionSystem::raycast(const Ray& ray, float maxDistance) const {
    std::optional<RaycastHit> closestHit;
    float closestDistance = std::numeric_limits<float>::max();

    for(const auto& tri : triangles){
      auto hit = intersectRayWithTriangle(ray, tri, maxDistance);
      if(hit && hit->distance < closestDistance){
        closestHit = hit;
        closestDistance = hit->distance;
      }
    }
    return closestHit;
  }
} // namespace lve