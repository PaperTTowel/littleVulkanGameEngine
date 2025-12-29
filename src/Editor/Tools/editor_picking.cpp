#include "Editor/Tools/editor_picking.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace lve::editor::tools {

  Ray BuildPickRay(const ViewportInfo &view, const glm::mat4 &viewMat, const glm::mat4 &projMat) {
    Ray ray{};
    if (!view.visible || view.width == 0 || view.height == 0) return ray;

    const float relX = (view.mousePosX - view.x) / static_cast<float>(view.width);
    const float relY = (view.mousePosY - view.y) / static_cast<float>(view.height);
    if (relX < 0.f || relX > 1.f || relY < 0.f || relY > 1.f) return ray;

    const float ndcX = relX * 2.f - 1.f;
    const float ndcY = relY * 2.f - 1.f;
    const glm::mat4 inv = glm::inverse(projMat * viewMat);

    glm::vec4 nearClip{ndcX, ndcY, 0.f, 1.f};
    glm::vec4 farClip{ndcX, ndcY, 1.f, 1.f};
    glm::vec4 nearWorld = inv * nearClip;
    glm::vec4 farWorld = inv * farClip;
    if (nearWorld.w == 0.f || farWorld.w == 0.f) return ray;
    nearWorld /= nearWorld.w;
    farWorld /= farWorld.w;

    ray.origin = glm::vec3(nearWorld);
    ray.direction = glm::normalize(glm::vec3(farWorld - nearWorld));
    ray.valid = true;
    return ray;
  }

  bool IntersectSphere(const Ray &ray, const glm::vec3 &center, float radius, float &tHit) {
    glm::vec3 oc = ray.origin - center;
    float b = glm::dot(oc, ray.direction);
    float c = glm::dot(oc, oc) - radius * radius;
    float h = b * b - c;
    if (h < 0.f) return false;
    h = std::sqrt(h);
    float t = -b - h;
    if (t < 0.f) t = -b + h;
    if (t < 0.f) return false;
    tHit = t;
    return true;
  }

  bool IntersectAabbLocal(
    const glm::vec3 &origin,
    const glm::vec3 &dir,
    const glm::vec3 &min,
    const glm::vec3 &max,
    float &tHit) {
    float tMin = -std::numeric_limits<float>::infinity();
    float tMax = std::numeric_limits<float>::infinity();

    for (int axis = 0; axis < 3; ++axis) {
      const float o = origin[axis];
      const float d = dir[axis];
      if (std::abs(d) < 1e-6f) {
        if (o < min[axis] || o > max[axis]) return false;
        continue;
      }
      float invD = 1.0f / d;
      float t1 = (min[axis] - o) * invD;
      float t2 = (max[axis] - o) * invD;
      if (t1 > t2) std::swap(t1, t2);
      tMin = std::max(tMin, t1);
      tMax = std::min(tMax, t2);
      if (tMin > tMax) return false;
    }

    if (tMax < 0.f) return false;
    tHit = (tMin >= 0.f) ? tMin : tMax;
    return true;
  }

  void TransformAabb(
    const glm::mat4 &transform,
    const glm::vec3 &min,
    const glm::vec3 &max,
    glm::vec3 &outMin,
    glm::vec3 &outMax) {
    outMin = glm::vec3(std::numeric_limits<float>::max());
    outMax = glm::vec3(std::numeric_limits<float>::lowest());
    glm::vec3 corners[8] = {
      {min.x, min.y, min.z},
      {max.x, min.y, min.z},
      {min.x, max.y, min.z},
      {max.x, max.y, min.z},
      {min.x, min.y, max.z},
      {max.x, min.y, max.z},
      {min.x, max.y, max.z},
      {max.x, max.y, max.z}
    };
    for (const auto &corner : corners) {
      glm::vec3 world = glm::vec3(transform * glm::vec4(corner, 1.f));
      outMin = glm::min(outMin, world);
      outMax = glm::max(outMax, world);
    }
  }

  bool IntersectBillboardQuad(
    const Ray &ray,
    const glm::vec3 &center,
    const glm::vec3 &right,
    const glm::vec3 &up,
    const glm::vec2 &halfSize,
    float &tHit) {
    const glm::vec3 normal = glm::normalize(glm::cross(right, up));
    float denom = glm::dot(normal, ray.direction);
    if (std::abs(denom) < 1e-6f) return false;
    float t = glm::dot(center - ray.origin, normal) / denom;
    if (t < 0.f) return false;
    glm::vec3 hit = ray.origin + ray.direction * t;
    glm::vec3 delta = hit - center;
    float localX = glm::dot(delta, right);
    float localY = glm::dot(delta, up);
    if (std::abs(localX) > halfSize.x || std::abs(localY) > halfSize.y) return false;
    tHit = t;
    return true;
  }

} // namespace lve::editor::tools
