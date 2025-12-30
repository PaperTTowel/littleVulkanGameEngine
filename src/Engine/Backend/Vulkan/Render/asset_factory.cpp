#include "Engine/Backend/Vulkan/Render/asset_factory.hpp"

#include "Engine/Backend/Vulkan/Render/material.hpp"
#include "Engine/Backend/Vulkan/Render/model.hpp"
#include "Engine/Backend/Vulkan/Render/texture.hpp"
#include "Engine/IO/image_io.hpp"
#include "Engine/IO/model_io.hpp"
#include "Engine/IO/material_io.hpp"

#include <exception>
#include <iostream>
#include <unordered_map>
#include <utility>
#include <vector>

namespace lve::backend {

  VulkanRenderAssetFactory::VulkanRenderAssetFactory(LveDevice &device)
    : device{device} {}

  std::shared_ptr<RenderModel> VulkanRenderAssetFactory::loadModel(const std::string &path) {
    backend::ModelData data{};
    std::string error;
    if (!loadModelDataFromFile(path, data, &error)) {
      std::cerr << "Failed to load model " << path;
      if (!error.empty()) {
        std::cerr << ": " << error;
      }
      std::cerr << "\n";
      return {};
    }

    std::unordered_map<std::string, std::shared_ptr<LveTexture>> fileCache;
    std::vector<std::shared_ptr<LveTexture>> materialTextures;
    materialTextures.resize(data.materials.size());

    for (std::size_t i = 0; i < data.materials.size(); ++i) {
      const auto &source = data.materials[i].diffuse;
      std::shared_ptr<LveTexture> texture{};

      if (source.kind == backend::ModelTextureSource::Kind::File && !source.path.empty()) {
        auto it = fileCache.find(source.path);
        if (it != fileCache.end()) {
          texture = it->second;
        } else {
          ImageData image{};
          std::string texError;
          if (loadImageDataFromFile(source.path, image, &texError)) {
            auto uniqueTex = LveTexture::createTextureFromRgba(
              device,
              image.pixels.data(),
              image.width,
              image.height);
            texture = std::shared_ptr<LveTexture>(std::move(uniqueTex));
            fileCache[source.path] = texture;
          } else {
            std::cerr << "Failed to load model texture " << source.path;
            if (!texError.empty()) {
              std::cerr << ": " << texError;
            }
            std::cerr << "\n";
          }
        }
      } else if (
        source.kind == backend::ModelTextureSource::Kind::EmbeddedCompressed ||
        source.kind == backend::ModelTextureSource::Kind::EmbeddedRaw) {
        ImageData image{};
        std::string texError;
        if (loadImageDataFromTextureSource(source, image, &texError)) {
          auto uniqueTex = LveTexture::createTextureFromRgba(
            device,
            image.pixels.data(),
            image.width,
            image.height);
          texture = std::shared_ptr<LveTexture>(std::move(uniqueTex));
        } else {
          std::cerr << "Failed to decode embedded model texture";
          if (!texError.empty()) {
            std::cerr << ": " << texError;
          }
          std::cerr << "\n";
        }
      }

      materialTextures[i] = texture;
    }

    try {
      return std::make_shared<LveModel>(device, data, std::move(materialTextures));
    } catch (const std::exception &e) {
      std::cerr << "Failed to load model " << path << ": " << e.what() << "\n";
      return {};
    }
  }

  std::shared_ptr<RenderMaterial> VulkanRenderAssetFactory::loadMaterial(
    const std::string &path,
    std::string *outError,
    const std::function<std::string(const std::string &)> &pathResolver) {
    return LveMaterial::loadFromFile(device, path, outError, pathResolver);
  }

  std::shared_ptr<RenderMaterial> VulkanRenderAssetFactory::createMaterial() {
    return std::make_shared<LveMaterial>(device);
  }

  bool VulkanRenderAssetFactory::saveMaterial(
    const std::string &path,
    const MaterialData &data,
    std::string *outError) {
    return saveMaterialToFile(path, data, outError);
  }

  std::shared_ptr<RenderTexture> VulkanRenderAssetFactory::loadTexture(const std::string &path) {
    try {
      ImageData image{};
      std::string error;
      if (!loadImageDataFromFile(path, image, &error)) {
        std::cerr << "Failed to load texture " << path;
        if (!error.empty()) {
          std::cerr << ": " << error;
        }
        std::cerr << "\n";
        return {};
      }
      auto uniqueTexture = LveTexture::createTextureFromRgba(
        device,
        image.pixels.data(),
        image.width,
        image.height);
      return std::shared_ptr<LveTexture>(std::move(uniqueTexture));
    } catch (const std::exception &e) {
      std::cerr << "Failed to load texture " << path << ": " << e.what() << "\n";
      return {};
    }
  }

  std::shared_ptr<RenderTexture> VulkanRenderAssetFactory::getDefaultTexture() {
    if (defaultTexture) {
      return defaultTexture;
    }
    defaultTexture = loadTexture("Assets/textures/missing.png");
    return defaultTexture;
  }

} // namespace lve::backend
