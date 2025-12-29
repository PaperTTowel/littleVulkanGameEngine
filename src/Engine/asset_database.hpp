#pragma once

#include <string>
#include <unordered_map>

namespace lve {

  enum class AssetType {
    Unknown,
    Model,
    Texture,
    Material,
    SpriteMeta,
    Scene
  };

  struct ModelImportSettings {
    float scale{1.f};
    bool generateNormals{false};
    bool generateTangents{true};
    bool flipUV{false};
  };

  struct TextureImportSettings {
    bool sRGB{true};
    bool generateMipmaps{true};
  };

  struct AssetMeta {
    int version{1};
    std::string guid{};
    AssetType type{AssetType::Unknown};
    std::string sourcePath{};
    ModelImportSettings modelSettings{};
    TextureImportSettings textureSettings{};
  };

  class AssetDatabase {
  public:
    explicit AssetDatabase(std::string rootPath = "Assets");

    void setRootPath(const std::string &rootPath);
    const std::string &getRootPath() const { return rootPath; }

    void initialize();

    std::string registerAsset(const std::string &assetPath, const std::string &sourcePath = {});
    std::string ensureMetaForAsset(const std::string &assetPath, const std::string &sourcePath = {});
    std::string getGuidForPath(const std::string &assetPath) const;
    std::string getPathForGuid(const std::string &guid) const;
    std::string resolveAssetPath(const std::string &assetPath) const;
    std::string resolveGuid(const std::string &guid) const;
    const AssetMeta *getMetaForPath(const std::string &assetPath) const;
    const AssetMeta *getMetaForGuid(const std::string &guid) const;

  private:
    std::string rootPath;
    std::unordered_map<std::string, std::string> pathToGuid;
    std::unordered_map<std::string, std::string> guidToPath;
    std::unordered_map<std::string, AssetMeta> pathToMeta;
  };

} // namespace lve
