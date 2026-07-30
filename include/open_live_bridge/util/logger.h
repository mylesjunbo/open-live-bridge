// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

namespace olb {

enum class LogLevel {
    Info,
    Warning,
    Error,
};

void Log(LogLevel level, const std::string& message);

} // namespace olb

