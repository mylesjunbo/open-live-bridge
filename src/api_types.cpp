// SPDX-License-Identifier: GPL-2.0-or-later

#include "open_live_bridge/api_types.h"

#include <sstream>

namespace olb {

const char* ToString(BridgeState state)
{
    switch (state) {
    case BridgeState::Idle:
        return "idle";
    case BridgeState::Starting:
        return "starting";
    case BridgeState::Running:
        return "running";
    case BridgeState::Stopping:
        return "stopping";
    case BridgeState::Failed:
        return "failed";
    }

    return "unknown";
}

const char* ToString(VideoFitMode mode)
{
    switch (mode) {
    case VideoFitMode::Contain:
        return "contain";
    case VideoFitMode::Cover:
        return "cover";
    case VideoFitMode::Stretch:
        return "stretch";
    }

    return "unknown";
}

bool ParseVideoFitMode(std::string_view value, VideoFitMode* mode)
{
    if (!mode) {
        return false;
    }

    if (value == "contain" || value == "fit") {
        *mode = VideoFitMode::Contain;
        return true;
    }

    if (value == "cover" || value == "fill") {
        *mode = VideoFitMode::Cover;
        return true;
    }

    if (value == "stretch") {
        *mode = VideoFitMode::Stretch;
        return true;
    }

    return false;
}

std::string JsonEscape(std::string_view value)
{
    std::ostringstream out;
    for (const char ch : value) {
        const auto uch = static_cast<unsigned char>(ch);
        switch (ch) {
        case '\\':
            out << "\\\\";
            break;
        case '"':
            out << "\\\"";
            break;
        case '\b':
            out << "\\b";
            break;
        case '\f':
            out << "\\f";
            break;
        case '\n':
            out << "\\n";
            break;
        case '\r':
            out << "\\r";
            break;
        case '\t':
            out << "\\t";
            break;
        default:
            if (uch < 0x20) {
                out << "\\u00";
                constexpr char hex[] = "0123456789abcdef";
                out << hex[(uch >> 4) & 0x0f] << hex[uch & 0x0f];
            } else {
                out << ch;
            }
        }
    }

    return out.str();
}

std::string MediaSourceStatusToJson(const MediaSourceRuntimeStatus& status)
{
    std::ostringstream out;
    out << "{";
    out << "\"name\":\"" << JsonEscape(status.name) << "\",";
    out << "\"state\":\"" << JsonEscape(status.state) << "\",";
    out << "\"exists\":" << (status.exists ? "true" : "false") << ",";
    out << "\"active\":" << (status.active ? "true" : "false") << ",";
    out << "\"width\":" << status.width << ",";
    out << "\"height\":" << status.height;
    out << "}";
    return out.str();
}

std::string StatusToJson(const BridgeStatus& status)
{
    std::ostringstream out;
    out << "{";
    out << "\"state\":\"" << ToString(status.state) << "\",";
    out << "\"currentUrl\":\"" << JsonEscape(status.currentUrl) << "\",";
    out << "\"lastError\":\"" << JsonEscape(status.lastError) << "\",";
    out << "\"obsInitialized\":" << (status.obsInitialized ? "true" : "false") << ",";
    out << "\"virtualCameraActive\":" << (status.virtualCameraActive ? "true" : "false") << ",";
    out << "\"mediaSource\":" << MediaSourceStatusToJson(status.mediaSource) << ",";
    out << "\"virtualCamera\":" << RegistrationStatusToJson(status.virtualCamera, status.virtualCameraRegistration);
    out << "}";
    return out.str();
}

} // namespace olb
