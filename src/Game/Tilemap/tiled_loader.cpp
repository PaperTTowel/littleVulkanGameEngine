#include "Game/Tilemap/tiled_loader.hpp"
#include "Engine/path_utils.hpp"

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

#include <stb_image.h>

#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace lve::tilemap {
  namespace {
    void setError(std::string *outError, const std::string &message) {
      if (outError) {
        *outError = message;
      }
    }

    std::string readFileToString(const std::filesystem::path &path) {
      std::ifstream file(path, std::ios::in | std::ios::binary);
      if (!file) return {};
      std::ostringstream ss;
      ss << file.rdbuf();
      return ss.str();
    }

    std::string normalizePath(const std::filesystem::path &path) {
      return pathutil::toGenericUtf8(path);
    }

    std::string makeAssetRelative(const std::string &path) {
      if (path.rfind("Assets/", 0) == 0) {
        return path;
      }
      const std::size_t pos = path.find("/Assets/");
      if (pos != std::string::npos) {
        return path.substr(pos + 1);
      }
      return path;
    }

    std::string extractXmlAttribute(const std::string &tag, const std::string &key) {
      std::regex re(key + R"RX(="([^"]*)")RX");
      std::smatch match;
      if (std::regex_search(tag, match, re) && match.size() > 1) {
        return match[1].str();
      }
      return {};
    }

    bool parseTsx(const std::filesystem::path &tsxPath, Tileset &tileset, std::string *outError) {
      const std::string content = readFileToString(tsxPath);
      if (content.empty()) {
        setError(outError, "failed to read tsx: " + pathutil::toUtf8(tsxPath));
        return false;
      }

      std::smatch tilesetMatch;
      if (!std::regex_search(content, tilesetMatch, std::regex(R"(<tileset[^>]*>)"))) {
        setError(outError, "tsx missing <tileset> tag: " + pathutil::toUtf8(tsxPath));
        return false;
      }
      const std::string tilesetTag = tilesetMatch.str();
      const std::string name = extractXmlAttribute(tilesetTag, "name");
      if (!name.empty()) tileset.name = name;
      const std::string tileWidth = extractXmlAttribute(tilesetTag, "tilewidth");
      const std::string tileHeight = extractXmlAttribute(tilesetTag, "tileheight");
      const std::string tileCount = extractXmlAttribute(tilesetTag, "tilecount");
      const std::string columns = extractXmlAttribute(tilesetTag, "columns");
      if (!tileWidth.empty()) tileset.tileWidth = std::stoi(tileWidth);
      if (!tileHeight.empty()) tileset.tileHeight = std::stoi(tileHeight);
      if (!tileCount.empty()) tileset.tileCount = std::stoi(tileCount);
      if (!columns.empty()) tileset.columns = std::stoi(columns);

      std::smatch imageMatch;
      if (std::regex_search(content, imageMatch, std::regex(R"(<image[^>]*>)"))) {
        const std::string imageTag = imageMatch.str();
        const std::string source = extractXmlAttribute(imageTag, "source");
        const std::string width = extractXmlAttribute(imageTag, "width");
        const std::string height = extractXmlAttribute(imageTag, "height");
        if (!width.empty()) tileset.imageWidth = std::stoi(width);
        if (!height.empty()) tileset.imageHeight = std::stoi(height);

        if (!source.empty()) {
          std::filesystem::path imagePath = pathutil::fromUtf8(source);
          if (imagePath.is_relative()) {
            imagePath = tsxPath.parent_path() / imagePath;
          }
          tileset.image = makeAssetRelative(normalizePath(imagePath));
        }
      }

      return true;
    }

    bool decodeBase64(const std::string &input, std::vector<std::uint8_t> &out, std::string *outError) {
      static int8_t kDecodeTable[256];
      static bool tableInit = false;
      if (!tableInit) {
        for (int i = 0; i < 256; ++i) {
          kDecodeTable[i] = static_cast<int8_t>(-1);
        }
        const char *alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        for (int i = 0; i < 64; ++i) {
          kDecodeTable[static_cast<unsigned char>(alphabet[i])] = static_cast<int8_t>(i);
        }
        tableInit = true;
      }

      out.clear();
      out.reserve((input.size() * 3) / 4);
      int value = 0;
      int valueBits = -8;
      for (unsigned char c : input) {
        if (std::isspace(c)) {
          continue;
        }
        if (c == '=') {
          break;
        }
        int8_t decoded = kDecodeTable[c];
        if (decoded < 0) {
          setError(outError, "base64 decode failed: invalid character");
          return false;
        }
        value = (value << 6) | decoded;
        valueBits += 6;
        if (valueBits >= 0) {
          out.push_back(static_cast<std::uint8_t>((value >> valueBits) & 0xFF));
          valueBits -= 8;
        }
      }
      return true;
    }

    bool decodeZlib(const std::vector<std::uint8_t> &input, std::vector<std::uint8_t> &out, std::string *outError) {
      out.clear();
      if (input.empty()) {
        setError(outError, "zlib decode failed: empty input");
        return false;
      }
      int outLen = 0;
      char *decoded = stbi_zlib_decode_malloc(
        reinterpret_cast<const char *>(input.data()),
        static_cast<int>(input.size()),
        &outLen);
      if (!decoded || outLen <= 0) {
        setError(outError, "zlib decode failed");
        if (decoded) {
          stbi_image_free(decoded);
        }
        return false;
      }
      out.assign(decoded, decoded + outLen);
      stbi_image_free(decoded);
      return true;
    }

    bool bytesToGids(
      const std::vector<std::uint8_t> &bytes,
      std::vector<std::uint32_t> &outGids,
      std::string *outError) {
      if (bytes.size() % 4 != 0) {
        setError(outError, "tile data size is not a multiple of 4 bytes");
        return false;
      }
      const std::size_t count = bytes.size() / 4;
      outGids.clear();
      outGids.reserve(count);
      for (std::size_t i = 0; i < bytes.size(); i += 4) {
        std::uint32_t gid = 0;
        gid |= static_cast<std::uint32_t>(bytes[i]);
        gid |= static_cast<std::uint32_t>(bytes[i + 1]) << 8;
        gid |= static_cast<std::uint32_t>(bytes[i + 2]) << 16;
        gid |= static_cast<std::uint32_t>(bytes[i + 3]) << 24;
        outGids.push_back(gid);
      }
      return true;
    }

    bool parseChunkData(
      const rapidjson::Value &chunkValue,
      const std::string &encoding,
      const std::string &compression,
      int expectedCount,
      std::vector<std::uint32_t> &outGids,
      std::string *outError) {
      if (!chunkValue.HasMember("data")) {
        setError(outError, "chunk missing data");
        return false;
      }
      const auto &dataValue = chunkValue["data"];
      if (dataValue.IsArray()) {
        outGids.clear();
        outGids.reserve(dataValue.Size());
        for (auto &v : dataValue.GetArray()) {
          if (!v.IsInt()) {
            setError(outError, "tile data array contains non-integer value");
            return false;
          }
          outGids.push_back(static_cast<std::uint32_t>(v.GetInt()));
        }
        if (expectedCount > 0 && static_cast<int>(outGids.size()) != expectedCount) {
          setError(outError, "tile data size does not match expected count");
          return false;
        }
        return true;
      }
      if (!dataValue.IsString()) {
        setError(outError, "tile data is not a string");
        return false;
      }
      std::string encoded = dataValue.GetString();
      if (encoding != "base64") {
        setError(outError, "unsupported tile encoding: " + encoding);
        return false;
      }

      std::vector<std::uint8_t> base64Bytes;
      if (!decodeBase64(encoded, base64Bytes, outError)) {
        return false;
      }

      std::vector<std::uint8_t> rawBytes;
      if (compression.empty()) {
        rawBytes = std::move(base64Bytes);
      } else if (compression == "zlib") {
        if (!decodeZlib(base64Bytes, rawBytes, outError)) {
          return false;
        }
      } else {
        setError(outError, "unsupported tile compression: " + compression);
        return false;
      }

      if (!bytesToGids(rawBytes, outGids, outError)) {
        return false;
      }
      if (expectedCount > 0 && static_cast<int>(outGids.size()) != expectedCount) {
        setError(outError, "tile data size does not match expected count");
        return false;
      }
      return true;
    }
  } // namespace

  bool loadFromFile(const std::string &path, TiledMap &outMap, std::string *outError) {
    outMap = TiledMap{};
    if (path.empty()) {
      setError(outError, "empty map path");
      return false;
    }

    const std::filesystem::path mapPath = pathutil::fromUtf8(path);
    const std::filesystem::path mapDir = mapPath.parent_path();

    std::string content = readFileToString(mapPath);
    if (content.empty()) {
      setError(outError, "failed to read map file: " + path);
      return false;
    }

    rapidjson::Document doc;
    doc.Parse(content.c_str());
    if (doc.HasParseError()) {
      std::string msg = rapidjson::GetParseError_En(doc.GetParseError());
      setError(outError, "json parse error: " + msg);
      return false;
    }
    if (!doc.IsObject()) {
      setError(outError, "map root is not an object");
      return false;
    }

    if (doc.HasMember("width") && doc["width"].IsInt()) outMap.width = doc["width"].GetInt();
    if (doc.HasMember("height") && doc["height"].IsInt()) outMap.height = doc["height"].GetInt();
    if (doc.HasMember("tilewidth") && doc["tilewidth"].IsInt()) outMap.tileWidth = doc["tilewidth"].GetInt();
    if (doc.HasMember("tileheight") && doc["tileheight"].IsInt()) outMap.tileHeight = doc["tileheight"].GetInt();
    if (doc.HasMember("infinite") && doc["infinite"].IsBool()) outMap.infinite = doc["infinite"].GetBool();

    if (doc.HasMember("tilesets") && doc["tilesets"].IsArray()) {
      for (auto &ts : doc["tilesets"].GetArray()) {
        if (!ts.IsObject()) continue;
        Tileset tileset{};
        if (ts.HasMember("firstgid") && ts["firstgid"].IsInt()) tileset.firstGid = ts["firstgid"].GetInt();
        if (ts.HasMember("source") && ts["source"].IsString()) tileset.source = ts["source"].GetString();
        if (ts.HasMember("name") && ts["name"].IsString()) tileset.name = ts["name"].GetString();
        if (ts.HasMember("tilewidth") && ts["tilewidth"].IsInt()) tileset.tileWidth = ts["tilewidth"].GetInt();
        if (ts.HasMember("tileheight") && ts["tileheight"].IsInt()) tileset.tileHeight = ts["tileheight"].GetInt();
        if (ts.HasMember("tilecount") && ts["tilecount"].IsInt()) tileset.tileCount = ts["tilecount"].GetInt();
        if (ts.HasMember("columns") && ts["columns"].IsInt()) tileset.columns = ts["columns"].GetInt();
        if (ts.HasMember("image") && ts["image"].IsString()) tileset.image = ts["image"].GetString();
        if (ts.HasMember("imagewidth") && ts["imagewidth"].IsInt()) tileset.imageWidth = ts["imagewidth"].GetInt();
        if (ts.HasMember("imageheight") && ts["imageheight"].IsInt()) tileset.imageHeight = ts["imageheight"].GetInt();
        if (!tileset.source.empty()) {
          std::filesystem::path tsxPath = pathutil::fromUtf8(tileset.source);
          if (tsxPath.is_relative()) {
            tsxPath = mapDir / tsxPath;
          }
          tsxPath = tsxPath.lexically_normal();

          if (!std::filesystem::exists(tsxPath)) {
            const std::filesystem::path fallback =
              (mapDir / ".." / "textures" / "tileset" / tsxPath.filename()).lexically_normal();
            if (std::filesystem::exists(fallback)) {
              tsxPath = fallback;
            }
          }

          tileset.source = normalizePath(tsxPath);
          if (!parseTsx(tsxPath, tileset, outError)) {
            return false;
          }
        }

        outMap.tilesets.push_back(std::move(tileset));
      }
    }

    if (doc.HasMember("layers") && doc["layers"].IsArray()) {
      for (auto &layerVal : doc["layers"].GetArray()) {
        if (!layerVal.IsObject()) continue;
        if (!layerVal.HasMember("type") || !layerVal["type"].IsString()) continue;
        const std::string layerType = layerVal["type"].GetString();
        if (layerType == "tilelayer") {

          TileLayer layer{};
          if (layerVal.HasMember("name") && layerVal["name"].IsString()) layer.name = layerVal["name"].GetString();
          if (layerVal.HasMember("id") && layerVal["id"].IsInt()) layer.id = layerVal["id"].GetInt();
          if (layerVal.HasMember("width") && layerVal["width"].IsInt()) layer.width = layerVal["width"].GetInt();
          if (layerVal.HasMember("height") && layerVal["height"].IsInt()) layer.height = layerVal["height"].GetInt();
          if (layerVal.HasMember("startx") && layerVal["startx"].IsInt()) layer.startx = layerVal["startx"].GetInt();
          if (layerVal.HasMember("starty") && layerVal["starty"].IsInt()) layer.starty = layerVal["starty"].GetInt();
          if (layerVal.HasMember("visible") && layerVal["visible"].IsBool()) layer.visible = layerVal["visible"].GetBool();
          if (layerVal.HasMember("opacity") && layerVal["opacity"].IsNumber()) layer.opacity = layerVal["opacity"].GetFloat();

          std::string encoding;
          std::string compression;
          if (layerVal.HasMember("encoding") && layerVal["encoding"].IsString()) {
            encoding = layerVal["encoding"].GetString();
          }
          if (layerVal.HasMember("compression") && layerVal["compression"].IsString()) {
            compression = layerVal["compression"].GetString();
          }

          if (layerVal.HasMember("chunks") && layerVal["chunks"].IsArray()) {
            for (auto &chunkVal : layerVal["chunks"].GetArray()) {
              if (!chunkVal.IsObject()) continue;
              Chunk chunk{};
              if (chunkVal.HasMember("x") && chunkVal["x"].IsInt()) chunk.x = chunkVal["x"].GetInt();
              if (chunkVal.HasMember("y") && chunkVal["y"].IsInt()) chunk.y = chunkVal["y"].GetInt();
              if (chunkVal.HasMember("width") && chunkVal["width"].IsInt()) chunk.width = chunkVal["width"].GetInt();
              if (chunkVal.HasMember("height") && chunkVal["height"].IsInt()) chunk.height = chunkVal["height"].GetInt();

              const int expectedCount = chunk.width * chunk.height;
              if (!parseChunkData(chunkVal, encoding, compression, expectedCount, chunk.gids, outError)) {
                return false;
              }

              layer.chunks.push_back(std::move(chunk));
            }
          } else if (layerVal.HasMember("data")) {
            Chunk chunk{};
            chunk.x = layer.startx;
            chunk.y = layer.starty;
            chunk.width = layer.width;
            chunk.height = layer.height;

            const int expectedCount = chunk.width * chunk.height;
            if (!parseChunkData(layerVal, encoding, compression, expectedCount, chunk.gids, outError)) {
              return false;
            }

            layer.chunks.push_back(std::move(chunk));
          }

          outMap.layers.push_back(std::move(layer));
        } else if (layerType == "objectgroup") {
          ObjectLayer objectLayer{};
          if (layerVal.HasMember("name") && layerVal["name"].IsString()) objectLayer.name = layerVal["name"].GetString();
          if (layerVal.HasMember("id") && layerVal["id"].IsInt()) objectLayer.id = layerVal["id"].GetInt();
          if (layerVal.HasMember("visible") && layerVal["visible"].IsBool()) {
            objectLayer.visible = layerVal["visible"].GetBool();
          }
          if (layerVal.HasMember("opacity") && layerVal["opacity"].IsNumber()) {
            objectLayer.opacity = layerVal["opacity"].GetFloat();
          }
          if (layerVal.HasMember("objects") && layerVal["objects"].IsArray()) {
            for (auto &objVal : layerVal["objects"].GetArray()) {
              if (!objVal.IsObject()) continue;
              Object obj{};
              if (objVal.HasMember("id") && objVal["id"].IsInt()) obj.id = objVal["id"].GetInt();
              if (objVal.HasMember("name") && objVal["name"].IsString()) obj.name = objVal["name"].GetString();
              if (objVal.HasMember("type") && objVal["type"].IsString()) obj.type = objVal["type"].GetString();
              if (objVal.HasMember("x") && objVal["x"].IsNumber()) obj.x = objVal["x"].GetFloat();
              if (objVal.HasMember("y") && objVal["y"].IsNumber()) obj.y = objVal["y"].GetFloat();
              if (objVal.HasMember("width") && objVal["width"].IsNumber()) obj.width = objVal["width"].GetFloat();
              if (objVal.HasMember("height") && objVal["height"].IsNumber()) obj.height = objVal["height"].GetFloat();
              if (objVal.HasMember("visible") && objVal["visible"].IsBool()) {
                obj.visible = objVal["visible"].GetBool();
              }
              objectLayer.objects.push_back(std::move(obj));
            }
          }
          outMap.objectLayers.push_back(std::move(objectLayer));
        }
      }
    }

    return true;
  }
} // namespace lve::tilemap
