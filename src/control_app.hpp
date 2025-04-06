#pragma once

#include "lve_window.hpp"
#include "lve_game_object.hpp"
#include "lve_device.hpp"
#include "lve_renderer.hpp"
#include "lve_descriptors.hpp"
#include "physics/lve_collisionSystem.hpp"

// std
#include <memory>
#include <vector>

namespace lve {
  class ControlApp {
  public:
    static constexpr int WIDTH = 800;
    static constexpr int HEIGHT = 600;

    ControlApp();
    ~ControlApp();

    ControlApp(const LveWindow &) = delete;
    ControlApp &operator=(const LveWindow &) = delete;

    void run();

  private:
    void loadGameObjects();

    LveWindow lveWindow{WIDTH, HEIGHT, "Hello Vulkan!"};
    LveDevice lveDevice{lveWindow};
    LveRenderer lveRenderer{lveWindow, lveDevice};

    std::unique_ptr<LveDescriptorPool> globalPool{};
    std::vector<std::unique_ptr<LveDescriptorPool>> framePools;
    LveGameObjectManager gameObjectManager{lveDevice};

    CollisionSystem collisionSystem;
    Ray ray;

    LveGameObject::id_t characterId;
  };
} // namespace lve