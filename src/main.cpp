#include "Engine/engine_loop.hpp"
#include "Engine/path_utils.hpp"

// std
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {
  std::filesystem::path getExecutableDirectory() {
#ifdef _WIN32
    wchar_t pathBuffer[MAX_PATH]{};
    const DWORD pathLen = GetModuleFileNameW(nullptr, pathBuffer, MAX_PATH);
    if (pathLen == 0 || pathLen >= MAX_PATH) {
      return std::filesystem::current_path();
    }
    return std::filesystem::path(pathBuffer).parent_path();
#else
    return std::filesystem::current_path();
#endif
  }

  std::string makeLogFileName() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
    std::tm localTime{};
#ifdef _WIN32
    localtime_s(&localTime, &nowTime);
#else
    localtime_r(&nowTime, &localTime);
#endif
    std::ostringstream oss;
    oss << "run_" << std::put_time(&localTime, "%Y%m%d_%H%M%S") << ".txt";
    return oss.str();
  }

  std::unique_ptr<std::ofstream> initializeFileLogging(std::string &outLogPath) {
    namespace fs = std::filesystem;

    const fs::path logDirectory = getExecutableDirectory() / "log";
    std::error_code ec;
    fs::create_directories(logDirectory, ec);

    const fs::path logPath = logDirectory / makeLogFileName();
    auto logFile = std::make_unique<std::ofstream>(logPath, std::ios::out | std::ios::trunc);
    if (!logFile->is_open()) {
      return {};
    }

    std::cout.rdbuf(logFile->rdbuf());
    std::cerr.rdbuf(logFile->rdbuf());
    std::cout.setf(std::ios::unitbuf);
    std::cerr.setf(std::ios::unitbuf);

    outLogPath = lve::pathutil::toUtf8(logPath);
    std::cout << "[Log] Started: " << outLogPath << '\n';
    return logFile;
  }
} // namespace

int main() {
  std::string logFilePath{};
  auto logFile = initializeFileLogging(logFilePath);

  try {
    lve::EngineLoop app{};
    app.run();
  } catch (const std::exception &e) {
    std::cerr << "[Fatal] " << e.what() << '\n';

#ifdef _WIN32
    std::string errorText = e.what();
    if (!logFilePath.empty()) {
      errorText += "\n\nLog file:\n" + logFilePath;
    }
    MessageBoxA(
      nullptr,
      errorText.c_str(),
      "2dVK Startup Error",
      MB_OK | MB_ICONERROR);
#endif
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
