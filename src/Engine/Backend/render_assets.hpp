#pragma once

#include "Engine/Backend/model_data.hpp"
#include "Engine/material_data.hpp"

#include <glm/glm.hpp>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace lve::backend {

  class RenderTexture {
  public:
    virtual ~RenderTexture() = default;
  };

  class RenderMaterial {
  public:
    virtual ~RenderMaterial() = default;

    virtual const MaterialData &getData() const = 0;
    virtual const std::string &getPath() const = 0;
    virtual bool hasBaseColorTexture() const = 0;
    virtual const RenderTexture *getBaseColorTexture() const = 0;
    virtual bool applyData(
      const MaterialData &data,
      std::string *outError = nullptr,
      const std::function<std::string(const std::string &)> &pathResolver = {}) = 0;
    virtual void setPath(const std::string &newPath) = 0;
  };

  class RenderModel {
  public:
    virtual ~RenderModel() = default;

    virtual const std::vector<ModelNode> &getNodes() const = 0;
    virtual const std::vector<ModelSubMesh> &getSubMeshes() const = 0;
    virtual const std::vector<MaterialPathInfo> &getMaterialPathInfo() const = 0;
    virtual std::string getDiffusePathForMaterialIndex(int materialIndex) const = 0;
    virtual std::string getDiffusePathForSubMesh(const ModelSubMesh &subMesh) const = 0;
    virtual const RenderTexture *getDiffuseTextureForSubMesh(const ModelSubMesh &subMesh) const = 0;
    virtual bool hasAnyDiffuseTexture() const = 0;
    virtual void computeNodeGlobals(
      const std::vector<glm::mat4> &localOverrides,
      std::vector<glm::mat4> &outGlobals) const = 0;
    virtual const ModelBoundingBox &getBoundingBox() const = 0;
  };

  class RenderAssetFactory {
  public:
    virtual ~RenderAssetFactory() = default;

    virtual std::shared_ptr<RenderModel> loadModel(const std::string &path) = 0;
    virtual std::shared_ptr<RenderMaterial> loadMaterial(
      const std::string &path,
      std::string *outError = nullptr,
      const std::function<std::string(const std::string &)> &pathResolver = {}) = 0;
    virtual std::shared_ptr<RenderMaterial> createMaterial() = 0;
    virtual bool saveMaterial(
      const std::string &path,
      const MaterialData &data,
      std::string *outError = nullptr) = 0;
    virtual std::shared_ptr<RenderTexture> loadTexture(const std::string &path) = 0;
    virtual std::shared_ptr<RenderTexture> getDefaultTexture() = 0;
  };

} // namespace lve::backend
