#pragma once

#include "Editor/viewport_info.hpp"

#include <glm/glm.hpp>

namespace lve::editor::tools {

  struct Ray {
    glm::vec3 origin{};
    glm::vec3 direction{};
    bool valid{false};
  };

  Ray BuildPickRay(const ViewportInfo &view, const glm::mat4 &viewMat, const glm::mat4 &projMat);
  bool IntersectSphere(const Ray &ray, const glm::vec3 &center, float radius, float &tHit);
  bool IntersectAabbLocal(
    const glm::vec3 &origin,
    const glm::vec3 &dir,
    const glm::vec3 &min,
    const glm::vec3 &max,
    float &tHit);
  void TransformAabb(
    const glm::mat4 &transform,
    const glm::vec3 &min,
    const glm::vec3 &max,
    glm::vec3 &outMin,
    glm::vec3 &outMax);
  bool IntersectBillboardQuad(
    const Ray &ray,
    const glm::vec3 &center,
    const glm::vec3 &right,
    const glm::vec3 &up,
    const glm::vec2 &halfSize,
    float &tHit);

} // namespace lve::editor::tools
