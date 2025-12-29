#include "resource_browser_panel.hpp"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace lve::editor {

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

    bool hasExtension(const std::filesystem::path &path, const std::vector<std::string> &exts) {
      const std::string ext = toLowerCopy(path.extension().string());
      for (const auto &candidate : exts) {
        if (ext == toLowerCopy(candidate)) {
          return true;
        }
      }
      return false;
    }

    std::string toGenericString(const fs::path &p) {
      return p.generic_string();
    }

    bool matchesFilter(const fs::path &path, const std::string &filter) {
      if (filter.empty()) return true;
      const std::string needle = toLowerCopy(filter);
      const std::string haystack = toLowerCopy(path.filename().string());
      return haystack.find(needle) != std::string::npos;
    }

    bool isMeshFile(const fs::path &path) {
      return hasExtension(path, {".obj", ".fbx", ".gltf", ".glb"});
    }

    bool isSpriteMetaFile(const fs::path &path) {
      return hasExtension(path, {".json"});
    }

    bool isMaterialFile(const fs::path &path) {
      return hasExtension(path, {".mat"});
    }

    void scanDirectory(
      const std::string &root,
      const std::string &filter,
      std::vector<std::string> &outDirs,
      std::vector<std::string> &outFiles) {
      outDirs.clear();
      outFiles.clear();
      if (root.empty()) return;
      std::error_code ec;
      if (!fs::exists(root, ec)) return;
      for (fs::directory_iterator it(root, ec); it != fs::directory_iterator(); it.increment(ec)) {
        if (ec) break;
        if (it->is_directory(ec)) {
          outDirs.push_back(toGenericString(it->path()));
          continue;
        }
        if (!it->is_regular_file(ec)) continue;
        if (toLowerCopy(it->path().extension().string()) == ".meta") continue;
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
      return fs::path(path).filename().string();
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

    bool isInsideRoot(const fs::path &path, const fs::path &root) {
      if (root.empty()) return true;
      return isSubPath(normalizePath(path), normalizePath(root));
    }

    void resetSelection(BrowserState &state) {
      state.selectedDir = -1;
      state.selectedFile = -1;
    }

    void clampSelection(BrowserState &state) {
      if (state.selectedDir >= static_cast<int>(state.directories.size())) {
        state.selectedDir = -1;
      }
      if (state.selectedFile >= static_cast<int>(state.files.size())) {
        state.selectedFile = -1;
      }
    }

    void jumpToPath(BrowserState &state, const fs::path &target) {
      state.currentPath = toGenericString(target);
      state.pendingRefresh = true;
      resetSelection(state);
    }

    void clampToRoot(BrowserState &state) {
      if (!state.restrictToRoot || state.rootPath.empty()) return;
      if (!isInsideRoot(state.currentPath, state.rootPath)) {
        state.currentPath = state.rootPath;
        resetSelection(state);
        state.pendingRefresh = true;
      }
    }

    void ensureBrowserReady(BrowserState &state) {
      if (state.currentPath.empty()) {
        if (!state.rootPath.empty()) {
          state.currentPath = state.rootPath;
        } else {
          std::error_code ec;
          const fs::path current = fs::current_path(ec);
          if (!ec) {
            state.currentPath = current.generic_string();
          }
        }
      }
      clampToRoot(state);
      if (state.pendingRefresh) {
        scanDirectory(state.currentPath, state.filter, state.directories, state.files);
        state.pendingRefresh = false;
        clampSelection(state);
      }
    }

    void drawDriveButtons(BrowserState &state) {
      ImGui::AlignTextToFramePadding();
      ImGui::TextUnformatted("Drives");
      ImGui::SameLine();
      bool first = true;
      for (char drive = 'A'; drive <= 'Z'; ++drive) {
        std::string root;
        root.push_back(drive);
        root.append(":/");
        std::error_code ec;
        if (!fs::exists(root, ec)) continue;
        if (!first) {
          ImGui::SameLine();
        }
        first = false;
        if (ImGui::SmallButton(root.c_str())) {
          jumpToPath(state, root);
        }
      }
    }

    struct BrowserSelection {
      std::string selectedFile{};
      std::string selectedDir{};
      bool fileActivated{false};
      bool refreshRequested{false};
    };

    struct BrowserContextMenuConfig {
      bool enabled{false};
      const LveGameObject *selected{nullptr};
      ResourceBrowserState *resourceState{nullptr};
      ResourceBrowserActions *resourceActions{nullptr};
    };

    BrowserSelection drawBrowserView(
      BrowserState &state,
      const char *id,
      bool allowPathEdit,
      bool showDrives,
      bool showTypeTags,
      BrowserContextMenuConfig *contextMenu) {
      BrowserSelection selection{};
      ImGui::PushID(id);

      ensureBrowserReady(state);

      if (ImGui::Button("Refresh")) {
        state.pendingRefresh = true;
        selection.refreshRequested = true;
      }
      ImGui::SameLine();
      fs::path current = state.currentPath;
      fs::path parent = current.parent_path();
      bool canUp = !parent.empty() && parent != current;
      if (state.restrictToRoot && canUp) {
        const fs::path normCurrent = normalizePath(current);
        const fs::path normRoot = normalizePath(state.rootPath);
        if (normCurrent == normRoot) {
          canUp = false;
        } else {
          canUp = isInsideRoot(parent, state.rootPath);
        }
      }
      ImGui::BeginDisabled(!canUp);
      if (ImGui::Button("Up")) {
        if (canUp) {
          jumpToPath(state, parent);
        }
      }
      ImGui::EndDisabled();
      ImGui::SameLine();
      ImGui::BeginDisabled(state.rootPath.empty());
      if (ImGui::Button("Root") && !state.rootPath.empty()) {
        jumpToPath(state, state.rootPath);
      }
      ImGui::EndDisabled();

      ImGui::SameLine();
      ImGui::AlignTextToFramePadding();
      ImGui::TextUnformatted("Path");
      ImGui::SameLine();
      char pathBuf[512];
      std::snprintf(pathBuf, sizeof(pathBuf), "%s", state.currentPath.c_str());
      ImGuiInputTextFlags pathFlags = allowPathEdit ? ImGuiInputTextFlags_EnterReturnsTrue : ImGuiInputTextFlags_ReadOnly;
      ImGui::SetNextItemWidth(-1.0f);
      if (ImGui::InputText("##Path", pathBuf, sizeof(pathBuf), pathFlags)) {
        state.currentPath = pathBuf;
        state.pendingRefresh = true;
        resetSelection(state);
      }

      if (showDrives) {
        ImGui::Spacing();
        drawDriveButtons(state);
      }

      char filterBuf[128];
      std::snprintf(filterBuf, sizeof(filterBuf), "%s", state.filter.c_str());
      ImGui::AlignTextToFramePadding();
      ImGui::TextUnformatted("Search");
      ImGui::SameLine();
      ImGui::SetNextItemWidth(-1.0f);
      if (ImGui::InputTextWithHint("##Filter", "Type to filter files", filterBuf, sizeof(filterBuf))) {
        state.filter = filterBuf;
        state.pendingRefresh = true;
      }

      ImGuiTableFlags tableFlags = ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp;
      if (ImGui::BeginTable("BrowserSplit", 2, tableFlags)) {
        ImGui::TableSetupColumn("Folders", ImGuiTableColumnFlags_WidthFixed, 220.0f);
        ImGui::TableSetupColumn("Files", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::BeginChild("FoldersList", ImVec2(0, 0), true);
        for (int i = 0; i < static_cast<int>(state.directories.size()); ++i) {
          const bool isSel = state.selectedDir == i;
          const std::string label = filenameLabel(state.directories[static_cast<std::size_t>(i)]);
          if (ImGui::Selectable(label.c_str(), isSel, ImGuiSelectableFlags_AllowDoubleClick)) {
            state.selectedDir = i;
            state.selectedFile = -1;
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
              jumpToPath(state, state.directories[static_cast<std::size_t>(i)]);
            }
          }
        }
        ImGui::EndChild();

        ImGui::TableSetColumnIndex(1);
        ImGui::BeginChild("FilesList", ImVec2(0, 0), true);
        for (int i = 0; i < static_cast<int>(state.files.size()); ++i) {
          ImGui::PushID(i);
          const bool isSel = state.selectedFile == i;
          const std::string path = state.files[static_cast<std::size_t>(i)];
          std::string label = filenameLabel(path);
          if (showTypeTags) {
            if (isMeshFile(path)) {
              label += " [Mesh]";
            } else if (isSpriteMetaFile(path)) {
              label += " [SpriteMeta]";
            } else if (isMaterialFile(path)) {
              label += " [Material]";
            }
          }
          if (ImGui::Selectable(label.c_str(), isSel, ImGuiSelectableFlags_AllowDoubleClick)) {
            state.selectedFile = i;
            state.selectedDir = -1;
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
              selection.fileActivated = true;
            }
          }
          if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            state.selectedFile = i;
            state.selectedDir = -1;
          }
          if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", path.c_str());
          }
          if (ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload("ASSET_PATH", path.c_str(), path.size() + 1);
            ImGui::TextUnformatted(label.c_str());
            ImGui::EndDragDropSource();
          }
          if (contextMenu && contextMenu->enabled) {
            if (ImGui::BeginPopupContextItem("FileContext")) {
              const bool isMesh = isMeshFile(path);
              const bool isSpriteMeta = isSpriteMetaFile(path);
              const bool canApplyMesh = contextMenu->selected &&
                contextMenu->selected->model != nullptr &&
                !contextMenu->selected->isSprite &&
                !contextMenu->selected->pointLight;
              const bool canApplySprite = contextMenu->selected && contextMenu->selected->isSprite;

              if (isMesh) {
                if (ImGui::MenuItem("Set Active Mesh")) {
                  if (contextMenu->resourceState) {
                    contextMenu->resourceState->activeMeshPath = path;
                  }
                  if (contextMenu->resourceActions) {
                    contextMenu->resourceActions->setActiveMesh = true;
                  }
                }
                ImGui::BeginDisabled(!canApplyMesh);
                if (ImGui::MenuItem("Apply to Selected Mesh")) {
                  if (contextMenu->resourceState) {
                    contextMenu->resourceState->activeMeshPath = path;
                  }
                  if (contextMenu->resourceActions) {
                    contextMenu->resourceActions->setActiveMesh = true;
                    contextMenu->resourceActions->applyMeshToSelection = true;
                  }
                }
                ImGui::EndDisabled();
              } else if (isSpriteMeta) {
                if (ImGui::MenuItem("Set Active Sprite Meta")) {
                  if (contextMenu->resourceState) {
                    contextMenu->resourceState->activeSpriteMetaPath = path;
                  }
                  if (contextMenu->resourceActions) {
                    contextMenu->resourceActions->setActiveSpriteMeta = true;
                  }
                }
                ImGui::BeginDisabled(!canApplySprite);
                if (ImGui::MenuItem("Apply to Selected Sprite")) {
                  if (contextMenu->resourceState) {
                    contextMenu->resourceState->activeSpriteMetaPath = path;
                  }
                  if (contextMenu->resourceActions) {
                    contextMenu->resourceActions->setActiveSpriteMeta = true;
                    contextMenu->resourceActions->applySpriteMetaToSelection = true;
                  }
                }
                ImGui::EndDisabled();
              } else if (isMaterialFile(path)) {
                const bool canApplyMaterial = contextMenu->selected &&
                  contextMenu->selected->model != nullptr &&
                  !contextMenu->selected->isSprite &&
                  !contextMenu->selected->pointLight;
                if (ImGui::MenuItem("Set Active Material")) {
                  if (contextMenu->resourceState) {
                    contextMenu->resourceState->activeMaterialPath = path;
                  }
                  if (contextMenu->resourceActions) {
                    contextMenu->resourceActions->setActiveMaterial = true;
                  }
                }
                ImGui::BeginDisabled(!canApplyMaterial);
                if (ImGui::MenuItem("Apply to Selected Mesh")) {
                  if (contextMenu->resourceState) {
                    contextMenu->resourceState->activeMaterialPath = path;
                  }
                  if (contextMenu->resourceActions) {
                    contextMenu->resourceActions->setActiveMaterial = true;
                    contextMenu->resourceActions->applyMaterialToSelection = true;
                  }
                }
                ImGui::EndDisabled();
              } else {
                ImGui::TextDisabled("No actions");
              }
              ImGui::EndPopup();
            }
          }
          ImGui::PopID();
        }
        ImGui::EndChild();
        ImGui::EndTable();
      }

      selection.selectedFile = selectedPath(state.files, state.selectedFile);
      selection.selectedDir = selectedPath(state.directories, state.selectedDir);

      ImGui::PopID();
      return selection;
    }

  } // namespace

  ResourceBrowserActions BuildResourceBrowserPanel(
    ResourceBrowserState &state,
    const LveGameObject *selected,
    bool *open) {
    ResourceBrowserActions actions{};

    if (open) {
      if (!ImGui::Begin("Resource Browser", open)) {
        ImGui::End();
        return actions;
      }
    } else {
      ImGui::Begin("Resource Browser");
    }
    const int previousSelectedFile = state.browser.selectedFile;
    BrowserContextMenuConfig contextMenu{};
    contextMenu.enabled = true;
    contextMenu.selected = selected;
    contextMenu.resourceState = &state;
    contextMenu.resourceActions = &actions;
    const BrowserSelection selection = drawBrowserView(
      state.browser,
      "ResourceBrowser",
      true,
      false,
      true,
      &contextMenu);
    actions.refreshRequested = selection.refreshRequested;

    const std::string selectedFile = selection.selectedFile;
    const bool selectedIsMesh = !selectedFile.empty() && isMeshFile(selectedFile);
    const bool selectedIsSpriteMeta = !selectedFile.empty() && isSpriteMetaFile(selectedFile);
    const bool selectedIsMaterial = !selectedFile.empty() && isMaterialFile(selectedFile);
    const bool fileSelectionChanged = state.browser.selectedFile != previousSelectedFile;

    if ((fileSelectionChanged || selection.fileActivated) && selectedIsMesh) {
      if (state.activeMeshPath != selectedFile) {
        state.activeMeshPath = selectedFile;
        actions.setActiveMesh = true;
      }
    }

    ImGui::Separator();
    ImGui::Text("Selected: %s", selectedFile.empty() ? "-" : selectedFile.c_str());

    if (ImGui::BeginTable("ActiveAssets", 3, ImGuiTableFlags_SizingStretchSame)) {
      ImGui::TableNextColumn();
      ImGui::TextUnformatted("Mesh");
      ImGui::TextWrapped("%s", state.activeMeshPath.c_str());
      ImGui::BeginDisabled(!selectedIsMesh);
      if (ImGui::Button("Use for new Mesh")) {
        state.activeMeshPath = selectedFile;
        actions.setActiveMesh = true;
      }
      ImGui::EndDisabled();
      const bool canApplyMesh = selected && selected->model != nullptr && !selected->isSprite && !selected->pointLight;
      ImGui::BeginDisabled(!selectedIsMesh || !canApplyMesh);
      if (ImGui::Button("Apply to Selected Mesh")) {
        actions.applyMeshToSelection = true;
      }
      ImGui::EndDisabled();

      ImGui::TableNextColumn();
      ImGui::TextUnformatted("Sprite Meta");
      ImGui::TextWrapped("%s", state.activeSpriteMetaPath.c_str());
      ImGui::BeginDisabled(!selectedIsSpriteMeta);
      if (ImGui::Button("Use for sprites")) {
        state.activeSpriteMetaPath = selectedFile;
        actions.setActiveSpriteMeta = true;
      }
      ImGui::EndDisabled();
      const bool canApplySprite = selected && selected->isSprite;
      ImGui::BeginDisabled(!selectedIsSpriteMeta || !canApplySprite);
      if (ImGui::Button("Apply to Selected Sprite")) {
        actions.applySpriteMetaToSelection = true;
      }
      ImGui::EndDisabled();

      ImGui::TableNextColumn();
      ImGui::TextUnformatted("Material");
      ImGui::TextWrapped("%s", state.activeMaterialPath.empty() ? "-" : state.activeMaterialPath.c_str());
      ImGui::BeginDisabled(!selectedIsMaterial);
      if (ImGui::Button("Set Active Material")) {
        state.activeMaterialPath = selectedFile;
        actions.setActiveMaterial = true;
      }
      ImGui::EndDisabled();
      const bool canApplyMaterial = selected && selected->model != nullptr && !selected->isSprite && !selected->pointLight;
      ImGui::BeginDisabled(!selectedIsMaterial || !canApplyMaterial);
      if (ImGui::Button("Apply to Selected Mesh")) {
        state.activeMaterialPath = selectedFile;
        actions.setActiveMaterial = true;
        actions.applyMaterialToSelection = true;
      }
      ImGui::EndDisabled();
      ImGui::EndTable();
    }

    ImGui::End();
    return actions;
  }

  FileDialogActions BuildFileDialogPanel(FileDialogState &state, bool *open) {
    FileDialogActions actions{};

    if (open) {
      if (!ImGui::Begin(state.title.c_str(), open)) {
        ImGui::End();
        return actions;
      }
    } else {
      ImGui::Begin(state.title.c_str());
    }

    const BrowserSelection selection = drawBrowserView(
      state.browser,
      "FileDialog",
      true,
      true,
      false,
      nullptr);

    const std::string selectedFile = selection.selectedFile;
    std::string chosenPath = selectedFile;
    bool selectionIsDir = false;
    if (chosenPath.empty() && state.allowDirectories) {
      chosenPath = selection.selectedDir;
      selectionIsDir = !chosenPath.empty();
    }

    const bool canAccept = !chosenPath.empty() && (!selectionIsDir || state.allowDirectories);

    ImGui::Separator();
    ImGui::Text("Selected: %s", chosenPath.empty() ? "-" : chosenPath.c_str());
    bool acceptNow = selection.fileActivated;

    ImGui::BeginDisabled(!canAccept);
    if (ImGui::Button(state.okLabel.c_str())) {
      acceptNow = true;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      actions.canceled = true;
      if (open) {
        *open = false;
      }
    }

    if (acceptNow && canAccept) {
      actions.accepted = true;
      actions.selectedPath = chosenPath;
      if (open) {
        *open = false;
      }
    }

    ImGui::End();
    return actions;
  }

} // namespace lve::editor
