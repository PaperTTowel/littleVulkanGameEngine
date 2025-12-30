#include "Editor/Workflow/editor_import.hpp"

#include "Engine/IO/material_io.hpp"
#include "Engine/scene_system.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>

namespace lve::editor::workflow {

  namespace {
    namespace fs = std::filesystem;

    std::string toLowerCopy(const std::string &value) {
      std::string out;
      out.reserve(value.size());
      for (char ch : value) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
      }
      return out;
    }

    bool hasExtension(const fs::path &path, std::initializer_list<const char *> exts) {
      const std::string ext = toLowerCopy(path.extension().string());
      for (const char *candidate : exts) {
        if (ext == toLowerCopy(candidate)) {
          return true;
        }
      }
      return false;
    }

    fs::path normalizePath(const fs::path &path) {
      std::error_code ec;
      fs::path normalized = fs::weakly_canonical(path, ec);
      if (ec) {
        normalized = path.lexically_normal();
      }
      return normalized;
    }

    bool isSubPath(const fs::path &path, const fs::path &base) {
      auto pathIt = path.begin();
      auto baseIt = base.begin();
      for (; baseIt != base.end(); ++baseIt, ++pathIt) {
        if (pathIt == path.end()) return false;
        if (*pathIt != *baseIt) return false;
      }
      return true;
    }
  } // namespace

  bool IsMeshFile(const std::filesystem::path &path) {
    return hasExtension(path, {".obj", ".fbx", ".gltf", ".glb"});
  }

  bool IsSpriteMetaFile(const std::filesystem::path &path) {
    return hasExtension(path, {".json"});
  }

  bool IsMaterialFile(const std::filesystem::path &path) {
    return hasExtension(path, {".mat"});
  }

  bool IsTextureFile(const std::filesystem::path &path) {
    return hasExtension(path, {".png", ".jpg", ".jpeg", ".tga", ".bmp", ".dds", ".hdr", ".tiff", ".ktx", ".ktx2"});
  }

  std::string PickImportSubdir(const std::filesystem::path &path) {
    if (IsMeshFile(path)) return "models";
    if (IsMaterialFile(path)) return "materials";
    if (IsTextureFile(path) || IsSpriteMetaFile(path)) return "textures";
    return "imported";
  }

  std::filesystem::path MakeUniquePath(const std::filesystem::path &path) {
    std::error_code ec;
    if (!fs::exists(path, ec)) {
      return path;
    }
    const fs::path parent = path.parent_path();
    const std::string stem = path.stem().string();
    const std::string ext = path.extension().string();
    for (int i = 1; i < 1000; ++i) {
      fs::path candidate = parent / (stem + "_" + std::to_string(i) + ext);
      if (!fs::exists(candidate, ec)) {
        return candidate;
      }
    }
    return path;
  }

  bool CopyIntoAssets(
    const std::filesystem::path &source,
    const std::string &root,
    std::filesystem::path &outPath,
    std::string &outError) {
    std::error_code ec;
    if (!fs::exists(source, ec) || fs::is_directory(source, ec)) {
      outError = "Source file not found";
      return false;
    }
    fs::path rootPath = root.empty() ? fs::path("Assets") : fs::path(root);
    fs::path targetDir = rootPath / PickImportSubdir(source);
    fs::create_directories(targetDir, ec);
    if (ec) {
      outError = "Failed to create target directory";
      return false;
    }
    fs::path destPath = MakeUniquePath(targetDir / source.filename());
    fs::copy_file(source, destPath, fs::copy_options::none, ec);
    if (ec) {
      outError = "Copy failed";
      return false;
    }
    outPath = destPath;
    return true;
  }

  std::string ToAssetPath(const std::string &path, const std::string &root) {
    if (path.empty()) return {};
    fs::path inputPath(path);
    if (!inputPath.is_absolute()) {
      return inputPath.generic_string();
    }
    fs::path rootPath = root.empty() ? fs::path("Assets") : fs::path(root);
    const fs::path normalizedRoot = normalizePath(rootPath);
    const fs::path normalizedInput = normalizePath(inputPath);
    if (isSubPath(normalizedInput, normalizedRoot)) {
      std::error_code ec;
      fs::path rel = fs::relative(normalizedInput, normalizedRoot, ec);
      if (!ec) {
        return (rootPath / rel).generic_string();
      }
    }
    return inputPath.generic_string();
  }

  bool CreateMaterialInstance(
    SceneSystem &sceneSystem,
    const std::string &sourcePath,
    const backend::RenderModel *model,
    LveGameObject::id_t objectId,
    const std::string &root,
    std::string &outPath,
    std::string &outError) {
    MaterialData data{};
    if (!sourcePath.empty()) {
      auto material = sceneSystem.loadMaterialCached(sourcePath);
      if (material) {
        data = material->getData();
      }
    }
    if (data.name.empty()) {
      data.name = "Material_" + std::to_string(objectId);
    }
    if (data.textures.baseColor.empty() && model) {
      std::string diffusePath;
      const auto &subMeshes = model->getSubMeshes();
      for (const auto &subMesh : subMeshes) {
        diffusePath = model->getDiffusePathForSubMesh(subMesh);
        if (!diffusePath.empty()) {
          break;
        }
      }
      if (diffusePath.empty()) {
        const auto &infos = model->getMaterialPathInfo();
        for (std::size_t i = 0; i < infos.size(); ++i) {
          diffusePath = model->getDiffusePathForMaterialIndex(static_cast<int>(i));
          if (!diffusePath.empty()) {
            break;
          }
        }
      }
      if (!diffusePath.empty()) {
        data.textures.baseColor = ToAssetPath(diffusePath, root);
      }
    }

    fs::path rootPath = root.empty() ? fs::path("Assets") : fs::path(root);
    fs::path targetDir = rootPath / "materials";
    fs::path targetPath = MakeUniquePath(targetDir / (data.name + ".mat"));
    std::string error;
    if (!saveMaterialToFile(targetPath.generic_string(), data, &error)) {
      outError = error.empty() ? "Failed to save material instance" : error;
      return false;
    }
    outPath = targetPath.generic_string();
    sceneSystem.getAssetDatabase().registerAsset(outPath);
    return true;
  }

  bool CreateLinkStub(
    const std::filesystem::path &source,
    const std::string &root,
    std::filesystem::path &outPath,
    std::string &outError) {
    std::error_code ec;
    if (!fs::exists(source, ec) || fs::is_directory(source, ec)) {
      outError = "Source file not found";
      return false;
    }
    fs::path rootPath = root.empty() ? fs::path("Assets") : fs::path(root);
    fs::path targetDir = rootPath / "links";
    fs::create_directories(targetDir, ec);
    if (ec) {
      outError = "Failed to create link directory";
      return false;
    }
    fs::path destPath = MakeUniquePath(targetDir / source.filename());
    std::ofstream file(destPath, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!file) {
      outError = "Failed to create link stub";
      return false;
    }
    outPath = destPath;
    return true;
  }

} // namespace lve::editor::workflow


