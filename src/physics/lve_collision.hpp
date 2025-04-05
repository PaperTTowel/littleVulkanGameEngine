#pragma once

// libs
#include <glm/glm.hpp>
// std
#include <vector>

namespace lve{
  struct Triangle{ glm::vec3 v0, v1, v2; };

  class LveCollision{
    public:
    void setTriangles(const std::vector<Triangle>& triangless);

    bool rayIntersectsTriangle(const glm::vec3& rayOrigin, const glm::vec3& rayDir, float& t) const;

    private:
    std::vector<Triangle> triangles; // 충돌 감지용 삼각형 리스트
  };
} // namespace lve