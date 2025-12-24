#include "resource_browser_panel.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace lve::editor {

  namespace {

    bool hasExtension(const std::filesystem::path &path, const std::vector<std::string> &exts) {
      const std::string ext = path.extension().string();
      for (const auto &candidate : exts) {
        if (_stricmp(ext.c_str(), candidate.c_str()) == 0) {
          return true;
        }
      }
      return false;
    }

    std::string toGenericString(const std::filesystem::path &p) {
      return p.generic_string();
    }

    void scanDirectory(
      const std::string &root,
      const std::vector<std::string> &exts,
      std::vector<std::string> &outFiles) {
      namespace fs = std::filesystem;
      outFiles.clear();
      std::error_code ec;
      for (fs::recursive_directory_iterator it(root, ec); it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) break;
        if (!it->is_regular_file(ec)) continue;
        if (hasExtension(it->path(), exts)) {
          outFiles.push_back(toGenericString(it->path()));
        }
      }
      std::sort(outFiles.begin(), outFiles.end());
    }

    std::string selectedPath(const std::vector<std::string> &files, int index) {
      if (index < 0 || index >= static_cast<int>(files.size())) return {};
      return files[static_cast<std::size_t>(index)];
    }

  } // namespace

  ResourceBrowserActions BuildResourceBrowserPanel(
    ResourceBrowserState &state,
    const LveGameObject *selected) {

    if (state.pendingRefresh) {
      scanDirectory("Assets/models", {".obj", ".fbx", ".gltf", ".glb"}, state.meshFiles);
      scanDirectory("Assets", {".json"}, state.spriteMetaFiles);
      state.pendingRefresh = false;
      if (state.selectedMesh >= static_cast<int>(state.meshFiles.size())) {
        state.selectedMesh = -1;
      }
      if (state.selectedSpriteMeta >= static_cast<int>(state.spriteMetaFiles.size())) {
        state.selectedSpriteMeta = -1;
      }
    }

    ResourceBrowserActions actions{};

    ImGui::Begin("Resource Browser");
    if (ImGui::Button("Refresh")) {
      state.pendingRefresh = true;
      actions.refreshRequested = true;
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Meshes (.obj/.fbx/.gltf/.glb)");
    ImGui::Text("Active: %s", state.activeMeshPath.c_str());

    ImGui::BeginChild("MeshList", ImVec2(0, 160), true);
    for (int i = 0; i < static_cast<int>(state.meshFiles.size()); ++i) {
      const bool isSel = state.selectedMesh == i;
      if (ImGui::Selectable(state.meshFiles[static_cast<std::size_t>(i)].c_str(), isSel)) {
        state.selectedMesh = i;
      }
    }
    ImGui::EndChild();

    ImGui::BeginDisabled(state.selectedMesh < 0);
    if (ImGui::Button("Use for new Mesh")) {
      state.activeMeshPath = selectedPath(state.meshFiles, state.selectedMesh);
      actions.setActiveMesh = true;
    }
    ImGui::EndDisabled();

    const bool canApplyMesh = selected && selected->model != nullptr && !selected->isSprite && !selected->pointLight;
    ImGui::SameLine();
    ImGui::BeginDisabled(state.selectedMesh < 0 || !canApplyMesh);
    if (ImGui::Button("Apply to Selected Mesh")) {
      actions.applyMeshToSelection = true;
    }
    ImGui::EndDisabled();

    ImGui::Separator();
    ImGui::TextUnformatted("Sprite Metadata (.json)");
    ImGui::Text("Active: %s", state.activeSpriteMetaPath.c_str());

    ImGui::BeginChild("SpriteMetaList", ImVec2(0, 160), true);
    for (int i = 0; i < static_cast<int>(state.spriteMetaFiles.size()); ++i) {
      const bool isSel = state.selectedSpriteMeta == i;
      if (ImGui::Selectable(state.spriteMetaFiles[static_cast<std::size_t>(i)].c_str(), isSel)) {
        state.selectedSpriteMeta = i;
      }
    }
    ImGui::EndChild();

    ImGui::BeginDisabled(state.selectedSpriteMeta < 0);
    if (ImGui::Button("Use for sprites")) {
      state.activeSpriteMetaPath = selectedPath(state.spriteMetaFiles, state.selectedSpriteMeta);
      actions.setActiveSpriteMeta = true;
    }
    ImGui::EndDisabled();

    const bool canApplySprite = selected && selected->isSprite;
    ImGui::SameLine();
    ImGui::BeginDisabled(state.selectedSpriteMeta < 0 || !canApplySprite);
    if (ImGui::Button("Apply to Selected Sprite")) {
      actions.applySpriteMetaToSelection = true;
    }
    ImGui::EndDisabled();

    ImGui::End();
    return actions;
  }

} // namespace lve::editor
