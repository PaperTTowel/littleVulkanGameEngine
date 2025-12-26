#include "resource_browser_panel.hpp"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
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

    std::string toLowerCopy(const std::string &value) {
      std::string out;
      out.reserve(value.size());
      for (char ch : value) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
      }
      return out;
    }

    bool matchesFilter(const std::filesystem::path &path, const std::string &filter) {
      if (filter.empty()) return true;
      const std::string needle = toLowerCopy(filter);
      const std::string haystack = toLowerCopy(path.filename().string());
      return haystack.find(needle) != std::string::npos;
    }

    bool isMeshFile(const std::filesystem::path &path) {
      return hasExtension(path, {".obj", ".fbx", ".gltf", ".glb"});
    }

    bool isSpriteMetaFile(const std::filesystem::path &path) {
      return hasExtension(path, {".json"});
    }

    void scanDirectory(
      const std::string &root,
      const std::string &filter,
      std::vector<std::string> &outDirs,
      std::vector<std::string> &outFiles) {
      namespace fs = std::filesystem;
      outDirs.clear();
      outFiles.clear();
      std::error_code ec;
      for (fs::directory_iterator it(root, ec); it != fs::directory_iterator(); it.increment(ec)) {
        if (ec) break;
        if (it->is_directory(ec)) {
          outDirs.push_back(toGenericString(it->path()));
          continue;
        }
        if (!it->is_regular_file(ec)) continue;
        if (matchesFilter(it->path(), filter)) {
          outFiles.push_back(toGenericString(it->path()));
        }
      }
      std::sort(outDirs.begin(), outDirs.end());
      std::sort(outFiles.begin(), outFiles.end());
    }

    std::string selectedPath(const std::vector<std::string> &files, int index) {
      if (index < 0 || index >= static_cast<int>(files.size())) return {};
      return files[static_cast<std::size_t>(index)];
    }

    std::string filenameLabel(const std::string &path) {
      return std::filesystem::path(path).filename().string();
    }

  } // namespace

  ResourceBrowserActions BuildResourceBrowserPanel(
    ResourceBrowserState &state,
    const LveGameObject *selected,
    bool *open) {

    if (state.pendingRefresh) {
      if (state.currentPath.empty()) {
        state.currentPath = state.rootPath;
      }
      scanDirectory(state.currentPath, state.filter, state.directories, state.files);
      state.pendingRefresh = false;
      if (state.selectedDir >= static_cast<int>(state.directories.size())) {
        state.selectedDir = -1;
      }
      if (state.selectedFile >= static_cast<int>(state.files.size())) {
        state.selectedFile = -1;
      }
    }

    ResourceBrowserActions actions{};

    if (open) {
      if (!ImGui::Begin("Resource Browser", open)) {
        ImGui::End();
        return actions;
      }
    } else {
      ImGui::Begin("Resource Browser");
    }
    if (ImGui::Button("Refresh")) {
      state.pendingRefresh = true;
      actions.refreshRequested = true;
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(state.currentPath == state.rootPath);
    if (ImGui::Button("Up")) {
      std::filesystem::path current = state.currentPath;
      std::filesystem::path parent = current.parent_path();
      if (!parent.empty()) {
        state.currentPath = parent.generic_string();
        state.pendingRefresh = true;
        state.selectedDir = -1;
        state.selectedFile = -1;
      }
    }
    ImGui::EndDisabled();

    char filterBuf[128];
    std::snprintf(filterBuf, sizeof(filterBuf), "%s", state.filter.c_str());
    if (ImGui::InputText("Filter", filterBuf, sizeof(filterBuf))) {
      state.filter = filterBuf;
      state.pendingRefresh = true;
    }

    ImGui::Text("Path: %s", state.currentPath.c_str());

    ImGui::Separator();
    ImGui::BeginChild("DirList", ImVec2(220, 240), true);
    for (int i = 0; i < static_cast<int>(state.directories.size()); ++i) {
      const bool isSel = state.selectedDir == i;
      const std::string label = filenameLabel(state.directories[static_cast<std::size_t>(i)]);
      if (ImGui::Selectable(label.c_str(), isSel)) {
        state.selectedDir = i;
      }
      if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        state.currentPath = state.directories[static_cast<std::size_t>(i)];
        state.pendingRefresh = true;
        state.selectedDir = -1;
        state.selectedFile = -1;
      }
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("FileList", ImVec2(0, 240), true);
    for (int i = 0; i < static_cast<int>(state.files.size()); ++i) {
      const bool isSel = state.selectedFile == i;
      const std::string label = filenameLabel(state.files[static_cast<std::size_t>(i)]);
      if (ImGui::Selectable(label.c_str(), isSel)) {
        state.selectedFile = i;
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", state.files[static_cast<std::size_t>(i)].c_str());
      }
    }
    ImGui::EndChild();

    const std::string selectedFile = selectedPath(state.files, state.selectedFile);
    const bool selectedIsMesh = !selectedFile.empty() && isMeshFile(selectedFile);
    const bool selectedIsSpriteMeta = !selectedFile.empty() && isSpriteMetaFile(selectedFile);

    ImGui::Separator();
    ImGui::Text("Active Mesh: %s", state.activeMeshPath.c_str());
    ImGui::BeginDisabled(!selectedIsMesh);
    if (ImGui::Button("Use for new Mesh")) {
      state.activeMeshPath = selectedFile;
      actions.setActiveMesh = true;
    }
    ImGui::EndDisabled();

    const bool canApplyMesh = selected && selected->model != nullptr && !selected->isSprite && !selected->pointLight;
    ImGui::SameLine();
    ImGui::BeginDisabled(!selectedIsMesh || !canApplyMesh);
    if (ImGui::Button("Apply to Selected Mesh")) {
      actions.applyMeshToSelection = true;
    }
    ImGui::EndDisabled();

    ImGui::Separator();
    ImGui::Text("Active Sprite Meta: %s", state.activeSpriteMetaPath.c_str());
    ImGui::BeginDisabled(!selectedIsSpriteMeta);
    if (ImGui::Button("Use for sprites")) {
      state.activeSpriteMetaPath = selectedFile;
      actions.setActiveSpriteMeta = true;
    }
    ImGui::EndDisabled();

    const bool canApplySprite = selected && selected->isSprite;
    ImGui::SameLine();
    ImGui::BeginDisabled(!selectedIsSpriteMeta || !canApplySprite);
    if (ImGui::Button("Apply to Selected Sprite")) {
      actions.applySpriteMetaToSelection = true;
    }
    ImGui::EndDisabled();

    ImGui::End();
    return actions;
  }

} // namespace lve::editor
