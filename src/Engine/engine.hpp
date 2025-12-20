#pragma once

#include "Backend/window.hpp"
#include "utils/game_object.hpp"
#include "Backend/device.hpp"
#include "Rendering/renderer.hpp"
#include "Backend/descriptors.hpp"
#include "utils/sprite_metadata.hpp"
#include "utils/sprite_animator.hpp"
#include "Editor/hierarchy_panel.hpp"
#include "Editor/inspector_panel.hpp"
#include "Engine/scene.hpp"
#include "Editor/scene_panel.hpp"
#include "Editor/resource_browser_panel.hpp"

// std
#include <memory>
#include <optional>
#include <vector>
#include <unordered_map>

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
    LveGameObject &createSpriteObject(
      const glm::vec3 &position,
      ObjectState state = ObjectState::IDLE,
      bool useOrtho = true,
      const std::string &metaPath = "Assets/textures/characters/player.json");
    LveGameObject &createMeshObject(
      const glm::vec3 &position,
      const std::string &modelPath = "Assets/models/colored_cube.obj");
    LveGameObject &createPointLightObject(const glm::vec3 &position);
    Scene exportSceneSnapshot() const;
    void importSceneSnapshot(const Scene &scene);
    void saveSceneToFile(const std::string &path);
    void loadSceneFromFile(const std::string &path);
    static ObjectState objectStateFromString(const std::string &name);
    static std::string objectStateToString(ObjectState state);
    std::shared_ptr<LveModel> loadModelCached(const std::string &path);
    bool setActiveSpriteMetadata(const std::string &path);

    LveWindow lveWindow{WIDTH, HEIGHT, "Hello Vulkan!"};
    LveDevice lveDevice{lveWindow};
    LveRenderer lveRenderer{lveWindow, lveDevice};

    std::unique_ptr<LveDescriptorPool> globalPool{};
    std::vector<std::unique_ptr<LveDescriptorPool>> framePools;
    LveGameObjectManager gameObjectManager{lveDevice};

    LveGameObject::id_t characterId;
    LveGameObject::id_t viewerId;
    bool wireframeEnabled{false};
    bool normalViewEnabled{false};
    SpriteMetadata playerMeta;
    std::unique_ptr<SpriteAnimator> spriteAnimator;
    std::shared_ptr<LveModel> cubeModel;
    std::shared_ptr<LveModel> spriteModel;
    std::unordered_map<std::string, std::shared_ptr<LveModel>> modelCache;
    editor::HierarchyPanelState hierarchyState;
    editor::ScenePanelState scenePanelState;
    editor::ResourceBrowserState resourceBrowserState;
  };
} // namespace lve
