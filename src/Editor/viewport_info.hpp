#pragma once

#include <cstdint>

namespace lve {

  struct ViewportInfo {
    float x{0.f};
    float y{0.f};
    uint32_t width{0};
    uint32_t height{0};
    bool visible{false};
    bool hovered{false};
    bool rightMouseDown{false};
    bool leftMouseClicked{false};
    bool allowPick{false};
    float mouseDeltaX{0.f};
    float mouseDeltaY{0.f};
    float mousePosX{0.f};
    float mousePosY{0.f};
  };

} // namespace lve
