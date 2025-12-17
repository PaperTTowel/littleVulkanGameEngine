#pragma once

#include "Backend/window.hpp"
#include "utils/game_object.hpp"
#include "Backend/device.hpp"
#include "Rendering/renderer.hpp"
#include "Backend/descriptors.hpp"
#include "utils/sprite_metadata.hpp"
#include "utils/sprite_animator.hpp"

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

    LveGameObject::id_t characterId;
    bool wireframeEnabled{false};
    bool normalViewEnabled{false};
    SpriteMetadata playerMeta;
    std::unique_ptr<SpriteAnimator> spriteAnimator;
  };
} // namespace lve
