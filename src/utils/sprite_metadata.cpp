#include "sprite_metadata.hpp"
#include "Engine/path_utils.hpp"

// std
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>

namespace lve {
  namespace {
    std::string readFileToString(const std::filesystem::path &path) {
      std::ifstream file(path, std::ios::in | std::ios::binary);
      if (!file) {
        return {};
      }
      std::ostringstream ss;
      ss << file.rdbuf();
      return ss.str();
    }

    int parseInt(const std::string &src, const std::string &key, int defaultValue) {
      std::regex re("\"" + key + "\"\\s*:\\s*(-?\\d+)");
      std::smatch match;
      if (std::regex_search(src, match, re) && match.size() > 1) {
        return std::stoi(match[1].str());
      }
      return defaultValue;
    }

    float parseFloat(const std::string &src, const std::string &key, float defaultValue) {
      std::regex re("\"" + key + "\"\\s*:\\s*(-?\\d+(?:\\.\\d+)?)");
      std::smatch match;
      if (std::regex_search(src, match, re) && match.size() > 1) {
        return std::stof(match[1].str());
      }
      return defaultValue;
    }

    bool parseBool(const std::string &src, const std::string &key, bool defaultValue) {
      std::regex re("\"" + key + "\"\\s*:\\s*(true|false)");
      std::smatch match;
      if (std::regex_search(src, match, re) && match.size() > 1) {
        return match[1].str() == "true";
      }
      return defaultValue;
    }

    std::string parseString(const std::string &src, const std::string &key, const std::string &defaultValue) {
      std::regex re("\"" + key + "\"\\s*:\\s*\"([^\"]+)\"");
      std::smatch match;
      if (std::regex_search(src, match, re) && match.size() > 1) {
        return match[1].str();
      }
      return defaultValue;
    }

    std::string parseStringBefore(const std::string &src, const std::string &key, std::size_t endPos, const std::string &defaultValue) {
      const std::string slice = (endPos == std::string::npos) ? src : src.substr(0, endPos);
      return parseString(slice, key, defaultValue);
    }

    int parseIntBefore(const std::string &src, const std::string &key, std::size_t endPos, int defaultValue) {
      const std::string slice = (endPos == std::string::npos) ? src : src.substr(0, endPos);
      return parseInt(slice, key, defaultValue);
    }

    glm::vec2 parseVec2(const std::string &src, const std::string &key, glm::vec2 defaultValue) {
      std::regex re("\"" + key + "\"\\s*:\\s*\\[\\s*(-?\\d+(?:\\.\\d+)?)\\s*,\\s*(-?\\d+(?:\\.\\d+)?)\\s*\\]");
      std::smatch match;
      if (std::regex_search(src, match, re) && match.size() > 2) {
        return glm::vec2{std::stof(match[1].str()), std::stof(match[2].str())};
      }
      return defaultValue;
    }

    std::string extractStatesBlock(const std::string &content) {
      std::size_t pos = content.find("\"states\"");
      if (pos == std::string::npos) return {};

      // find first '{' after "states"
      pos = content.find('{', pos);
      if (pos == std::string::npos) return {};

      int depth = 0;
      std::size_t start = pos;
      for (std::size_t i = pos; i < content.size(); ++i) {
        if (content[i] == '{') depth++;
        else if (content[i] == '}') {
          depth--;
          if (depth == 0) {
            return content.substr(start + 1, i - start - 1); // inner block without outer braces
          }
        }
      }
      return {};
    }
  } // namespace
  
  bool loadSpriteMetadata(const std::string &filepath, SpriteMetadata &outMetadata) {
    const std::string content = readFileToString(pathutil::fromUtf8(filepath));
    if (content.empty()) {
      return false;
    }

    const std::size_t statesPos = content.find("\"states\"");
    outMetadata.texturePath = parseStringBefore(content, "texture", statesPos, outMetadata.texturePath);
    outMetadata.atlasCols = parseIntBefore(content, "cols", statesPos, outMetadata.atlasCols);
    outMetadata.atlasRows = parseIntBefore(content, "rows", statesPos, outMetadata.atlasRows);
    outMetadata.pixelsPerUnit = parseFloat(content, "pixelsPerUnit", outMetadata.pixelsPerUnit);
    outMetadata.pixelsPerUnit = parseFloat(content, "ppu", outMetadata.pixelsPerUnit);
    outMetadata.hp = parseFloat(content, "hp", outMetadata.hp);
    outMetadata.spawnInterval = parseFloat(content, "spawnInterval", outMetadata.spawnInterval);
    outMetadata.size = parseVec2(content, "size", outMetadata.size);
    outMetadata.pivot = parseVec2(content, "pivot", outMetadata.pivot);

    // parse states object (simple brace matching to capture entire states block)
    const std::string statesBody = extractStatesBlock(content);
    if (!statesBody.empty()) {

      std::regex stateRe("\"([^\"]+)\"\\s*:\\s*\\{([\\s\\S]*?)\\}");
      auto it = std::sregex_iterator(statesBody.begin(), statesBody.end(), stateRe);
      auto end = std::sregex_iterator();
      for (; it != end; ++it) {
        const std::smatch &m = *it;
        if (m.size() < 3) continue;
        const std::string stateName = m[1].str();
        const std::string body = m[2].str();

        SpriteStateInfo state{};
        state.texturePath = parseString(body, "texture", "");
        state.row = parseInt(body, "row", state.row);
        state.startFrame = parseInt(body, "startFrame", state.startFrame);
        state.startFrame = parseInt(body, "start", state.startFrame);
        state.frameCount = parseInt(body, "frames", state.frameCount);
        float fps = parseFloat(body, "fps", 0.0f);
        if (fps > 0.0f) {
          state.frameDuration = 1.0f / fps;
        } else {
          state.frameDuration = parseFloat(body, "frameDuration", state.frameDuration);
        }
        state.loop = parseBool(body, "loop", state.loop);
        state.atlasCols = parseInt(body, "cols", state.atlasCols);
        state.atlasRows = parseInt(body, "rows", state.atlasRows);
        outMetadata.states[stateName] = state;
      }
    }

    return true;
  }

} // namespace lve
