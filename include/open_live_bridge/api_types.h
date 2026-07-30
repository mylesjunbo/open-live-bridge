// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "open_live_bridge/virtual_camera/virtual_camera_registry.h"

#include <cstdint>
#include <string>
#include <string_view>

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
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

struct BridgeStatus {
    BridgeState state = BridgeState::Idle;
    std::string currentUrl;
    std::string lastError;
    bool obsInitialized = false;
    bool virtualCameraActive = false;
    MediaSourceRuntimeStatus mediaSource;
    VirtualCameraIdentity virtualCamera;
    VirtualCameraRegistrationStatus virtualCameraRegistration;
};

const char* ToString(BridgeState state);
const char* ToString(VideoFitMode mode);
bool ParseVideoFitMode(std::string_view value, VideoFitMode* mode);
std::string JsonEscape(std::string_view value);
std::string StatusToJson(const BridgeStatus& status);

} // namespace olb
