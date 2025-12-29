#pragma once

#include "utils/game_object.hpp"
#include "Engine/Backend/render_assets.hpp"
#include "utils/sprite_metadata.hpp"

// std
#include <memory>
#include <string>
#include <unordered_map>

namespace lve {

  class SpriteAnimator {
  public:
    explicit SpriteAnimator(backend::RenderAssetFactory &assets, SpriteMetadata meta);

    bool applySpriteState(LveGameObject &character, const std::string &stateName);
    bool applySpriteState(LveGameObject &character, ObjectState desiredState);
    const std::string &getCurrentTexturePath() const { return currentTexturePath; }
    const SpriteMetadata &getMetadata() const { return metadata; }

  private:
    std::shared_ptr<backend::RenderTexture> loadTextureCached(const std::string &path);

    backend::RenderAssetFactory &assets;
    SpriteMetadata metadata;
    std::unordered_map<std::string, std::shared_ptr<backend::RenderTexture>> textureCache;
    std::string currentTexturePath;
  };

} // namespace lve

