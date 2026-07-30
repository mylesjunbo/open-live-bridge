// SPDX-License-Identifier: GPL-2.0-or-later

#include "open_live_bridge/util/logger.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace olb {

namespace {

const char* LevelName(LogLevel level)
{
    switch (level) {
    case LogLevel::Info:
        return "INFO";
    case LogLevel::Warning:
        return "WARN";
    case LogLevel::Error:
        return "ERROR";
    }

    return "INFO";
}

std::string NowText()
{
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);

    std::tm localTime{};
#if defined(_WIN32)
    localtime_s(&localTime, &time);
#else
    localtime_r(&time, &localTime);
#endif

    std::ostringstream out;
    out << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
    return out.str();
}

} // namespace

void Log(LogLevel level, const std::string& message)
{
    std::ostream& out = level == LogLevel::Error ? std::cerr : std::cout;
    out << "[" << NowText() << "] [" << LevelName(level) << "] " << message << std::endl;
}

} // namespace olb

