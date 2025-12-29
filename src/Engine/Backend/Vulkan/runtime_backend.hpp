#pragma once

#include "Engine/Backend/runtime_backend.hpp"
#include "Engine/Backend/Vulkan/Core/device.hpp"
#include "Engine/Backend/Vulkan/Core/vulkan_input.hpp"
#include "Engine/Backend/Vulkan/Core/window_backend.hpp"
#include "Engine/Backend/Vulkan/Editor/editor_render_backend.hpp"
#include "Engine/Backend/Vulkan/Render/asset_factory.hpp"
#include "Engine/Backend/Vulkan/Render/render_backend.hpp"
#include "Engine/scene_system.hpp"

#include <memory>
#include <string>

namespace lve::backend {
  class VulkanRuntimeBackend final : public RuntimeBackend {
  public:
    VulkanRuntimeBackend(int width, int height, std::string title);

    WindowBackend &window() override { return windowBackend; }
    RenderBackend &renderBackend() override { return renderBackendImpl; }
    EditorRenderBackend &editorBackend() override { return editorBackendImpl; }
    SceneSystem &sceneSystem() override { return sceneSystemImpl; }

  private:
    LveWindow windowImpl;
    VulkanInputProvider inputProvider;
    VulkanWindowBackend windowBackend;
    LveDevice device;
    VulkanRenderAssetFactory assetFactory;
    SceneSystem sceneSystemImpl;
    VulkanRenderBackend renderBackendImpl;
    VulkanEditorRenderBackend editorBackendImpl;
  };
} // namespace lve::backend
