// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "open_live_bridge/api_types.h"
#include "open_live_bridge/obs/obs_controller.h"

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace olb {

class MediaBridgeService {
public:
    MediaBridgeService(std::unique_ptr<ObsController> obsController, BridgeConfig config);

    bool Warmup(std::string* error);
    bool Start(const StartRequest& request, std::string* error);
    bool Stop(std::string* error);
    bool ListAudioMonitoringDevices(std::vector<AudioMonitoringDeviceInfo>* devices, std::string* error);
    bool SetAudioMonitoringDevice(const AudioMonitoringDeviceInfo& device, std::string* error);
    BridgeStatus GetStatus() const;

private:
    bool EnsureObsInitialized(std::string* error);
    void SetErrorLocked(const std::string& error);

    mutable std::mutex mutex_;
    std::unique_ptr<ObsController> obsController_;
    BridgeConfig config_;
    BridgeStatus status_;
};

} // namespace olb
