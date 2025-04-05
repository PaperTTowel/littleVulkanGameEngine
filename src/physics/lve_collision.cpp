#include "lve_collision.hpp"

#include <glm/gtx/intersect.hpp>

namespace lve{
  void LveCollision::setTriangles(const std::vector<Triangle>& triangles){
    this->triangles = triangles;
  }
  // 광선(Ray)과 삼각형 충돌 감지 (Möller–Trumbore 알고리즘)
  bool LveCollision::rayIntersectsTriangle(const glm::vec3 &rayOrigin, const glm::vec3 &rayDir, float &t) const{
    for (const auto &tri : triangles){
      glm::vec3 edge1 = tri.v1 - tri.v0;
      glm::vec3 edge2 = tri.v2 - tri.v0;
      glm::vec3 h = glm::cross(rayDir, edge2);
      float a = glm::dot(edge1, h);

      if(a > -0.00001f && a < 0.00001f) continue;

      float f = 1.0f / a;
      glm::vec3 s = rayOrigin - tri.v0;
      float u = f * glm::dot(s, h);
      if(u < 0.0f || u > 1.0f) continue;

      glm::vec3 q = glm::cross(s, edge1);
      float v = f * glm::dot(rayDir, q);
      if(v < 0.0f || u + v > 1.0f) continue;

      t = f * glm::dot(edge2, q);
      if(t > 0.00001f){
        return true; // 충돌 발생
      }
    }
    return false; // 충돌 없음
  }
}