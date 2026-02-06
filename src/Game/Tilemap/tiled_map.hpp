#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace lve::tilemap {
  constexpr std::uint32_t kFlippedHorizontallyFlag = 0x80000000u;
  constexpr std::uint32_t kFlippedVerticallyFlag = 0x40000000u;
  constexpr std::uint32_t kFlippedDiagonallyFlag = 0x20000000u;
  constexpr std::uint32_t kFlipFlagsMask = 0xE0000000u;

  inline std::uint32_t clearFlipFlags(std::uint32_t gid) {
    return gid & ~kFlipFlagsMask;
  }

  struct Chunk {
    int x{0};
    int y{0};
    int width{0};
    int height{0};
    std::vector<std::uint32_t> gids;
  };

  struct TileLayer {
    std::string name;
    int id{0};
    int width{0};
    int height{0};
    int startx{0};
    int starty{0};
    bool visible{true};
    float opacity{1.0f};
    std::vector<Chunk> chunks;
  };

  struct Object {
    int id{0};
    std::string name;
    std::string type;
    float x{0.f};
    float y{0.f};
    float width{0.f};
    float height{0.f};
    bool visible{true};
  };

  struct ObjectLayer {
    std::string name;
    int id{0};
    bool visible{true};
    float opacity{1.0f};
    std::vector<Object> objects;
  };

  struct Tileset {
    int firstGid{0};
    std::string source;
    std::string name;
    int tileWidth{0};
    int tileHeight{0};
    int tileCount{0};
    int columns{0};
    std::string image;
    int imageWidth{0};
    int imageHeight{0};
  };

  struct TiledMap {
    int width{0};
    int height{0};
    int tileWidth{0};
    int tileHeight{0};
    bool infinite{false};
    std::vector<TileLayer> layers;
    std::vector<ObjectLayer> objectLayers;
    std::vector<Tileset> tilesets;
  };
} // namespace lve::tilemap
