#pragma once

// libs
#include <glm/vec2.hpp>

// std
#include <string>
#include <unordered_map>

namespace lve {

  struct SpriteStateInfo {
    int row = 0;
    int startFrame = 0;
    int frameCount = 1;
    float frameDuration = 0.15f;
    bool loop = true;
    int atlasCols = 0; // optional override; if 0 use parent atlasCols
    int atlasRows = 0; // optional override; if 0 use parent atlasRows
    std::string texturePath; // optional per-state texture
  };

  struct SpriteMetadata {
    std::string texturePath; // optional default texture (single atlas)
    int atlasCols = 1;
    int atlasRows = 1;
    float pixelsPerUnit = 1.0f;
    float hp = 1.0f;
    float spawnInterval = 0.0f;
    glm::vec2 size{1.f, 1.f};   // original pixel size of a single frame
    glm::vec2 pivot{0.5f, 0.5f}; // normalized pivot (0..1)
    std::unordered_map<std::string, SpriteStateInfo> states;
  };

  // very small JSON subset loader tailored for sprite metadata
  bool loadSpriteMetadata(const std::string &filepath, SpriteMetadata &outMetadata);

} // namespace lve
