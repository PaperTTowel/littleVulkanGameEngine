#pragma once

#include "utils/game_object.hpp"
#include "Engine/Backend/device.hpp"
#include "utils/sprite_metadata.hpp"

// std
#include <memory>
#include <string>
#include <unordered_map>

namespace lve {

  class SpriteAnimator {
  public:
    explicit SpriteAnimator(LveDevice &device, SpriteMetadata meta);

    void applySpriteState(LveGameObject &character, ObjectState desiredState);
    const std::string &getCurrentTexturePath() const { return currentTexturePath; }
    const SpriteMetadata &getMetadata() const { return metadata; }

  private:
    std::shared_ptr<LveTexture> loadTextureCached(const std::string &path);

    LveDevice &device;
    SpriteMetadata metadata;
    std::unordered_map<std::string, std::shared_ptr<LveTexture>> textureCache;
    ObjectState currentState{ObjectState::IDLE};
    std::string currentTexturePath;
  };

} // namespace lve
