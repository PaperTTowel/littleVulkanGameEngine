#pragma once

#include "utils/game_object.hpp"

#include <filesystem>
#include <string>

namespace lve {
  class SceneSystem;
}

namespace lve::backend {
  class RenderModel;
}

namespace lve::editor::workflow {

  bool IsMeshFile(const std::filesystem::path &path);
  bool IsSpriteMetaFile(const std::filesystem::path &path);
  bool IsMaterialFile(const std::filesystem::path &path);
  bool IsTextureFile(const std::filesystem::path &path);
  std::string PickImportSubdir(const std::filesystem::path &path);
  std::filesystem::path MakeUniquePath(const std::filesystem::path &path);
  bool CopyIntoAssets(
    const std::filesystem::path &source,
    const std::string &root,
    std::filesystem::path &outPath,
    std::string &outError);
  std::string ToAssetPath(const std::string &path, const std::string &root);
  bool CreateMaterialInstance(
    SceneSystem &sceneSystem,
    const std::string &sourcePath,
    const backend::RenderModel *model,
    LveGameObject::id_t objectId,
    const std::string &root,
    std::string &outPath,
    std::string &outError);
  bool CreateLinkStub(
    const std::filesystem::path &source,
    const std::string &root,
    std::filesystem::path &outPath,
    std::string &outError);

} // namespace lve::editor::workflow
