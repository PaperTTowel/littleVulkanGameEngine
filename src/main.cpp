#include "Engine/engine_loop.hpp"

// std
#include <cstdlib>
#include <iostream>
#include <stdexcept>

int main() {
  lve::EngineLoop app{};

  try {
    app.run();
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
