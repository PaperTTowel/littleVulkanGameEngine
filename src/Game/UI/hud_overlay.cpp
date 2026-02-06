#include "Game/UI/hud_overlay.hpp"

#include "Engine/camera.hpp"
#include "Game/player_controller.hpp"
#include "utils/game_object.hpp"

#include <imgui.h>

namespace lve::game::ui {
  namespace {
    bool worldToScreen(
      const LveCamera &camera,
      const glm::vec3 &worldPos,
      float screenWidth,
      float screenHeight,
      ImVec2 &outPos) {
      const glm::vec4 clip = camera.getProjection() * camera.getView() * glm::vec4(worldPos, 1.f);
      if (clip.w <= 0.0001f) {
        return false;
      }
      const glm::vec3 ndc = glm::vec3(clip) / clip.w;
      if (ndc.x < -1.f || ndc.x > 1.f || ndc.y < -1.f || ndc.y > 1.f) {
        return false;
      }
      const float x = (ndc.x * 0.5f + 0.5f) * screenWidth;
      const float y = (ndc.y * 0.5f + 0.5f) * screenHeight;
      outPos = ImVec2(x, y);
      return true;
    }
  } // namespace

  void drawPlayerHpBar(
    const LveCamera &camera,
    const LveGameObject &player,
    const PlayerStats &stats) {
    if (stats.maxHp <= 0.f) {
      return;
    }
    float ratio = stats.hp / stats.maxHp;
    if (ratio < 0.f) ratio = 0.f;
    if (ratio > 1.f) ratio = 1.f;

    ImGuiViewport *viewport = ImGui::GetMainViewport();
    if (!viewport) {
      return;
    }

    const float halfHeight = player.transform.scale.y * 0.5f;
    const glm::vec3 barWorldPos = player.transform.translation + glm::vec3(-0.1f, -halfHeight - 0.1f, 0.f);
    ImVec2 screenPos{};
    if (!worldToScreen(camera, barWorldPos, viewport->Size.x, viewport->Size.y, screenPos)) {
      return;
    }
    screenPos.x += viewport->Pos.x;
    screenPos.y += viewport->Pos.y;

    const float barWidth = 36.f;
    const float barHeight = 5.f;
    const ImVec2 p0{screenPos.x - barWidth * 0.5f, screenPos.y - barHeight};
    const ImVec2 p1{screenPos.x + barWidth * 0.5f, screenPos.y};

    ImDrawList *draw = ImGui::GetForegroundDrawList();
    const ImU32 bg = IM_COL32(20, 20, 20, 200);
    const ImU32 fg = IM_COL32(220, 70, 70, 230);
    const ImU32 border = IM_COL32(0, 0, 0, 255);

    draw->AddRectFilled(p0, p1, bg, 2.0f);
    const ImVec2 p1fg{p0.x + barWidth * ratio, p1.y};
    draw->AddRectFilled(p0, p1fg, fg, 2.0f);
    draw->AddRect(p0, p1, border, 2.0f);
  }

  void drawTimedMessage(const std::string &message, float remainingSeconds) {
    if (message.empty() || remainingSeconds <= 0.f) {
      return;
    }

    ImGuiViewport *viewport = ImGui::GetMainViewport();
    if (!viewport) {
      return;
    }

    ImDrawList *draw = ImGui::GetForegroundDrawList(viewport);
    const ImVec2 textSize = ImGui::CalcTextSize(message.c_str());

    const float padX = 18.f;
    const float padY = 10.f;
    const ImVec2 center{
      viewport->Pos.x + (viewport->Size.x * 0.5f),
      viewport->Pos.y + (viewport->Size.y * 0.82f)};

    const ImVec2 p0{
      center.x - (textSize.x * 0.5f) - padX,
      center.y - (textSize.y * 0.5f) - padY};
    const ImVec2 p1{
      center.x + (textSize.x * 0.5f) + padX,
      center.y + (textSize.y * 0.5f) + padY};

    draw->AddRectFilled(p0, p1, IM_COL32(15, 15, 20, 215), 6.0f);
    draw->AddRect(p0, p1, IM_COL32(220, 220, 220, 230), 6.0f);
    draw->AddText(
      ImVec2{center.x - (textSize.x * 0.5f), center.y - (textSize.y * 0.5f)},
      IM_COL32(245, 245, 245, 255),
      message.c_str());
  }

} // namespace lve::game::ui
