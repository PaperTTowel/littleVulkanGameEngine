#include "sprite_animator.hpp"

// std
#include <iostream>

namespace lve {

  SpriteAnimator::SpriteAnimator(LveDevice &device, SpriteMetadata meta)
    : device{device}, metadata{std::move(meta)} {}

  std::shared_ptr<LveTexture> SpriteAnimator::loadTextureCached(const std::string &path) {
    auto it = textureCache.find(path);
    if (it != textureCache.end()) {
      return it->second;
    }
    auto uniqueTex = LveTexture::createTextureFromFile(device, path);
    auto sharedTex = std::shared_ptr<LveTexture>(std::move(uniqueTex));
    textureCache[path] = sharedTex;
    return sharedTex;
  }

  void SpriteAnimator::applySpriteState(LveGameObject &character, ObjectState desiredState) {
    const std::string stateKey = (desiredState == ObjectState::WALKING) ? "walking" : "idle";
    auto it = metadata.states.find(stateKey);
    if (it == metadata.states.end()) {
      std::cerr << "Sprite state missing in metadata: " << stateKey << "\n";
      return;
    }

    const SpriteStateInfo &stateInfo = it->second;

    std::string texturePath = stateInfo.texturePath;
    if (texturePath.empty()) {
      std::cerr << "Sprite state missing texture path: " << stateKey << "\n";
      return;
    }

    std::shared_ptr<LveTexture> texture;
    try {
      texture = loadTextureCached(texturePath);
    } catch (const std::exception &e) {
      std::cerr << "Failed to load sprite texture: " << texturePath << " - " << e.what() << "\n";
      return;
    }

    character.diffuseMap = texture;
    character.atlasColumns = (stateInfo.atlasCols > 0) ? stateInfo.atlasCols : metadata.atlasCols;
    character.atlasRows = (stateInfo.atlasRows > 0) ? stateInfo.atlasRows : metadata.atlasRows;

    character.spriteStates.clear();
    character.spriteStates[static_cast<int>(desiredState)] = stateInfo;
    character.spriteStateName = stateKey;

    float aspect = (metadata.size.y != 0.f) ? (metadata.size.x / metadata.size.y) : 1.f;
    character.transform.scale = glm::vec3(aspect, 1.f, 1.f);
    character.transformDirty = true;

    if (desiredState != currentState) {
      character.currentFrame = 0;
      character.animationTimeAccumulator = 0.0f;
    }
    currentState = desiredState;
    currentTexturePath = texturePath;
  }

} // namespace lve
