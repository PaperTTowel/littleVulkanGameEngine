#pragma once

#include "Engine/scene_system.hpp"
#include "Game/Tilemap/tiled_loader.hpp"

#include <cstdint>
#include <unordered_map>
#include <string>
#include <unordered_set>
#include <vector>

namespace lve::tilemap {
  class TilemapSystem {
  public:
    bool load(SceneSystem &sceneSystem, const std::string &mapPath, std::string *outError);
    bool resolveCollisions(const glm::vec3 &prevPosition, LveGameObject &character, bool allowGroundDrop);
    bool isLadderAtWorld(const glm::vec3 &position) const;
    bool isGroundAtWorld(const glm::vec3 &position) const;
    bool isWaterAtWorld(const glm::vec3 &position) const;
    bool getSignMessageAtWorld(const glm::vec3 &position, std::string &outMessage) const;
    void updateTriggers(const glm::vec3 &position);
    bool hasTileBounds() const { return hasBounds; }
    glm::vec3 getTileBoundsCenterWorld() const;
    std::string buildDebugString(const glm::vec3 &worldPos) const;
    const std::vector<glm::vec3> &getMobSpawnPointsWorld() const { return mobSpawnPoints; }
    bool hasPlayerSpawnWorld() const { return hasPlayerSpawnPoint; }
    glm::vec3 getPlayerSpawnWorld() const { return playerSpawnPoint; }

  private:
    using TileKey = std::int64_t;

    TileKey makeKey(int x, int y) const;
    bool isSolidAt(int x, int y) const;
    bool isGroundAt(int x, int y) const;
    bool isWaterAt(int x, int y) const;
    bool isLadderAt(int x, int y) const;
    bool isDoorAt(int x, int y) const;
    void worldToTile(const glm::vec3 &position, int &outX, int &outY) const;
    std::uint32_t getLayerGidAt(const TileLayer &layer, int tileX, int tileY) const;

    const Tileset *findTilesetForGid(std::uint32_t gid) const;

    TiledMap map{};
    float tileWorldWidth{1.0f};
    float tileWorldHeight{1.0f};
    std::unordered_set<TileKey> groundTiles;
    std::unordered_set<TileKey> waterTiles;
    std::unordered_set<TileKey> ladderTiles;
    std::unordered_set<TileKey> doorTiles;
    std::unordered_map<TileKey, std::string> signTiles;

    bool hasBounds{false};
    int minTileX{0};
    int maxTileX{0};
    int minTileY{0};
    int maxTileY{0};

    bool lastOnLadder{false};
    bool lastOnDoor{false};
    int lastTileX{0};
    int lastTileY{0};
    bool hasLastTile{false};

    std::vector<glm::vec3> mobSpawnPoints;
    bool hasPlayerSpawnPoint{false};
    glm::vec3 playerSpawnPoint{0.f, 0.f, 0.f};
  };
} // namespace lve::tilemap
