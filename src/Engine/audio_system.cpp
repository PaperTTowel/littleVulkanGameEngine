#include "Engine/audio_system.hpp"
#include "Engine/path_utils.hpp"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

// std
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace lve {
  namespace {
    namespace fs = std::filesystem;

    struct ClipInfo {
      fs::path path;
      std::string displayName;
      std::string stemName;
    };

    std::string trimAscii(std::string value) {
      std::size_t start = 0;
      while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
        ++start;
      }

      std::size_t end = value.size();
      while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
      }

      return value.substr(start, end - start);
    }

    std::string normalizeKey(std::string value) {
      value = trimAscii(std::move(value));
      std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
      });
      return value;
    }

    bool startsWithNoCase(std::string_view text, std::string_view prefix) {
      if (text.size() < prefix.size()) {
        return false;
      }

      for (std::size_t i = 0; i < prefix.size(); ++i) {
        const auto a = static_cast<unsigned char>(text[i]);
        const auto b = static_cast<unsigned char>(prefix[i]);
        if (std::tolower(a) != std::tolower(b)) {
          return false;
        }
      }
      return true;
    }

    bool isSupportedAudioFile(const fs::path &path) {
      std::string ext = normalizeKey(pathutil::toUtf8(path.extension()));
      return ext == ".mp3" || ext == ".wav" || ext == ".ogg" || ext == ".flac";
    }

    std::string stripTag(const std::string &name, std::string_view tag) {
      if (!startsWithNoCase(name, tag)) {
        return name;
      }
      return trimAscii(name.substr(tag.size()));
    }

    void insertClipAlias(
      std::unordered_map<std::string, ClipInfo> &clips,
      const std::string &alias,
      const ClipInfo &clip) {
      const std::string key = normalizeKey(alias);
      if (key.empty()) {
        return;
      }
      clips.emplace(key, clip);
    }

    std::string registerClip(
      std::unordered_map<std::string, ClipInfo> &clips,
      const ClipInfo &clip,
      std::string_view categoryPrefix) {
      const std::string primary = normalizeKey(clip.displayName);
      if (!primary.empty()) {
        clips.emplace(primary, clip);
      }

      insertClipAlias(clips, clip.stemName, clip);
      insertClipAlias(clips, pathutil::toUtf8(clip.path.filename()), clip);

      if (!primary.empty()) {
        insertClipAlias(clips, std::string(categoryPrefix) + clip.displayName, clip);
      }

      return primary;
    }

    const ClipInfo *findClip(
      const std::unordered_map<std::string, ClipInfo> &clips,
      const std::string &clipName) {
      const auto it = clips.find(normalizeKey(clipName));
      if (it == clips.end()) {
        return nullptr;
      }
      return &it->second;
    }

    void cleanupSeVoices(std::vector<std::unique_ptr<ma_sound>> &voices) {
      for (std::size_t i = 0; i < voices.size();) {
        ma_sound *sound = voices[i].get();
        if (sound == nullptr) {
          voices.erase(voices.begin() + static_cast<std::ptrdiff_t>(i));
          continue;
        }
        if (ma_sound_is_playing(sound) == MA_FALSE) {
          ma_sound_uninit(sound);
          voices.erase(voices.begin() + static_cast<std::ptrdiff_t>(i));
          continue;
        }
        ++i;
      }
    }
  } // namespace

  struct AudioSystem::Impl {
    ma_engine engine{};
    bool isInitialized{false};

    std::unordered_map<std::string, ClipInfo> bgmClips{};
    std::unordered_map<std::string, ClipInfo> seClips{};
    std::vector<std::string> bgmOrder{};

    ma_sound currentBgm{};
    bool hasCurrentBgm{false};
    std::vector<std::unique_ptr<ma_sound>> activeSeVoices{};
  };

  AudioSystem::AudioSystem()
    : impl(std::make_unique<Impl>()) {}

  AudioSystem::~AudioSystem() {
    shutdown();
  }

  bool AudioSystem::init() {
    if (!impl) {
      impl = std::make_unique<Impl>();
    }
    if (impl->isInitialized) {
      return true;
    }

    ma_engine_config config = ma_engine_config_init();
    const ma_result result = ma_engine_init(&config, &impl->engine);
    if (result != MA_SUCCESS) {
      std::cerr << "Audio init failed. miniaudio error: " << result << "\n";
      return false;
    }

    impl->isInitialized = true;
    return true;
  }

  bool AudioSystem::loadFromDirectory(const std::string &directoryPath) {
    if (!impl || !impl->isInitialized) {
      return false;
    }

    stopBgm();
    for (auto &voice : impl->activeSeVoices) {
      if (voice) {
        ma_sound_uninit(voice.get());
      }
    }
    impl->activeSeVoices.clear();
    impl->bgmClips.clear();
    impl->seClips.clear();
    impl->bgmOrder.clear();

    const fs::path root{pathutil::fromUtf8(directoryPath)};
    if (!fs::exists(root)) {
      return false;
    }

    std::vector<ClipInfo> discoveredBgm{};
    std::vector<ClipInfo> discoveredSe{};

    std::error_code ec;
    fs::recursive_directory_iterator it{
      root,
      fs::directory_options::skip_permission_denied,
      ec};
    fs::recursive_directory_iterator end{};
    if (ec) {
      std::cerr << "Audio directory scan failed: " << directoryPath << "\n";
      return false;
    }

    for (; it != end; it.increment(ec)) {
      if (ec) {
        break;
      }

      const fs::directory_entry &entry = *it;
      if (!entry.is_regular_file(ec) || ec) {
        ec.clear();
        continue;
      }

      const fs::path filePath = entry.path();
      if (!isSupportedAudioFile(filePath)) {
        continue;
      }

      const std::string stemName = pathutil::toUtf8(filePath.stem());
      ClipInfo clip{};
      clip.path = filePath;
      clip.stemName = stemName;

      if (startsWithNoCase(stemName, "[BGM]")) {
        clip.displayName = stripTag(stemName, "[BGM]");
        discoveredBgm.push_back(std::move(clip));
      } else if (startsWithNoCase(stemName, "[SE]")) {
        clip.displayName = stripTag(stemName, "[SE]");
        discoveredSe.push_back(std::move(clip));
      }
    }

    auto sortByPath = [](const ClipInfo &a, const ClipInfo &b) {
      return normalizeKey(pathutil::toUtf8(a.path)) < normalizeKey(pathutil::toUtf8(b.path));
    };
    std::sort(discoveredBgm.begin(), discoveredBgm.end(), sortByPath);
    std::sort(discoveredSe.begin(), discoveredSe.end(), sortByPath);

    for (const auto &clip : discoveredBgm) {
      const std::string primaryKey = registerClip(impl->bgmClips, clip, "bgm:");
      if (!primaryKey.empty()) {
        impl->bgmOrder.push_back(primaryKey);
      }
    }

    for (const auto &clip : discoveredSe) {
      registerClip(impl->seClips, clip, "se:");
    }

    return !impl->bgmClips.empty() || !impl->seClips.empty();
  }

  bool AudioSystem::playBgm(const std::string &clipName, bool loop) {
    if (!impl || !impl->isInitialized) {
      return false;
    }

    const ClipInfo *clip = findClip(impl->bgmClips, clipName);
    if (!clip) {
      return false;
    }

    stopBgm();

    ma_result result = MA_ERROR;
#if defined(_WIN32)
    result = ma_sound_init_from_file_w(
      &impl->engine,
      clip->path.c_str(),
      MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION,
      nullptr,
      nullptr,
      &impl->currentBgm);
#else
    const std::string filePath = pathutil::toUtf8(clip->path);
    result = ma_sound_init_from_file(
      &impl->engine,
      filePath.c_str(),
      MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION,
      nullptr,
      nullptr,
      &impl->currentBgm);
#endif
    if (result != MA_SUCCESS) {
      std::cerr << "Failed to load BGM: " << clipName << " (error " << result << ")\n";
      return false;
    }

    ma_sound_set_looping(&impl->currentBgm, loop ? MA_TRUE : MA_FALSE);
    ma_sound_set_volume(&impl->currentBgm, 0.6f);
    result = ma_sound_start(&impl->currentBgm);
    if (result != MA_SUCCESS) {
      ma_sound_uninit(&impl->currentBgm);
      std::cerr << "Failed to play BGM: " << clipName << " (error " << result << ")\n";
      return false;
    }

    impl->hasCurrentBgm = true;
    return true;
  }

  bool AudioSystem::playFirstBgm() {
    if (!impl || impl->bgmOrder.empty()) {
      return false;
    }
    return playBgm(impl->bgmOrder.front(), true);
  }

  bool AudioSystem::playSe(const std::string &clipName) {
    if (!impl || !impl->isInitialized) {
      return false;
    }

    const ClipInfo *clip = findClip(impl->seClips, clipName);
    if (!clip) {
      return false;
    }

    cleanupSeVoices(impl->activeSeVoices);

    auto sound = std::make_unique<ma_sound>();
    ma_result result = MA_ERROR;
#if defined(_WIN32)
    result = ma_sound_init_from_file_w(
      &impl->engine,
      clip->path.c_str(),
      MA_SOUND_FLAG_NO_SPATIALIZATION,
      nullptr,
      nullptr,
      sound.get());
#else
    const std::string filePath = pathutil::toUtf8(clip->path);
    result = ma_sound_init_from_file(
      &impl->engine,
      filePath.c_str(),
      MA_SOUND_FLAG_NO_SPATIALIZATION,
      nullptr,
      nullptr,
      sound.get());
#endif
    if (result != MA_SUCCESS) {
      std::cerr << "Failed to load SE: " << clipName << " (error " << result << ")\n";
      return false;
    }

    ma_sound_set_volume(sound.get(), 1.0f);
    result = ma_sound_start(sound.get());
    if (result != MA_SUCCESS) {
      ma_sound_uninit(sound.get());
      std::cerr << "Failed to play SE: " << clipName << " (error " << result << ")\n";
      return false;
    }

    impl->activeSeVoices.push_back(std::move(sound));
    return true;
  }

  bool AudioSystem::hasBgm(const std::string &clipName) const {
    if (!impl) {
      return false;
    }
    return findClip(impl->bgmClips, clipName) != nullptr;
  }

  bool AudioSystem::hasSe(const std::string &clipName) const {
    if (!impl) {
      return false;
    }
    return findClip(impl->seClips, clipName) != nullptr;
  }

  void AudioSystem::stopBgm() {
    if (!impl || !impl->hasCurrentBgm) {
      return;
    }
    ma_sound_stop(&impl->currentBgm);
    ma_sound_uninit(&impl->currentBgm);
    impl->hasCurrentBgm = false;
  }

  void AudioSystem::shutdown() {
    if (!impl || !impl->isInitialized) {
      return;
    }

    stopBgm();

    for (auto &voice : impl->activeSeVoices) {
      if (voice) {
        ma_sound_uninit(voice.get());
      }
    }
    impl->activeSeVoices.clear();
    impl->bgmClips.clear();
    impl->seClips.clear();
    impl->bgmOrder.clear();

    ma_engine_uninit(&impl->engine);
    impl->isInitialized = false;
  }
} // namespace lve
