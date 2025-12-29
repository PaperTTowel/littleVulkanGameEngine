#include "Engine/Backend/Vulkan/Render/asset_factory.hpp"

#include "Engine/Backend/Vulkan/Render/material.hpp"
#include "Engine/Backend/Vulkan/Render/model.hpp"
#include "Engine/Backend/Vulkan/Render/texture.hpp"
#include "Engine/material_io.hpp"

#include <exception>
#include <iostream>

namespace lve::backend {

  VulkanRenderAssetFactory::VulkanRenderAssetFactory(LveDevice &device)
    : device{device} {}

  std::shared_ptr<RenderModel> VulkanRenderAssetFactory::loadModel(const std::string &path) {
    try {
      auto uniqueModel = LveModel::createModelFromFile(device, path);
      return std::shared_ptr<LveModel>(std::move(uniqueModel));
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
      auto uniqueTexture = LveTexture::createTextureFromFile(device, path);
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
