#include "Engine/Backend/Vulkan/Render/asset_factory.hpp"

#include "Engine/Backend/Vulkan/Render/material.hpp"
#include "Engine/Backend/Vulkan/Render/model.hpp"
#include "Engine/Backend/Vulkan/Render/texture.hpp"
#include "Engine/IO/image_io.hpp"
#include "Engine/IO/material_io.hpp"

#include <cctype>
#include <exception>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lve::backend {
  namespace {
    std::string toLowerCopy(std::string_view value) {
      std::string out;
      out.reserve(value.size());
      for (char ch : value) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
      }
      return out;
    }

    bool isQuadPath(const std::string &path) {
      const std::string lower = toLowerCopy(path);
      const std::string_view needle = "quad";
      const std::size_t pos = lower.find(needle);
      if (pos == std::string::npos) {
        return false;
      }
      return true;
    }

    backend::ModelData makeQuadModelData() {
      backend::ModelData data{};
      data.vertices = {
        {{-0.5f, -0.5f, 0.0f}, {1.f, 1.f, 1.f}, {0.f, 0.f, 1.f}, {0.f, 1.f}},
        {{ 0.5f, -0.5f, 0.0f}, {1.f, 1.f, 1.f}, {0.f, 0.f, 1.f}, {1.f, 1.f}},
        {{ 0.5f,  0.5f, 0.0f}, {1.f, 1.f, 1.f}, {0.f, 0.f, 1.f}, {1.f, 0.f}},
        {{-0.5f,  0.5f, 0.0f}, {1.f, 1.f, 1.f}, {0.f, 0.f, 1.f}, {0.f, 0.f}},
      };
      data.indices = {0, 1, 2, 2, 3, 0};

      backend::ModelSubMesh subMesh{};
      subMesh.firstIndex = 0;
      subMesh.indexCount = static_cast<std::uint32_t>(data.indices.size());
      subMesh.materialIndex = -1;
      subMesh.hasBounds = true;
      subMesh.boundsMin = {-0.5f, -0.5f, 0.0f};
      subMesh.boundsMax = {0.5f, 0.5f, 0.0f};
      data.subMeshes.push_back(subMesh);

      backend::ModelNode node{};
      node.name = "Quad";
      node.parent = -1;
      node.meshes.push_back(0);
      data.nodes.push_back(std::move(node));
      return data;
    }
  } // namespace

  VulkanRenderAssetFactory::VulkanRenderAssetFactory(LveDevice &device)
    : device{device} {}

  std::shared_ptr<RenderModel> VulkanRenderAssetFactory::loadModel(const std::string &path) {
    if (!isQuadPath(path)) {
      std::cerr << "Model loading disabled in 2D mode: " << path << "\n";
      return {};
    }

    backend::ModelData data = makeQuadModelData();
    std::vector<std::shared_ptr<LveTexture>> materialTextures;
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
