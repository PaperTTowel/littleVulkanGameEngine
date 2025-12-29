#include "sprite_animator.hpp"

// std
#include <iostream>

namespace lve {

  namespace {
    const char *defaultStateName(ObjectState state) {
      return (state == ObjectState::WALKING) ? "walking" : "idle";
    }

    std::string resolveStateName(const SpriteMetadata &metadata, const std::string &stateName) {
      if (!stateName.empty() && metadata.states.find(stateName) != metadata.states.end()) {
        return stateName;
      }
      auto idleIt = metadata.states.find("idle");
      if (idleIt != metadata.states.end()) {
        return idleIt->first;
      }
      if (!metadata.states.empty()) {
        return metadata.states.begin()->first;
      }
      return {};
    }
  } // namespace

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

  bool SpriteAnimator::applySpriteState(LveGameObject &character, const std::string &stateName) {
    const std::string resolvedName = resolveStateName(metadata, stateName);
    if (resolvedName.empty()) {
      std::cerr << "Sprite metadata has no states to apply\n";
      return false;
    }
    auto it = metadata.states.find(resolvedName);
    if (it == metadata.states.end()) {
      std::cerr << "Sprite state missing in metadata: " << resolvedName << "\n";
      return false;
    }

    const SpriteStateInfo &stateInfo = it->second;

    std::string texturePath = stateInfo.texturePath;
    if (texturePath.empty()) {
      std::cerr << "Sprite state missing texture path: " << resolvedName << "\n";
      return false;
    }

    std::shared_ptr<LveTexture> texture;
    try {
      texture = loadTextureCached(texturePath);
    } catch (const std::exception &e) {
      std::cerr << "Failed to load sprite texture: " << texturePath << " - " << e.what() << "\n";
      return false;
    }

    character.diffuseMap = texture;
    character.atlasColumns = (stateInfo.atlasCols > 0) ? stateInfo.atlasCols : metadata.atlasCols;
    character.atlasRows = (stateInfo.atlasRows > 0) ? stateInfo.atlasRows : metadata.atlasRows;

    const bool stateChanged = (resolvedName != character.spriteStateName);
    character.spriteState = stateInfo;
    character.hasSpriteState = true;
    character.spriteStateName = resolvedName;

    float aspect = (metadata.size.y != 0.f) ? (metadata.size.x / metadata.size.y) : 1.f;
    character.transform.scale = glm::vec3(aspect, 1.f, 1.f);
    character.transformDirty = true;

    if (stateChanged) {
      character.currentFrame = 0;
      character.animationTimeAccumulator = 0.0f;
    }
    currentTexturePath = texturePath;
    return true;
  }

  bool SpriteAnimator::applySpriteState(LveGameObject &character, ObjectState desiredState) {
    return applySpriteState(character, defaultStateName(desiredState));
  }

} // namespace lve
