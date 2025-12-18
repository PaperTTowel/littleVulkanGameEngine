#pragma once

#include <glm/glm.hpp>

#include <optional>
#include <string>
#include <vector>

namespace lve {

  struct SceneResources {
    std::string basePath;
    std::string spritePath;
    std::string modelPath;
    std::string materialPath;
  };

  struct TransformData {
    glm::vec3 position{0.f};
    glm::vec3 rotation{0.f};
    glm::vec3 scale{1.f};
  };

  enum class EntityType { Sprite, Mesh, Light, Camera };
  enum class BillboardKind { None, Cylindrical, Spherical };
  enum class LightKind { Point, Spot, Directional };

  struct SpriteComponent {
    std::string spriteMeta;
    std::string state{"idle"};
    BillboardKind billboard{BillboardKind::Cylindrical};
    bool useOrtho{true};
    int layer{0};
  };

  struct MeshComponent {
    std::string model;
    std::string material;
  };

  struct LightComponent {
    LightKind kind{LightKind::Point};
    glm::vec3 color{1.f, 1.f, 1.f};
    float intensity{1.f};
    float range{10.f};
    float angle{45.f};
  };

  struct CameraComponent {
    std::string projection{"persp"};
    float fov{60.f};
    float orthoHeight{10.f};
    float nearPlane{0.1f};
    float farPlane{100.f};
    bool active{false};
  };

  struct SceneEntity {
    std::string id;
    std::string name;
    EntityType type{EntityType::Mesh};
    TransformData transform{};
    std::optional<SpriteComponent> sprite;
    std::optional<MeshComponent> mesh;
    std::optional<LightComponent> light;
    std::optional<CameraComponent> camera;
  };

  struct Scene {
    int version{1};
    SceneResources resources{};
    std::vector<SceneEntity> entities{};
  };

  class SceneSerializer {
  public:
    static bool saveToFile(const Scene &scene, const std::string &path);
    static bool loadFromFile(const std::string &path, Scene &outScene);
  };

} // namespace lve
