#include "character_panel.hpp"

#include <imgui.h>

namespace lve {

  void BuildCharacterPanel(LveGameObject &character, SpriteAnimator &animator) {
    ImGui::Begin("Character");
    auto stateToStr = [](ObjectState s) {
      switch (s) {
        case ObjectState::WALKING: return "Walking";
        case ObjectState::IDLE:
        default: return "Idle";
      }
    };
    ImGui::Text("State: %s", stateToStr(character.objState));
    ImGui::Text("Frame: %d", character.currentFrame);
    ImGui::Text("Atlas: %d x %d", character.atlasColumns, character.atlasRows);
    ImGui::Text("Texture: %s", animator.getCurrentTexturePath().c_str());

    if (ImGui::Button("Idle")) {
      character.objState = ObjectState::IDLE;
      animator.applySpriteState(character, character.objState);
      character.currentFrame = 0;
    }
    ImGui::SameLine();
    if (ImGui::Button("Walk")) {
      character.objState = ObjectState::WALKING;
      animator.applySpriteState(character, character.objState);
      character.currentFrame = 0;
    }

    ImGui::Text("Billboard mode:");
    int mode = static_cast<int>(character.billboardMode);
    const char* modeLabels[] = { "None", "Cylindrical", "Spherical" };
    if (ImGui::Combo("##billboard", &mode, modeLabels, IM_ARRAYSIZE(modeLabels))) {
      character.billboardMode = static_cast<BillboardMode>(mode);
    }
    ImGui::End();
  }

} // namespace lve
