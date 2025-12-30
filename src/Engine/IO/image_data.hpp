#pragma once

#include <vector>

namespace lve {

  struct ImageData {
    int width{0};
    int height{0};
    int channels{0};
    std::vector<unsigned char> pixels{};
  };

} // namespace lve
