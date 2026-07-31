// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "open_live_bridge/virtual_camera/virtual_camera_registry.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace olb {

enum class BridgeState {
    Idle,
    Starting,
    Running,
    Stopping,
    Failed,
};

enum class VideoFitMode {
    Contain,
    Cover,
    Stretch,
};

enum class AudioMonitoringMode {
    None,
    MonitorOnly,
    MonitorAndOutput,
};

struct BridgeConfig {
    std::string obsRoot;
    std::string sceneName = "OLB_MAIN_SCENE";
    std::string sourceName = "OLB_MEDIA_SOURCE";
    VirtualCameraIdentity virtualCamera;
    std::uint32_t width = 720;
    std::uint32_t height = 1280;
    std::uint32_t fps = 30;
    bool autoStartVirtualCamera = true;
    bool matchSourceSize = false;
    VideoFitMode fitMode = VideoFitMode::Contain;
    AudioMonitoringMode audioMonitoringMode = AudioMonitoringMode::MonitorAndOutput;
    std::string audioMonitoringDeviceId;
};

struct AudioMonitoringDeviceInfo {
    std::string name;
    std::string id;
};

struct StartRequest {
    std::string url;
    bool startVirtualCamera = true;
};

struct MediaSourceRuntimeStatus {
    std::string name;
    std::string state = "unknown";
    bool exists = false;
    bool active = false;
    bool audioActive = false;
    bool audioObserved = false;
    std::uint64_t audioFramesObserved = 0;
    double audioPeak = 0.0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

struct BridgeStatus {
    BridgeState state = BridgeState::Idle;
    std::string currentUrl;
    std::string lastError;
    bool obsInitialized = false;
    bool virtualCameraActive = false;
    AudioMonitoringMode audioMonitoringMode = AudioMonitoringMode::MonitorAndOutput;
    std::string audioMonitoringDeviceName;
    std::string audioMonitoringDeviceId;
    MediaSourceRuntimeStatus mediaSource;
    VirtualCameraIdentity virtualCamera;
    VirtualCameraRegistrationStatus virtualCameraRegistration;
};

const char* ToString(BridgeState state);
const char* ToString(VideoFitMode mode);
const char* ToString(AudioMonitoringMode mode);
bool ParseVideoFitMode(std::string_view value, VideoFitMode* mode);
bool ParseAudioMonitoringMode(std::string_view value, AudioMonitoringMode* mode);
std::string AudioMonitoringDeviceToJson(const AudioMonitoringDeviceInfo& device);
std::string AudioMonitoringDevicesToJson(const std::vector<AudioMonitoringDeviceInfo>& devices);
std::string JsonEscape(std::string_view value);
std::string StatusToJson(const BridgeStatus& status);

} // namespace olb
