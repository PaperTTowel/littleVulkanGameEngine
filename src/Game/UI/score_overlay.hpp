#pragma once

#include "Engine/camera.hpp"
#include "Engine/scene_system.hpp"
#include "utils/game_object.hpp"

#include <cctype>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace lve {
  namespace game::ui {
    class ScoreOverlay {
    public:
      void init(SceneSystem &sceneSystem) {
        ensureInitialized(sceneSystem);
      }

      void update(
        SceneSystem &sceneSystem,
        const LveCamera &camera,
        float orthoWidth,
        float orthoHeight,
        int score) {
        if (!ensureInitialized(sceneSystem) || !available) {
          return;
        }

        const std::string text = "SCORE " + std::to_string(score);
        const glm::vec3 cameraPos = camera.getPosition();
        const float zoomScale = (orthoHeight > 0.f) ? (orthoHeight / kReferenceOrthoHeight) : 1.f;
        const float glyphWorldSize = kGlyphWorldSize * zoomScale;
        const float glyphSpacing = kGlyphSpacing * zoomScale;
        const float marginX = kMarginX * zoomScale;
        const float marginY = kMarginY * zoomScale;
        const float stepX = glyphWorldSize + glyphSpacing;
        const float startX = cameraPos.x - (orthoWidth * 0.5f) + marginX + (glyphWorldSize * 0.5f);
        const float y = cameraPos.y - (orthoHeight * 0.5f) + marginY + (glyphWorldSize * 0.5f);

        for (std::size_t i = 0; i < text.size(); ++i) {
          auto *glyph = ensureGlyphObject(sceneSystem, i);
          if (!glyph) {
            continue;
          }

          glyph->transform.translation = {startX + (stepX * static_cast<float>(i)), y, kHudZ};
          glyph->transform.scale = {glyphWorldSize, glyphWorldSize, 1.f};
          glyph->renderOrder = kRenderOrder;
          glyph->diffuseMap = texture;
          glyph->enableTextureType = 1;
          glyph->atlasColumns = kAtlasCols;
          glyph->atlasRows = kAtlasRows;
          glyph->currentFrame = 0;
          glyph->directions = Direction::RIGHT;
          glyph->uvTransformFlags = 0;

          int col = 0;
          int topRow = 0;
          if (resolveGlyph(text[i], col, topRow)) {
            glyph->hasSpriteState = true;
            glyph->spriteState.row = toShaderRow(topRow);
            glyph->spriteState.startFrame = col;
            glyph->spriteState.frameCount = 1;
          } else {
            glyph->transform.scale = {0.f, 0.f, 1.f};
          }

          glyph->transformDirty = true;
        }

        for (std::size_t i = text.size(); i < glyphIds.size(); ++i) {
          hideGlyph(sceneSystem, i);
        }
      }

    private:
      static constexpr const char *kUiTexturePath = "Assets/textures/tileset/UI.png";
      static constexpr int kAtlasCols = 26;
      static constexpr int kAtlasRows = 19;
      static constexpr int kDigitTopRow = 14;
      static constexpr int kAlphaTopRowUpper = 17;
      static constexpr int kAlphaTopRowLower = 18;
      static constexpr int kRenderOrder = 50000;
      static constexpr float kGlyphWorldSize = 0.22f;
      static constexpr float kGlyphSpacing = 0.05f;
      static constexpr float kMarginX = 0.22f;
      static constexpr float kMarginY = 0.20f;
      static constexpr float kHudZ = -0.2f;
      static constexpr float kReferenceOrthoHeight = 10.0f;

      bool ensureInitialized(SceneSystem &sceneSystem) {
        if (initialized) {
          return available;
        }

        texture = sceneSystem.loadTextureCached(kUiTexturePath);
        available = static_cast<bool>(texture);
        initialized = true;
        return available;
      }

      bool resolveGlyph(char ch, int &outCol, int &outTopRow) const {
        if (ch >= '0' && ch <= '9') {
          outCol = ch - '0';
          outTopRow = kDigitTopRow;
          return true;
        }

        const unsigned char uch = static_cast<unsigned char>(ch);
        const char upper = static_cast<char>(std::toupper(uch));
        if (upper >= 'A' && upper <= 'Z') {
          const int alphaIndex = upper - 'A';
          if (alphaIndex < 13) {
            outCol = alphaIndex;
            outTopRow = kAlphaTopRowUpper;
          } else {
            outCol = alphaIndex - 13;
            outTopRow = kAlphaTopRowLower;
          }
          return true;
        }

        return false;
      }

      int toShaderRow(int topRow) const {
        if (topRow < 0) {
          topRow = 0;
        }
        if (topRow >= kAtlasRows) {
          topRow = kAtlasRows - 1;
        }
        return (kAtlasRows - 1) - topRow;
      }

      LveGameObject *ensureGlyphObject(SceneSystem &sceneSystem, std::size_t index) {
        if (index < glyphIds.size()) {
          if (auto *existing = sceneSystem.findObject(glyphIds[index])) {
            return existing;
          }
        }

        auto &obj = sceneSystem.createTileSpriteObject(
          {0.f, 0.f, kHudZ},
          texture,
          kAtlasCols,
          kAtlasRows,
          toShaderRow(kDigitTopRow),
          0,
          {0.f, 0.f, 1.f},
          kRenderOrder);
        obj.name = "HUD.ScoreGlyph";
        obj.enableTextureType = 1;
        obj.transformDirty = true;

        if (index < glyphIds.size()) {
          glyphIds[index] = obj.getId();
        } else {
          glyphIds.push_back(obj.getId());
        }
        return &obj;
      }

      void hideGlyph(SceneSystem &sceneSystem, std::size_t index) {
        if (index >= glyphIds.size()) {
          return;
        }
        auto *obj = sceneSystem.findObject(glyphIds[index]);
        if (!obj) {
          return;
        }
        obj->transform.scale = {0.f, 0.f, 1.f};
        obj->transformDirty = true;
      }

      bool initialized{false};
      bool available{false};
      std::shared_ptr<backend::RenderTexture> texture{};
      std::vector<LveGameObject::id_t> glyphIds{};
    };
  } // namespace game::ui
} // namespace lve
