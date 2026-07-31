// SPDX-License-Identifier: GPL-2.0-or-later

#include "open_live_bridge/api_types.h"

#include <vector>
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

const char* ToString(AudioMonitoringMode mode)
{
    switch (mode) {
    case AudioMonitoringMode::None:
        return "none";
    case AudioMonitoringMode::MonitorOnly:
        return "monitor";
    case AudioMonitoringMode::MonitorAndOutput:
        return "both";
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

bool ParseAudioMonitoringMode(std::string_view value, AudioMonitoringMode* mode)
{
    if (!mode) {
        return false;
    }

    if (value == "none" || value == "off" || value == "disabled") {
        *mode = AudioMonitoringMode::None;
        return true;
    }

    if (value == "monitor" || value == "monitor-only" || value == "monitor_only") {
        *mode = AudioMonitoringMode::MonitorOnly;
        return true;
    }

    if (value == "both" || value == "monitor-and-output" || value == "monitor_and_output" || value == "output") {
        *mode = AudioMonitoringMode::MonitorAndOutput;
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
    out << "\"audioActive\":" << (status.audioActive ? "true" : "false") << ",";
    out << "\"audioObserved\":" << (status.audioObserved ? "true" : "false") << ",";
    out << "\"audioFramesObserved\":" << status.audioFramesObserved << ",";
    out << "\"audioPeak\":" << status.audioPeak << ",";
    out << "\"width\":" << status.width << ",";
    out << "\"height\":" << status.height;
    out << "}";
    return out.str();
}

std::string AudioMonitoringDeviceToJson(const AudioMonitoringDeviceInfo& device)
{
    std::ostringstream out;
    out << "{";
    out << "\"name\":\"" << JsonEscape(device.name) << "\",";
    out << "\"id\":\"" << JsonEscape(device.id) << "\"";
    out << "}";
    return out.str();
}

std::string AudioMonitoringDevicesToJson(const std::vector<AudioMonitoringDeviceInfo>& devices)
{
    std::ostringstream out;
    out << "[";
    for (std::size_t i = 0; i < devices.size(); ++i) {
        if (i > 0) {
            out << ",";
        }
        out << AudioMonitoringDeviceToJson(devices[i]);
    }
    out << "]";
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
    out << "\"audioMonitoringMode\":\"" << ToString(status.audioMonitoringMode) << "\",";
    out << "\"audioMonitoringDeviceName\":\"" << JsonEscape(status.audioMonitoringDeviceName) << "\",";
    out << "\"audioMonitoringDeviceId\":\"" << JsonEscape(status.audioMonitoringDeviceId) << "\",";
    out << "\"mediaSource\":" << MediaSourceStatusToJson(status.mediaSource) << ",";
    out << "\"virtualCamera\":" << RegistrationStatusToJson(status.virtualCamera, status.virtualCameraRegistration);
    out << "}";
    return out.str();
}

} // namespace olb
