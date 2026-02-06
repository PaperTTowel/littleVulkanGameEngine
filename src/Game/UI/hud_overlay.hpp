#pragma once

#include <string>

namespace lve {
  class LveCamera;
  class LveGameObject;

  namespace game {
    struct PlayerStats;

    namespace ui {
      void drawPlayerHpBar(
        const LveCamera &camera,
        const LveGameObject &player,
        const PlayerStats &stats);
      void drawTimedMessage(const std::string &message, float remainingSeconds);
    } // namespace ui
  } // namespace game
} // namespace lve
