#include "Game/Tilemap/tilemap_system.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iostream>
#include <sstream>
#include <string_view>
#include <vector>

namespace lve::tilemap {
  namespace {
    bool layerNameEquals(const std::string &name, const char *candidate) {
      return name == candidate;
    }

    bool startsWith(const std::string &text, std::string_view prefix) {
      if (text.size() < prefix.size()) {
        return false;
      }
      return std::equal(prefix.begin(), prefix.end(), text.begin());
    }

    std::string trimAscii(std::string value) {
      std::size_t start = 0;
      while (start < value.size() &&
             std::isspace(static_cast<unsigned char>(value[start])) != 0) {
        ++start;
      }

      std::size_t end = value.size();
      while (end > start &&
             std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
      }

      return value.substr(start, end - start);
    }
  } // namespace

  TilemapSystem::TileKey TilemapSystem::makeKey(int x, int y) const {
    return (static_cast<TileKey>(x) << 32) ^ static_cast<TileKey>(static_cast<std::uint32_t>(y));
  }

  bool TilemapSystem::isSolidAt(int x, int y) const {
    const TileKey key = makeKey(x, y);
    return waterTiles.count(key) > 0 || groundTiles.count(key) > 0;
  }

  bool TilemapSystem::isGroundAt(int x, int y) const {
    return groundTiles.count(makeKey(x, y)) > 0;
  }

  bool TilemapSystem::isWaterAt(int x, int y) const {
    return waterTiles.count(makeKey(x, y)) > 0;
  }

  bool TilemapSystem::isLadderAt(int x, int y) const {
    return ladderTiles.count(makeKey(x, y)) > 0;
  }

  bool TilemapSystem::isDoorAt(int x, int y) const {
    return doorTiles.count(makeKey(x, y)) > 0;
  }

  bool TilemapSystem::isLadderAtWorld(const glm::vec3 &position) const {
    int tileX = 0;
    int tileY = 0;
    worldToTile(position, tileX, tileY);
    return isLadderAt(tileX, tileY);
  }

  bool TilemapSystem::isGroundAtWorld(const glm::vec3 &position) const {
    int tileX = 0;
    int tileY = 0;
    worldToTile(position, tileX, tileY);
    return isGroundAt(tileX, tileY);
  }

  bool TilemapSystem::isWaterAtWorld(const glm::vec3 &position) const {
    int tileX = 0;
    int tileY = 0;
    worldToTile(position, tileX, tileY);
    return isWaterAt(tileX, tileY);
  }

  bool TilemapSystem::getSignMessageAtWorld(const glm::vec3 &position, std::string &outMessage) const {
    int centerX = 0;
    int centerY = 0;
    worldToTile(position, centerX, centerY);

    for (int dy = -1; dy <= 1; ++dy) {
      for (int dx = -1; dx <= 1; ++dx) {
        const auto it = signTiles.find(makeKey(centerX + dx, centerY + dy));
        if (it == signTiles.end()) {
          continue;
        }
        outMessage = it->second;
        return !outMessage.empty();
      }
    }

    outMessage.clear();
    return false;
  }

  void TilemapSystem::worldToTile(const glm::vec3 &position, int &outX, int &outY) const {
    const float x = position.x / tileWorldWidth;
    const float y = position.y / tileWorldHeight;
    outX = static_cast<int>(std::floor(x));
    outY = static_cast<int>(std::floor(y));
  }

  std::uint32_t TilemapSystem::getLayerGidAt(const TileLayer &layer, int tileX, int tileY) const {
    for (const auto &chunk : layer.chunks) {
      if (tileX < chunk.x || tileX >= chunk.x + chunk.width) {
        continue;
      }
      if (tileY < chunk.y || tileY >= chunk.y + chunk.height) {
        continue;
      }
      const int localX = tileX - chunk.x;
      const int localY = tileY - chunk.y;
      const int index = localY * chunk.width + localX;
      if (index < 0 || index >= static_cast<int>(chunk.gids.size())) {
        return 0;
      }
      return chunk.gids[static_cast<std::size_t>(index)];
    }
    return 0;
  }

  const Tileset *TilemapSystem::findTilesetForGid(std::uint32_t gid) const {
    if (map.tilesets.empty()) return nullptr;
    const std::uint32_t cleanGid = clearFlipFlags(gid);
    const Tileset *result = nullptr;
    for (const auto &tileset : map.tilesets) {
      if (tileset.firstGid <= static_cast<int>(cleanGid)) {
        result = &tileset;
      }
    }
    return result;
  }

  bool TilemapSystem::load(SceneSystem &sceneSystem, const std::string &mapPath, std::string *outError) {
    map = TiledMap{};
    groundTiles.clear();
    waterTiles.clear();
    ladderTiles.clear();
    doorTiles.clear();
    signTiles.clear();
    mobSpawnPoints.clear();
    hasPlayerSpawnPoint = false;
    playerSpawnPoint = {0.f, 0.f, 0.f};
    hasBounds = false;
    hasLastTile = false;
    lastOnLadder = false;
    lastOnDoor = false;

    if (!loadFromFile(mapPath, map, outError)) {
      return false;
    }

    std::sort(map.tilesets.begin(), map.tilesets.end(), [](const Tileset &a, const Tileset &b) {
      return a.firstGid < b.firstGid;
    });

    if (map.tileHeight > 0) {
      tileWorldHeight = 1.0f;
      tileWorldWidth = (map.tileWidth > 0)
        ? static_cast<float>(map.tileWidth) / static_cast<float>(map.tileHeight)
        : 1.0f;
    }

    const glm::vec3 tileScale{tileWorldWidth, tileWorldHeight, 1.0f};

    int layerIndex = 0;
    for (const auto &layer : map.layers) {
      const bool isGround = layerNameEquals(layer.name, "Ground") ||
        layerNameEquals(layer.name, "1F") ||
        layerNameEquals(layer.name, "2F");
      const bool isWater = layerNameEquals(layer.name, "Water");
      const bool isLadder = layerNameEquals(layer.name, "Ladder");
      const bool isDoor = layerNameEquals(layer.name, "Door");
      const bool isSign = startsWith(layer.name, "Sign:");
      const std::string signMessage = isSign ? trimAscii(layer.name.substr(5)) : std::string{};

      const int renderOrder = layerIndex * 10;

      for (const auto &chunk : layer.chunks) {
        for (int y = 0; y < chunk.height; ++y) {
          for (int x = 0; x < chunk.width; ++x) {
            const int index = y * chunk.width + x;
            if (index < 0 || index >= static_cast<int>(chunk.gids.size())) {
              continue;
            }
            std::uint32_t gid = chunk.gids[static_cast<std::size_t>(index)];
            const std::uint32_t cleanGid = clearFlipFlags(gid);
            if (cleanGid == 0) continue;

            const int tileX = chunk.x + x;
            const int tileY = chunk.y + y;
            const TileKey key = makeKey(tileX, tileY);

            if (!hasBounds) {
              minTileX = tileX;
              maxTileX = tileX;
              minTileY = tileY;
              maxTileY = tileY;
              hasBounds = true;
            } else {
              minTileX = std::min(minTileX, tileX);
              maxTileX = std::max(maxTileX, tileX);
              minTileY = std::min(minTileY, tileY);
              maxTileY = std::max(maxTileY, tileY);
            }

            if (isGround) groundTiles.insert(key);
            if (isWater) waterTiles.insert(key);
            if (isLadder) ladderTiles.insert(key);
            if (isDoor) doorTiles.insert(key);
            if (isSign) signTiles[key] = signMessage;

            const Tileset *tileset = findTilesetForGid(cleanGid);
            if (!tileset) continue;

            int columns = tileset->columns > 0 ? tileset->columns : 1;
            int rows = tileset->tileCount > 0 ? (tileset->tileCount + columns - 1) / columns : 0;
            if (rows <= 0 && tileset->tileHeight > 0 && tileset->imageHeight > 0) {
              rows = tileset->imageHeight / tileset->tileHeight;
            }
            if (rows <= 0) rows = 1;

            const int tileId = static_cast<int>(cleanGid) - tileset->firstGid;
            if (tileId < 0) continue;
            const int col = tileId % columns;
            const int row = tileId / columns;
            const int flippedRow = (rows > 0) ? (rows - 1 - row) : row;

            const float worldX = (static_cast<float>(tileX) + 0.5f) * tileWorldWidth;
            const float worldY = (static_cast<float>(tileY) + 0.5f) * tileWorldHeight;
            const glm::vec3 position{worldX, worldY, 0.0f};

            auto texture = sceneSystem.loadTextureCached(tileset->image);
            auto &tileObj = sceneSystem.createTileSpriteObject(
              position,
              texture,
              columns,
              rows,
              flippedRow,
              col,
              tileScale,
              renderOrder);

            int uvFlags = 0;
            if ((gid & kFlippedHorizontallyFlag) != 0) {
              uvFlags |= LveGameObject::kUvTransformFlipHorizontal;
            }
            if ((gid & kFlippedVerticallyFlag) != 0) {
              uvFlags |= LveGameObject::kUvTransformFlipVertical;
            }
            if ((gid & kFlippedDiagonallyFlag) != 0) {
              uvFlags |= LveGameObject::kUvTransformFlipDiagonal;
            }
            tileObj.uvTransformFlags = uvFlags;
            tileObj.directions = Direction::RIGHT;
          }
        }
      }
      layerIndex++;
    }

    if (map.tileHeight > 0) {
      const float pixelToWorld = 1.0f / static_cast<float>(map.tileHeight);
      for (const auto &objLayer : map.objectLayers) {
        const bool isMobSpawn = (objLayer.name == "MobSpawn");
        const bool isPlayerSpawn = (objLayer.name == "PlayerSpawn");
        if (!isMobSpawn && !isPlayerSpawn) {
          continue;
        }
        for (const auto &obj : objLayer.objects) {
          if (!obj.visible) {
            continue;
          }
          const float worldX = (obj.x + (obj.width * 0.5f)) * pixelToWorld;
          const float worldY = (obj.y + (obj.height * 0.5f)) * pixelToWorld;
          if (isMobSpawn) {
            mobSpawnPoints.push_back({worldX, worldY, 0.0f});
          } else if (isPlayerSpawn && !hasPlayerSpawnPoint) {
            playerSpawnPoint = {worldX, worldY, 0.0f};
            hasPlayerSpawnPoint = true;
          }
        }
      }
    }

    return true;
  }

  glm::vec3 TilemapSystem::getTileBoundsCenterWorld() const {
    if (!hasBounds) {
      return {};
    }
    const float centerTileX = (static_cast<float>(minTileX + maxTileX) * 0.5f) + 0.5f;
    const float centerTileY = (static_cast<float>(minTileY + maxTileY) * 0.5f) + 0.5f;
    return {
      centerTileX * tileWorldWidth,
      centerTileY * tileWorldHeight,
      0.0f
    };
  }

  std::string TilemapSystem::buildDebugString(const glm::vec3 &worldPos) const {
    if (map.layers.empty()) {
      return {};
    }
    int tileX = 0;
    int tileY = 0;
    worldToTile(worldPos, tileX, tileY);

    std::ostringstream ss;
    ss << "Tile: (" << tileX << ", " << tileY << ")\n";
    ss << "World: (" << worldPos.x << ", " << worldPos.y << ")\n";

    bool any = false;
    for (const auto &layer : map.layers) {
      if (!layer.visible) {
        continue;
      }
      const std::uint32_t gid = getLayerGidAt(layer, tileX, tileY);
      if (gid == 0) {
        continue;
      }
      const std::uint32_t cleanGid = clearFlipFlags(gid);
      const Tileset *tileset = findTilesetForGid(cleanGid);
      const int localId = tileset ? static_cast<int>(cleanGid) - tileset->firstGid : -1;
      const int columns = tileset && tileset->columns > 0 ? tileset->columns : 1;
      const int col = (localId >= 0) ? (localId % columns) : -1;
      const int row = (localId >= 0) ? (localId / columns) : -1;
      const bool flipH = (gid & kFlippedHorizontallyFlag) != 0;
      const bool flipV = (gid & kFlippedVerticallyFlag) != 0;
      const bool flipD = (gid & kFlippedDiagonallyFlag) != 0;

      ss << "[" << layer.name << "] gid=" << gid
         << " local=" << localId
         << " col=" << col
         << " row=" << row;
      if (tileset) {
        ss << " tileset=" << tileset->name
           << " first=" << tileset->firstGid
           << " cols=" << tileset->columns
           << " count=" << tileset->tileCount;
      } else {
        ss << " tileset=none";
      }
      if (flipH || flipV || flipD) {
        ss << " flip(" << (flipH ? "H" : "")
           << (flipV ? "V" : "")
           << (flipD ? "D" : "") << ")";
      }
      ss << "\n";
      any = true;
    }

    if (!any) {
      ss << "No tile on visible layers.";
    }
    return ss.str();
  }

  bool TilemapSystem::resolveCollisions(
    const glm::vec3 &prevPosition,
    LveGameObject &character,
    bool allowGroundDrop) {
    bool collided = false;
    int tileX = 0;
    int tileY = 0;
    worldToTile(character.transform.translation, tileX, tileY);
    if (isWaterAt(tileX, tileY)) {
      character.transform.translation = prevPosition;
      character.transformDirty = true;
      return true;
    }

    const bool onLadder = isLadderAt(tileX, tileY);
    const float halfWidth = character.transform.scale.x * 0.5f;
    const float halfHeight = character.transform.scale.y * 0.5f;
    const float edgeEpsilon = 0.01f;

    if (!onLadder) {
      if (character.transform.translation.x > prevPosition.x) {
        const glm::vec3 rightTop{
          character.transform.translation.x + halfWidth + edgeEpsilon,
          character.transform.translation.y - halfHeight + edgeEpsilon,
          0.f};
        const glm::vec3 rightBottom{
          character.transform.translation.x + halfWidth + edgeEpsilon,
          character.transform.translation.y + halfHeight - edgeEpsilon,
          0.f};
        if (isGroundAtWorld(rightTop) || isGroundAtWorld(rightBottom)) {
          character.transform.translation.x = prevPosition.x;
          character.transformDirty = true;
          collided = true;
        }
      } else if (character.transform.translation.x < prevPosition.x) {
        const glm::vec3 leftTop{
          character.transform.translation.x - halfWidth - edgeEpsilon,
          character.transform.translation.y - halfHeight + edgeEpsilon,
          0.f};
        const glm::vec3 leftBottom{
          character.transform.translation.x - halfWidth - edgeEpsilon,
          character.transform.translation.y + halfHeight - edgeEpsilon,
          0.f};
        if (isGroundAtWorld(leftTop) || isGroundAtWorld(leftBottom)) {
          character.transform.translation.x = prevPosition.x;
          character.transformDirty = true;
          collided = true;
        }
      }
    }

    if (!onLadder && character.transform.translation.y < prevPosition.y) {
      const glm::vec3 headLeft{
        character.transform.translation.x - halfWidth + edgeEpsilon,
        character.transform.translation.y - halfHeight - edgeEpsilon,
        0.f};
      const glm::vec3 headRight{
        character.transform.translation.x + halfWidth - edgeEpsilon,
        character.transform.translation.y - halfHeight - edgeEpsilon,
        0.f};
      if (isGroundAtWorld(headLeft) || isGroundAtWorld(headRight)) {
        character.transform.translation.y = prevPosition.y;
        character.transformDirty = true;
        collided = true;
      }
    }

    if (character.transform.translation.y > prevPosition.y) {
      const float footOffset = (character.transform.scale.y * 0.5f) + 0.01f;
      const glm::vec3 footPos = character.transform.translation + glm::vec3(0.f, footOffset, 0.f);
      int footX = 0;
      int footY = 0;
      worldToTile(footPos, footX, footY);
      if (isGroundAt(footX, footY)) {
        const bool ladderHere = isLadderAt(footX, footY);
        if (!(allowGroundDrop && ladderHere)) {
          character.transform.translation.y = prevPosition.y;
          character.transformDirty = true;
          collided = true;
        }
      }
    }
    return collided;
  }

  void TilemapSystem::updateTriggers(const glm::vec3 &position) {
    int tileX = 0;
    int tileY = 0;
    worldToTile(position, tileX, tileY);
    const bool onLadder = isLadderAt(tileX, tileY);
    const bool onDoor = isDoorAt(tileX, tileY);

    if (!hasLastTile || tileX != lastTileX || tileY != lastTileY) {
      if (onLadder && !lastOnLadder) {
        std::cout << "[Trigger] Ladder entered at (" << tileX << ", " << tileY << ")\n";
      }
      if (onDoor && !lastOnDoor) {
        std::cout << "[Trigger] Door entered at (" << tileX << ", " << tileY << ")\n";
      }
      if (!onLadder && lastOnLadder) {
        std::cout << "[Trigger] Ladder exited at (" << lastTileX << ", " << lastTileY << ")\n";
      }
      if (!onDoor && lastOnDoor) {
        std::cout << "[Trigger] Door exited at (" << lastTileX << ", " << lastTileY << ")\n";
      }
    }

    lastTileX = tileX;
    lastTileY = tileY;
    lastOnLadder = onLadder;
    lastOnDoor = onDoor;
    hasLastTile = true;
  }

} // namespace lve::tilemap
