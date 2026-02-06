#pragma once

// libs
#include <glm/vec3.hpp>

namespace lve::coords {

  // Runtime world axes: +X right, +Y down, +Z forward.
  inline constexpr glm::vec3 kRight{1.f, 0.f, 0.f};
  inline constexpr glm::vec3 kUp{0.f, -1.f, 0.f};
  inline constexpr glm::vec3 kDown{0.f, 1.f, 0.f};
  inline constexpr glm::vec3 kForward{0.f, 0.f, 1.f};

} // namespace lve::coords
