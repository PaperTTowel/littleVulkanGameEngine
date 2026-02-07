#include "scene.hpp"
#include "Engine/path_utils.hpp"

// std
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>

namespace lve {

  namespace {
    std::string toString(EntityType type) {
      switch (type) {
        case EntityType::Sprite: return "sprite";
        case EntityType::Mesh: return "mesh";
        case EntityType::Light: return "light";
        case EntityType::Camera: return "camera";
        default: return "mesh";
      }
    }

    EntityType entityTypeFromString(const std::string &s) {
      if (s == "sprite") return EntityType::Sprite;
      if (s == "mesh") return EntityType::Mesh;
      if (s == "light") return EntityType::Light;
      if (s == "camera") return EntityType::Camera;
      return EntityType::Mesh;
    }

    std::string toString(BillboardKind kind) {
      switch (kind) {
        case BillboardKind::None: return "none";
        case BillboardKind::Cylindrical: return "cylindrical";
        case BillboardKind::Spherical: return "spherical";
        default: return "none";
      }
    }

    BillboardKind billboardFromString(const std::string &s) {
      if (s == "cylindrical") return BillboardKind::Cylindrical;
      if (s == "spherical") return BillboardKind::Spherical;
      if (s == "none") return BillboardKind::None;
      return BillboardKind::Cylindrical;
    }

    std::string toString(LightKind kind) {
      switch (kind) {
        case LightKind::Point: return "point";
        case LightKind::Spot: return "spot";
        case LightKind::Directional: return "dir";
        default: return "point";
      }
    }

    LightKind lightFromString(const std::string &s) {
      if (s == "point") return LightKind::Point;
      if (s == "spot") return LightKind::Spot;
      if (s == "dir" || s == "directional") return LightKind::Directional;
      return LightKind::Point;
    }

    std::string readFileToString(const std::filesystem::path &path) {
      std::ifstream file(path, std::ios::in | std::ios::binary);
      if (!file) return {};
      std::ostringstream ss;
      ss << file.rdbuf();
      return ss.str();
    }

    bool writeStringToFile(const std::filesystem::path &path, const std::string &data) {
      std::ofstream file(path, std::ios::out | std::ios::binary | std::ios::trunc);
      if (!file) return false;
      file << data;
      return true;
    }

    std::string indent(int level) {
      return std::string(level * 2, ' ');
    }

    void writeVec3(std::ostringstream &ss, const glm::vec3 &v) {
      ss << "[" << v.x << ", " << v.y << ", " << v.z << "]";
    }

    glm::vec3 parseVec3(const std::string &src, const std::string &key, glm::vec3 defVal) {
      std::regex re("\"" + key + "\"\\s*:\\s*\\[\\s*(-?\\d+(?:\\.\\d+)?)\\s*,\\s*(-?\\d+(?:\\.\\d+)?)\\s*,\\s*(-?\\d+(?:\\.\\d+)?)\\s*\\]");
      std::smatch m;
      if (std::regex_search(src, m, re) && m.size() >= 4) {
        return glm::vec3{std::stof(m[1].str()), std::stof(m[2].str()), std::stof(m[3].str())};
      }
      return defVal;
    }

    float parseFloat(const std::string &src, const std::string &key, float defVal) {
      std::regex re("\"" + key + "\"\\s*:\\s*(-?\\d+(?:\\.\\d+)?)");
      std::smatch m;
      if (std::regex_search(src, m, re) && m.size() > 1) {
        return std::stof(m[1].str());
      }
      return defVal;
    }

    int parseInt(const std::string &src, const std::string &key, int defVal) {
      std::regex re("\"" + key + "\"\\s*:\\s*(-?\\d+)");
      std::smatch m;
      if (std::regex_search(src, m, re) && m.size() > 1) {
        return std::stoi(m[1].str());
      }
      return defVal;
    }

    bool parseBool(const std::string &src, const std::string &key, bool defVal) {
      std::regex re("\"" + key + "\"\\s*:\\s*(true|false)");
      std::smatch m;
      if (std::regex_search(src, m, re) && m.size() > 1) {
        return m[1].str() == "true";
      }
      return defVal;
    }

    std::string parseString(const std::string &src, const std::string &key, const std::string &defVal) {
      std::regex re("\"" + key + "\"\\s*:\\s*\"([^\"]+)\"");
      std::smatch m;
      if (std::regex_search(src, m, re) && m.size() > 1) {
        return m[1].str();
      }
      return defVal;
    }

    // Extract object blocks inside the entities array. Assumes reasonably formatted JSON.
    std::vector<std::string> extractEntityBlocks(const std::string &content) {
      std::vector<std::string> blocks;
      std::size_t pos = content.find("\"entities\"");
      if (pos == std::string::npos) return blocks;
      pos = content.find('[', pos);
      if (pos == std::string::npos) return blocks;

      int depth = 0;
      std::size_t startObj = std::string::npos;
      for (std::size_t i = pos; i < content.size(); ++i) {
        char c = content[i];
        if (c == '{') {
          if (depth == 0) startObj = i;
          depth++;
        } else if (c == '}') {
          depth--;
          if (depth == 0 && startObj != std::string::npos) {
            blocks.emplace_back(content.substr(startObj, i - startObj + 1));
            startObj = std::string::npos;
          }
        } else if (c == ']' && depth == 0) {
          break;
        }
      }
      return blocks;
    }

    std::vector<std::string> extractObjectArrayBlocks(const std::string &content, const std::string &key) {
      std::vector<std::string> blocks;
      std::size_t pos = content.find("\"" + key + "\"");
      if (pos == std::string::npos) return blocks;
      pos = content.find('[', pos);
      if (pos == std::string::npos) return blocks;

      int depth = 0;
      std::size_t startObj = std::string::npos;
      for (std::size_t i = pos; i < content.size(); ++i) {
        char c = content[i];
        if (c == '{') {
          if (depth == 0) startObj = i;
          depth++;
        } else if (c == '}') {
          depth--;
          if (depth == 0 && startObj != std::string::npos) {
            blocks.emplace_back(content.substr(startObj, i - startObj + 1));
            startObj = std::string::npos;
          }
        } else if (c == ']' && depth == 0) {
          break;
        }
      }
      return blocks;
    }

    void serializeEntity(std::ostringstream &ss, const SceneEntity &e, int level) {
      ss << indent(level) << "{\n";
      ss << indent(level + 1) << "\"id\": \"" << e.id << "\",\n";
      ss << indent(level + 1) << "\"name\": \"" << e.name << "\",\n";
      ss << indent(level + 1) << "\"type\": \"" << toString(e.type) << "\",\n";
      ss << indent(level + 1) << "\"transform\": {\n";
      ss << indent(level + 2) << "\"position\": "; writeVec3(ss, e.transform.position); ss << ",\n";
      ss << indent(level + 2) << "\"rotation\": "; writeVec3(ss, e.transform.rotation); ss << ",\n";
      ss << indent(level + 2) << "\"scale\": "; writeVec3(ss, e.transform.scale); ss << "\n";
      ss << indent(level + 1) << "}";

      if (e.sprite) {
        const auto &s = *e.sprite;
        ss << ",\n" << indent(level + 1) << "\"sprite\": {\n";
        ss << indent(level + 2) << "\"spriteMeta\": \"" << s.spriteMeta << "\",\n";
        ss << indent(level + 2) << "\"spriteMetaGuid\": \"" << s.spriteMetaGuid << "\",\n";
        ss << indent(level + 2) << "\"state\": \"" << s.state << "\",\n";
        ss << indent(level + 2) << "\"billboard\": \"" << toString(s.billboard) << "\",\n";
        ss << indent(level + 2) << "\"layer\": " << s.layer << "\n";
        ss << indent(level + 1) << "}";
      }

      if (e.mesh) {
        const auto &m = *e.mesh;
        ss << ",\n" << indent(level + 1) << "\"mesh\": {\n";
        ss << indent(level + 2) << "\"model\": \"" << m.model << "\",\n";
        ss << indent(level + 2) << "\"modelGuid\": \"" << m.modelGuid << "\",\n";
        ss << indent(level + 2) << "\"material\": \"" << m.material << "\",\n";
        ss << indent(level + 2) << "\"materialGuid\": \"" << m.materialGuid << "\"";
        if (!m.nodeOverrides.empty()) {
          ss << ",\n" << indent(level + 2) << "\"nodeOverrides\": [\n";
          for (std::size_t i = 0; i < m.nodeOverrides.size(); ++i) {
            const auto &o = m.nodeOverrides[i];
            ss << indent(level + 3) << "{\n";
            ss << indent(level + 4) << "\"node\": " << o.node << ",\n";
            ss << indent(level + 4) << "\"position\": "; writeVec3(ss, o.transform.position); ss << ",\n";
            ss << indent(level + 4) << "\"rotation\": "; writeVec3(ss, o.transform.rotation); ss << ",\n";
            ss << indent(level + 4) << "\"scale\": "; writeVec3(ss, o.transform.scale); ss << "\n";
            ss << indent(level + 3) << "}";
            if (i + 1 < m.nodeOverrides.size()) {
              ss << ",";
            }
            ss << "\n";
          }
          ss << indent(level + 2) << "]\n";
          ss << indent(level + 1) << "}";
        } else {
          ss << "\n" << indent(level + 1) << "}";
        }
      }

      if (e.light) {
        const auto &l = *e.light;
        ss << ",\n" << indent(level + 1) << "\"light\": {\n";
        ss << indent(level + 2) << "\"kind\": \"" << toString(l.kind) << "\",\n";
        ss << indent(level + 2) << "\"color\": "; writeVec3(ss, l.color); ss << ",\n";
        ss << indent(level + 2) << "\"intensity\": " << l.intensity << ",\n";
        ss << indent(level + 2) << "\"range\": " << l.range << ",\n";
        ss << indent(level + 2) << "\"angle\": " << l.angle << "\n";
        ss << indent(level + 1) << "}";
      }

      if (e.camera) {
        const auto &c = *e.camera;
        ss << ",\n" << indent(level + 1) << "\"camera\": {\n";
        ss << indent(level + 2) << "\"projection\": \"" << c.projection << "\",\n";
        ss << indent(level + 2) << "\"fov\": " << c.fov << ",\n";
        ss << indent(level + 2) << "\"orthoHeight\": " << c.orthoHeight << ",\n";
        ss << indent(level + 2) << "\"near\": " << c.nearPlane << ",\n";
        ss << indent(level + 2) << "\"far\": " << c.farPlane << ",\n";
        ss << indent(level + 2) << "\"active\": " << (c.active ? "true" : "false") << "\n";
        ss << indent(level + 1) << "}";
      }

      ss << "\n" << indent(level) << "}";
    }

  } // namespace

  bool SceneSerializer::saveToFile(const Scene &scene, const std::string &path) {
    std::ostringstream ss;
    ss << "{\n";
    ss << indent(1) << "\"version\": " << scene.version << ",\n";
    ss << indent(1) << "\"resources\": {\n";
    ss << indent(2) << "\"basePath\": \"" << scene.resources.basePath << "\",\n";
    ss << indent(2) << "\"sprites\": \"" << scene.resources.spritePath << "\",\n";
    ss << indent(2) << "\"models\": \"" << scene.resources.modelPath << "\",\n";
    ss << indent(2) << "\"materials\": \"" << scene.resources.materialPath << "\"\n";
    ss << indent(1) << "},\n";
    ss << indent(1) << "\"entities\": [\n";
    for (std::size_t i = 0; i < scene.entities.size(); ++i) {
      serializeEntity(ss, scene.entities[i], 2);
      if (i + 1 < scene.entities.size()) {
        ss << ",";
      }
      ss << "\n";
    }
    ss << indent(1) << "]\n";
    ss << "}\n";
    return writeStringToFile(pathutil::fromUtf8(path), ss.str());
  }

  bool SceneSerializer::loadFromFile(const std::string &path, Scene &outScene) {
    const std::string content = readFileToString(pathutil::fromUtf8(path));
    if (content.empty()) return false;

    outScene.version = parseInt(content, "version", 1);
    outScene.resources.basePath = parseString(content, "basePath", "");
    outScene.resources.spritePath = parseString(content, "sprites", "");
    outScene.resources.modelPath = parseString(content, "models", "");
    outScene.resources.materialPath = parseString(content, "materials", "");

    const auto blocks = extractEntityBlocks(content);
    outScene.entities.clear();
    for (const auto &block : blocks) {
      SceneEntity e{};
      e.id = parseString(block, "id", "");
      e.name = parseString(block, "name", "");
      e.type = entityTypeFromString(parseString(block, "type", "mesh"));
      e.transform.position = parseVec3(block, "position", e.transform.position);
      e.transform.rotation = parseVec3(block, "rotation", e.transform.rotation);
      e.transform.scale = parseVec3(block, "scale", e.transform.scale);

      // sprite
      if (block.find("\"sprite\"") != std::string::npos) {
        SpriteComponent sc{};
        sc.spriteMeta = parseString(block, "spriteMeta", sc.spriteMeta);
        sc.spriteMetaGuid = parseString(block, "spriteMetaGuid", sc.spriteMetaGuid);
        sc.state = parseString(block, "state", sc.state);
        sc.billboard = billboardFromString(parseString(block, "billboard", toString(sc.billboard)));
        sc.layer = parseInt(block, "layer", sc.layer);
        e.sprite = sc;
      }

      if (block.find("\"mesh\"") != std::string::npos) {
        MeshComponent mc{};
        mc.model = parseString(block, "model", mc.model);
        mc.modelGuid = parseString(block, "modelGuid", mc.modelGuid);
        mc.material = parseString(block, "material", mc.material);
        mc.materialGuid = parseString(block, "materialGuid", mc.materialGuid);
        const auto overrideBlocks = extractObjectArrayBlocks(block, "nodeOverrides");
        for (const auto &overrideBlock : overrideBlocks) {
          MeshComponent::NodeOverride ov{};
          ov.node = parseInt(overrideBlock, "node", ov.node);
          ov.transform.position = parseVec3(overrideBlock, "position", ov.transform.position);
          ov.transform.rotation = parseVec3(overrideBlock, "rotation", ov.transform.rotation);
          ov.transform.scale = parseVec3(overrideBlock, "scale", ov.transform.scale);
          if (ov.node >= 0) {
            mc.nodeOverrides.push_back(std::move(ov));
          }
        }
        e.mesh = mc;
      }

      if (block.find("\"light\"") != std::string::npos) {
        LightComponent lc{};
        lc.kind = lightFromString(parseString(block, "kind", toString(lc.kind)));
        lc.color = parseVec3(block, "color", lc.color);
        lc.intensity = parseFloat(block, "intensity", lc.intensity);
        lc.range = parseFloat(block, "range", lc.range);
        lc.angle = parseFloat(block, "angle", lc.angle);
        e.light = lc;
      }

      if (block.find("\"camera\"") != std::string::npos) {
        CameraComponent cc{};
        cc.projection = parseString(block, "projection", cc.projection);
        cc.fov = parseFloat(block, "fov", cc.fov);
        cc.orthoHeight = parseFloat(block, "orthoHeight", cc.orthoHeight);
        cc.nearPlane = parseFloat(block, "near", cc.nearPlane);
        cc.farPlane = parseFloat(block, "far", cc.farPlane);
        cc.active = parseBool(block, "active", cc.active);
        e.camera = cc;
      }

      outScene.entities.push_back(std::move(e));
    }

    return true;
  }

} // namespace lve
