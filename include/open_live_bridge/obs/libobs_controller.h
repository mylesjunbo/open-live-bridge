// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "open_live_bridge/obs/obs_controller.h"

#include <memory>

namespace olb {

class LibObsController final : public ObsController {
public:
    LibObsController();
    ~LibObsController() override;

    bool Initialize(const BridgeConfig& config, std::string* error) override;
    bool SetMediaSource(const std::string& url, std::string* error) override;
    bool RestartMediaSource(std::string* error) override;
    MediaSourceRuntimeStatus GetMediaSourceStatus() const override;
    bool GetAudioMonitoringDevices(std::vector<AudioMonitoringDeviceInfo>* devices, std::string* error) const override;
    bool SetAudioMonitoringDevice(const AudioMonitoringDeviceInfo& device, std::string* error) override;
    bool StartVirtualCamera(std::string* error) override;
    bool StopVirtualCamera(std::string* error) override;
    void Shutdown() override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace olb
