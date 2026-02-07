#pragma once

#include <algorithm>
#include <filesystem>
#include <string>
#include <string_view>

namespace lve::pathutil {
  inline std::string toUtf8(const std::filesystem::path &path) {
#if defined(__cpp_char8_t)
    const auto text = path.u8string();
    return {text.begin(), text.end()};
#else
    return path.u8string();
#endif
  }

  inline std::string toGenericUtf8(const std::filesystem::path &path) {
    std::string text = toUtf8(path.lexically_normal());
    std::replace(text.begin(), text.end(), '\\', '/');
    return text;
  }

  inline std::filesystem::path fromUtf8(std::string_view text) {
#if defined(_WIN32)
#if defined(__cpp_char8_t)
    std::u8string utf8;
    utf8.reserve(text.size());
    for (char ch : text) {
      utf8.push_back(static_cast<char8_t>(static_cast<unsigned char>(ch)));
    }
    return std::filesystem::path(utf8);
#else
    return std::filesystem::u8path(text.begin(), text.end());
#endif
#else
    return std::filesystem::path(text);
#endif
  }
} // namespace lve::pathutil
