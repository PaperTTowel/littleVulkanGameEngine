#include "Game/background_system.hpp"

// std
#include <array>
#include <cmath>
#include <iostream>
#include <utility>

namespace lve::game {
  namespace {
    struct PixelSize {
      float width;
      float height;
    };

    constexpr int kSunRenderOrder = -9010;
    constexpr int kCloudRenderOrder = -9000;
    constexpr int kWindRenderOrder = -8990;
    constexpr float kSkyZ = -0.92f;
    constexpr float kSunMarginX = 1.3f;
    constexpr float kSunMarginY = 1.0f;
    constexpr float kPixelsPerUnit = 8.0f;
    constexpr float kSunScaleFactor = 0.34f;
    constexpr float kCloudScaleFactor = 0.48f;
    constexpr float kWindScaleFactor = 1.25f;
    constexpr float kWindWrapPadding = 3.0f;
    constexpr float kCloudWrapPadding = 2.5f;
    constexpr float kReferenceOrthoHeight = 10.0f;

    constexpr PixelSize kSunPixelSize{35.0f, 17.0f};
    constexpr std::array<PixelSize, 4> kCloudPixelSizes{{
      {29.0f, 12.0f},
      {31.0f, 11.0f},
      {38.0f, 15.0f},
      {43.0f, 15.0f},
    }};
    constexpr std::array<PixelSize, 4> kWindPixelSizes{{
      {8.0f, 1.0f},
      {13.0f, 1.0f},
      {6.0f, 3.0f},
      {9.0f, 4.0f},
    }};

    glm::vec3 pixelScale(float widthPx, float heightPx, float factor = 1.0f) {
      return {
        (widthPx / kPixelsPerUnit) * factor,
        (heightPx / kPixelsPerUnit) * factor,
        1.0f};
    }
  } // namespace

  BackgroundSystem::BackgroundSystem(std::string texturePath)
    : texturePath(std::move(texturePath)) {}

  void BackgroundSystem::init(SceneSystem &sceneSystem) {
    if (initialized) {
      return;
    }

    auto bgTexture = sceneSystem.loadTextureCached(texturePath);
    if (!bgTexture) {
      std::cerr << "Background texture missing; skipping background sprite\n";
      return;
    }

    auto &bg = sceneSystem.createTileSpriteObject(
      {0.f, 0.f, -1.f},
      bgTexture,
      1,
      1,
      0,
      0,
      {1.f, 1.f, 1.f},
      -10000);
    bg.name = "Background";

    backgroundId = bg.getId();
    uvScroll = {0.f, 0.f};
    cloudTime = 0.f;
    windTime = 0.f;

    auto sunTexture = sceneSystem.loadTextureCached(sunTexturePath);

    std::vector<std::shared_ptr<backend::RenderTexture>> cloudTextures{};
    cloudTextures.reserve(cloudTexturePaths.size());
    for (const auto &path : cloudTexturePaths) {
      auto tex = sceneSystem.loadTextureCached(path);
      if (tex) {
        cloudTextures.push_back(std::move(tex));
      }
    }

    std::vector<std::shared_ptr<backend::RenderTexture>> windTextures{};
    windTextures.reserve(windTexturePaths.size());
    for (const auto &path : windTexturePaths) {
      auto tex = sceneSystem.loadTextureCached(path);
      if (tex) {
        windTextures.push_back(std::move(tex));
      }
    }

    if (sunTexture) {
      const glm::vec3 sunScale = pixelScale(
        kSunPixelSize.width,
        kSunPixelSize.height,
        kSunScaleFactor);
      auto &sun = sceneSystem.createTileSpriteObject(
        {0.f, 0.f, kSkyZ},
        sunTexture,
        1,
        1,
        0,
        0,
        sunScale,
        kSunRenderOrder);
      sun.name = "Sky.Sun";
      sunId = sun.getId();
    } else {
      std::cerr << "Sun texture missing: " << sunTexturePath << "\n";
    }

    clouds.clear();
    if (!cloudTextures.empty()) {
      struct CloudSeed {
        std::size_t textureIndex;
        float speed;
        float baseOffsetX;
        float baseOffsetY;
        float bobAmplitude;
        float bobFrequency;
        float phase;
        float scale;
      };

      const std::array<CloudSeed, 7> kCloudSeeds{{
        {0, 0.55f, 1.0f, 1.8f, 0.07f, 0.90f, 0.1f, 1.00f},
        {1, 0.42f, 5.6f, 2.4f, 0.06f, 0.65f, 1.3f, 1.00f},
        {2, 0.33f, 10.4f, 1.5f, 0.07f, 0.75f, 2.0f, 1.05f},
        {3, 0.48f, 14.8f, 2.0f, 0.05f, 0.95f, 2.7f, 1.10f},
        {1, 0.28f, 19.2f, 1.3f, 0.06f, 0.70f, 3.4f, 0.95f},
        {0, 0.37f, 24.0f, 2.6f, 0.05f, 0.80f, 4.1f, 0.95f},
        {3, 0.52f, 29.0f, 1.6f, 0.06f, 1.00f, 4.8f, 1.05f},
      }};

      clouds.reserve(kCloudSeeds.size());
      for (std::size_t i = 0; i < kCloudSeeds.size(); ++i) {
        const auto &seed = kCloudSeeds[i];
        const std::size_t textureIndex = seed.textureIndex % cloudTextures.size();
        const auto &cloudTexture = cloudTextures[textureIndex];
        const PixelSize pixelSize = kCloudPixelSizes[textureIndex % kCloudPixelSizes.size()];
        const glm::vec3 cloudScale = pixelScale(
          pixelSize.width,
          pixelSize.height,
          kCloudScaleFactor * seed.scale);
        auto &cloud = sceneSystem.createTileSpriteObject(
          {0.f, 0.f, kSkyZ},
          cloudTexture,
          1,
          1,
          0,
          0,
          cloudScale,
          kCloudRenderOrder);
        cloud.name = "Sky.Cloud." + std::to_string(i);

        CloudInstance instance{};
        instance.id = cloud.getId();
        instance.speed = seed.speed;
        instance.baseOffsetX = seed.baseOffsetX;
        instance.baseOffsetY = seed.baseOffsetY;
        instance.bobAmplitude = seed.bobAmplitude;
        instance.bobFrequency = seed.bobFrequency;
        instance.phase = seed.phase;
        instance.scaleX = cloudScale.x;
        instance.scaleY = cloudScale.y;
        clouds.push_back(instance);
      }
    } else {
      std::cerr << "Cloud textures missing in Assets/textures/background\n";
    }

    winds.clear();
    if (!windTextures.empty()) {
      struct WindSeed {
        std::size_t textureIndex;
        float speed;
        float baseOffsetX;
        float baseOffsetY;
        float bobAmplitude;
        float bobFrequency;
        float phase;
        float scale;
      };

      const std::array<WindSeed, 5> kWindSeeds{{
        {0, 1.10f, 1.2f, 0.8f, 0.05f, 1.00f, 0.2f, 1.00f},
        {1, 1.25f, 8.8f, 1.1f, 0.04f, 1.15f, 0.9f, 1.00f},
        {2, 1.00f, 16.4f, 0.6f, 0.06f, 0.85f, 1.6f, 0.95f},
        {3, 1.40f, 24.6f, 1.4f, 0.04f, 1.25f, 2.2f, 1.10f},
        {1, 1.18f, 32.0f, 0.9f, 0.05f, 0.95f, 2.9f, 1.00f},
      }};

      winds.reserve(kWindSeeds.size());
      for (std::size_t i = 0; i < kWindSeeds.size(); ++i) {
        const auto &seed = kWindSeeds[i];
        const std::size_t textureIndex = seed.textureIndex % windTextures.size();
        const auto &windTexture = windTextures[textureIndex];
        const PixelSize pixelSize = kWindPixelSizes[textureIndex % kWindPixelSizes.size()];
        const glm::vec3 windScale = pixelScale(
          pixelSize.width,
          pixelSize.height,
          kWindScaleFactor * seed.scale);
        auto &wind = sceneSystem.createTileSpriteObject(
          {0.f, 0.f, kSkyZ},
          windTexture,
          1,
          1,
          0,
          0,
          windScale,
          kWindRenderOrder);
        wind.name = "Sky.Wind." + std::to_string(i);

        WindInstance instance{};
        instance.id = wind.getId();
        instance.speed = seed.speed;
        instance.baseOffsetX = seed.baseOffsetX;
        instance.baseOffsetY = seed.baseOffsetY;
        instance.bobAmplitude = seed.bobAmplitude;
        instance.bobFrequency = seed.bobFrequency;
        instance.phase = seed.phase;
        instance.scaleX = windScale.x;
        instance.scaleY = windScale.y;
        winds.push_back(instance);
      }
    } else {
      std::cerr << "Wind textures missing in Assets/textures/background\n";
    }

    initialized = true;
  }

  void BackgroundSystem::update(
    SceneSystem &sceneSystem,
    const glm::vec3 &focusPosition,
    float orthoWidth,
    float orthoHeight,
    float dt) {
    if (!initialized) {
      init(sceneSystem);
      if (!initialized) {
        return;
      }
    }

    auto *bg = sceneSystem.findObject(backgroundId);
    if (!bg) {
      initialized = false;
      return;
    }

    bg->transform.translation = {focusPosition.x, focusPosition.y, -1.f};
    bg->transform.scale = {
      orthoWidth * tuning.scalePadding,
      orthoHeight * tuning.scalePadding,
      1.f};

    const float uvStepX = (bg->transform.scale.x > 0.f)
      ? ((tuning.scrollSpeed * dt) / bg->transform.scale.x)
      : 0.f;
    const float uvStepY = (bg->transform.scale.y > 0.f)
      ? ((tuning.scrollSpeed * dt) / bg->transform.scale.y)
      : 0.f;
    uvScroll.x = std::fmod(uvScroll.x + uvStepX, 1.0f);
    uvScroll.y = std::fmod(uvScroll.y + uvStepY, 1.0f);
    if (uvScroll.x < 0.f) uvScroll.x += 1.0f;
    if (uvScroll.y < 0.f) uvScroll.y += 1.0f;

    bg->uvOffset = uvScroll;
    bg->transformDirty = true;

    const float left = focusPosition.x - (orthoWidth * 0.5f);
    const float top = focusPosition.y - (orthoHeight * 0.5f);
    const float zoomScale = (orthoHeight > 0.f) ? (orthoHeight / kReferenceOrthoHeight) : 1.f;
    const float sunMarginX = kSunMarginX * zoomScale;
    const float sunMarginY = kSunMarginY * zoomScale;
    const float cloudWrapPadding = kCloudWrapPadding * zoomScale;
    const float windWrapPadding = kWindWrapPadding * zoomScale;

    if (auto *sun = sceneSystem.findObject(sunId)) {
      const glm::vec3 sunScale = pixelScale(
        kSunPixelSize.width,
        kSunPixelSize.height,
        kSunScaleFactor * zoomScale);
      sun->transform.translation = {
        left + sunMarginX,
        top + sunMarginY,
        kSkyZ};
      sun->transform.scale = sunScale;
      sun->transformDirty = true;
    }

    cloudTime += dt;
    const float cloudSpan = orthoWidth + (cloudWrapPadding * 2.f);
    if (cloudSpan > 0.f) {
      for (auto &cloud : clouds) {
        auto *cloudObj = sceneSystem.findObject(cloud.id);
        if (!cloudObj) {
          continue;
        }

        const float xPhase = (cloud.baseOffsetX + (cloudTime * cloud.speed)) * zoomScale;
        float xTravel = std::fmod(xPhase, cloudSpan);
        if (xTravel < 0.f) {
          xTravel += cloudSpan;
        }

        const float yBob =
          std::sin(cloud.phase + (cloudTime * cloud.bobFrequency)) * (cloud.bobAmplitude * zoomScale);
        cloudObj->transform.translation = {
          left - cloudWrapPadding + xTravel,
          top + (cloud.baseOffsetY * zoomScale) + yBob,
          kSkyZ};
        cloudObj->transform.scale = {cloud.scaleX * zoomScale, cloud.scaleY * zoomScale, 1.f};
        cloudObj->transformDirty = true;
      }
    }

    windTime += dt;
    const float windSpan = orthoWidth + (windWrapPadding * 2.f);
    if (windSpan > 0.f) {
      for (auto &wind : winds) {
        auto *windObj = sceneSystem.findObject(wind.id);
        if (!windObj) {
          continue;
        }

        const float xPhase = (wind.baseOffsetX + (windTime * wind.speed)) * zoomScale;
        float xTravel = std::fmod(xPhase, windSpan);
        if (xTravel < 0.f) {
          xTravel += windSpan;
        }

        const float yBob =
          std::sin(wind.phase + (windTime * wind.bobFrequency)) * (wind.bobAmplitude * zoomScale);
        windObj->transform.translation = {
          left - windWrapPadding + xTravel,
          top + (wind.baseOffsetY * zoomScale) + yBob,
          kSkyZ};
        windObj->transform.scale = {wind.scaleX * zoomScale, wind.scaleY * zoomScale, 1.f};
        windObj->transformDirty = true;
      }
    }
  }

} // namespace lve::game
