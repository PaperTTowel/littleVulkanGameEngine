#pragma once

#include <memory>
#include <string>

namespace lve {
  class AudioSystem {
  public:
    AudioSystem();
    ~AudioSystem();

    AudioSystem(const AudioSystem &) = delete;
    AudioSystem &operator=(const AudioSystem &) = delete;
    AudioSystem(AudioSystem &&) = delete;
    AudioSystem &operator=(AudioSystem &&) = delete;

    bool init();
    bool loadFromDirectory(const std::string &directoryPath);
    bool playBgm(const std::string &clipName, bool loop = true);
    bool playFirstBgm();
    bool playSe(const std::string &clipName);
    bool hasBgm(const std::string &clipName) const;
    bool hasSe(const std::string &clipName) const;
    void stopBgm();
    void shutdown();

  private:
    struct Impl;
    std::unique_ptr<Impl> impl;
  };
} // namespace lve

