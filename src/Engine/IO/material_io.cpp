#include "Engine/IO/material_io.hpp"

#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>

namespace lve {

  namespace {
    std::string readFileToString(const std::string &path) {
      std::ifstream file(path, std::ios::in | std::ios::binary);
      if (!file) return {};
      std::ostringstream ss;
      ss << file.rdbuf();
      return ss.str();
    }

    bool writeStringToFile(const std::string &path, const std::string &data) {
      std::ofstream file(path, std::ios::out | std::ios::binary | std::ios::trunc);
      if (!file) return false;
      file << data;
      return true;
    }

    std::string parseString(const std::string &src, const std::string &key, const std::string &defVal) {
      std::regex re("\"" + key + "\"\\s*:\\s*\"([^\"]*)\"");
      std::smatch m;
      if (std::regex_search(src, m, re) && m.size() > 1) {
        return m[1].str();
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

    glm::vec3 parseVec3(const std::string &src, const std::string &key, const glm::vec3 &defVal) {
      std::regex re("\"" + key + "\"\\s*:\\s*\\[\\s*(-?\\d+(?:\\.\\d+)?)\\s*,\\s*(-?\\d+(?:\\.\\d+)?)\\s*,\\s*(-?\\d+(?:\\.\\d+)?)\\s*\\]");
      std::smatch m;
      if (std::regex_search(src, m, re) && m.size() >= 4) {
        return glm::vec3{std::stof(m[1].str()), std::stof(m[2].str()), std::stof(m[3].str())};
      }
      return defVal;
    }

    glm::vec4 parseVec4(const std::string &src, const std::string &key, const glm::vec4 &defVal) {
      std::regex re("\"" + key + "\"\\s*:\\s*\\[\\s*(-?\\d+(?:\\.\\d+)?)\\s*,\\s*(-?\\d+(?:\\.\\d+)?)\\s*,\\s*(-?\\d+(?:\\.\\d+)?)\\s*,\\s*(-?\\d+(?:\\.\\d+)?)\\s*\\]");
      std::smatch m;
      if (std::regex_search(src, m, re) && m.size() >= 5) {
        return glm::vec4{
          std::stof(m[1].str()),
          std::stof(m[2].str()),
          std::stof(m[3].str()),
          std::stof(m[4].str())};
      }
      return defVal;
    }

    std::string normalizeSlashes(const std::string &value) {
      std::string out = value;
      for (auto &ch : out) {
        if (ch == '\\') {
          ch = '/';
        }
      }
      return out;
    }

    MaterialData parseMaterialData(const std::string &content, const MaterialData &base) {
      MaterialData data = base;
      data.version = parseInt(content, "version", data.version);
      data.name = parseString(content, "name", data.name);
      data.textures.baseColor = parseString(content, "baseColorTexture", data.textures.baseColor);
      data.textures.normal = parseString(content, "normalTexture", data.textures.normal);
      data.textures.metallicRoughness = parseString(content, "metallicRoughnessTexture", data.textures.metallicRoughness);
      data.textures.occlusion = parseString(content, "occlusionTexture", data.textures.occlusion);
      data.textures.emissive = parseString(content, "emissiveTexture", data.textures.emissive);
      data.factors.baseColor = parseVec4(content, "baseColorFactor", data.factors.baseColor);
      data.factors.metallic = parseFloat(content, "metallicFactor", data.factors.metallic);
      data.factors.roughness = parseFloat(content, "roughnessFactor", data.factors.roughness);
      data.factors.emissive = parseVec3(content, "emissiveFactor", data.factors.emissive);
      data.factors.occlusionStrength = parseFloat(content, "occlusionStrength", data.factors.occlusionStrength);
      data.factors.normalScale = parseFloat(content, "normalScale", data.factors.normalScale);
      return data;
    }
  } // namespace

  bool saveMaterialToFile(
    const std::string &path,
    const MaterialData &data,
    std::string *outError) {
    if (path.empty()) {
      if (outError) {
        *outError = "Material path is empty";
      }
      return false;
    }

    std::filesystem::path outputPath(path);
    std::error_code ec;
    if (outputPath.has_parent_path()) {
      std::filesystem::create_directories(outputPath.parent_path(), ec);
    }
    if (ec) {
      if (outError) {
        *outError = "Failed to create material directory";
      }
      return false;
    }

    const std::string materialName = data.name.empty()
      ? outputPath.stem().string()
      : data.name;

    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"version\": " << data.version << ",\n";
    ss << "  \"name\": \"" << materialName << "\",\n";
    ss << "  \"baseColorTexture\": \"" << normalizeSlashes(data.textures.baseColor) << "\",\n";
    ss << "  \"normalTexture\": \"" << normalizeSlashes(data.textures.normal) << "\",\n";
    ss << "  \"metallicRoughnessTexture\": \"" << normalizeSlashes(data.textures.metallicRoughness) << "\",\n";
    ss << "  \"occlusionTexture\": \"" << normalizeSlashes(data.textures.occlusion) << "\",\n";
    ss << "  \"emissiveTexture\": \"" << normalizeSlashes(data.textures.emissive) << "\",\n";
    ss << "  \"baseColorFactor\": [" << data.factors.baseColor.r << ", " << data.factors.baseColor.g << ", "
       << data.factors.baseColor.b << ", " << data.factors.baseColor.a << "],\n";
    ss << "  \"metallicFactor\": " << data.factors.metallic << ",\n";
    ss << "  \"roughnessFactor\": " << data.factors.roughness << ",\n";
    ss << "  \"emissiveFactor\": [" << data.factors.emissive.r << ", " << data.factors.emissive.g << ", "
       << data.factors.emissive.b << "],\n";
    ss << "  \"occlusionStrength\": " << data.factors.occlusionStrength << ",\n";
    ss << "  \"normalScale\": " << data.factors.normalScale << "\n";
    ss << "}\n";

    if (!writeStringToFile(path, ss.str())) {
      if (outError) {
        *outError = "Failed to write material file";
      }
      return false;
    }
    return true;
  }

  bool loadMaterialDataFromFile(
    const std::string &path,
    MaterialData &outData,
    std::string *outError,
    const std::function<std::string(const std::string &)> &pathResolver) {
    const std::string resolvedPath = pathResolver ? pathResolver(path) : path;
    const std::string content = readFileToString(resolvedPath);
    if (content.empty()) {
      if (outError) {
        *outError = "Failed to read material file";
      }
      return false;
    }

    outData = parseMaterialData(content, outData);
    return true;
  }

} // namespace lve
