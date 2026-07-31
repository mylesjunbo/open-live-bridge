// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "open_live_bridge/api_types.h"

#include <string>
#include <vector>

namespace olb {

class ObsController {
public:
    virtual ~ObsController() = default;

    virtual bool Initialize(const BridgeConfig& config, std::string* error) = 0;
    virtual bool SetMediaSource(const std::string& url, std::string* error) = 0;
    virtual bool RestartMediaSource(std::string* error) = 0;
    virtual MediaSourceRuntimeStatus GetMediaSourceStatus() const = 0;
    virtual bool GetAudioMonitoringDevices(std::vector<AudioMonitoringDeviceInfo>* devices, std::string* error) const = 0;
    virtual bool SetAudioMonitoringDevice(const AudioMonitoringDeviceInfo& device, std::string* error) = 0;
    virtual bool StartVirtualCamera(std::string* error) = 0;
    virtual bool StopVirtualCamera(std::string* error) = 0;
    virtual void Shutdown() = 0;
};

} // namespace olb
