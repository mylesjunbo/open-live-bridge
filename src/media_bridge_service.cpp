// SPDX-License-Identifier: GPL-2.0-or-later

#include "open_live_bridge/media_bridge_service.h"

#include "open_live_bridge/util/logger.h"
#include "open_live_bridge/virtual_camera/virtual_camera_registry.h"

#include <utility>

namespace olb {

MediaBridgeService::MediaBridgeService(std::unique_ptr<ObsController> obsController, BridgeConfig config)
    : obsController_(std::move(obsController))
    , config_(std::move(config))
{
    status_.virtualCamera = config_.virtualCamera;
    status_.audioMonitoringMode = config_.audioMonitoringMode;
    status_.audioMonitoringDeviceName = config_.audioMonitoringDeviceId.empty() ? "Default" : config_.audioMonitoringDeviceId;
    status_.audioMonitoringDeviceId = config_.audioMonitoringDeviceId.empty() ? "default" : config_.audioMonitoringDeviceId;
}

bool MediaBridgeService::Warmup(std::string* error)
{
    std::lock_guard lock(mutex_);

    status_.lastError.clear();

    if (!config_.autoStartVirtualCamera) {
        return true;
    }

    if (!EnsureObsInitialized(error)) {
        return false;
    }

    if (status_.virtualCameraActive) {
        return true;
    }

    if (!obsController_->StartVirtualCamera(error)) {
        SetErrorLocked(error ? *error : "failed to start standby virtual camera");
        return false;
    }

    status_.virtualCameraActive = true;
    Log(LogLevel::Info, "standby virtual camera started");
    return true;
}

bool MediaBridgeService::Start(const StartRequest& request, std::string* error)
{
    std::lock_guard lock(mutex_);

    if (request.url.empty()) {
        SetErrorLocked("missing media url");
        if (error) {
            *error = status_.lastError;
        }
        return false;
    }

    status_.state = BridgeState::Starting;
    status_.lastError.clear();

    if (!EnsureObsInitialized(error)) {
        return false;
    }

    if (status_.virtualCameraActive) {
        if (!obsController_->StopVirtualCamera(error)) {
            SetErrorLocked(error ? *error : "failed to stop virtual camera before updating media source");
            return false;
        }
        status_.virtualCameraActive = false;
    }

    if (!obsController_->SetMediaSource(request.url, error)) {
        SetErrorLocked(error ? *error : "failed to set media source");
        return false;
    }

    if (request.startVirtualCamera && config_.autoStartVirtualCamera) {
        if (!obsController_->StartVirtualCamera(error)) {
            SetErrorLocked(error ? *error : "failed to start virtual camera");
            return false;
        }
        if (!obsController_->RestartMediaSource(error)) {
            SetErrorLocked(error ? *error : "failed to restart media source");
            return false;
        }
        status_.virtualCameraActive = true;
    }

    status_.currentUrl = request.url;
    status_.state = BridgeState::Running;
    Log(LogLevel::Info, "media bridge started");
    return true;
}

bool MediaBridgeService::Stop(std::string* error)
{
    std::lock_guard lock(mutex_);

    status_.state = BridgeState::Stopping;
    status_.lastError.clear();

    if (status_.obsInitialized && !obsController_->StopVirtualCamera(error)) {
        SetErrorLocked(error ? *error : "failed to stop virtual camera");
        return false;
    }

    status_.virtualCameraActive = false;
    status_.currentUrl.clear();
    status_.state = BridgeState::Idle;
    Log(LogLevel::Info, "media bridge stopped");
    return true;
}

bool MediaBridgeService::ListAudioMonitoringDevices(
    std::vector<AudioMonitoringDeviceInfo>* devices,
    std::string* error)
{
    std::lock_guard lock(mutex_);

    if (!EnsureObsInitialized(error)) {
        return false;
    }

    return obsController_->GetAudioMonitoringDevices(devices, error);
}

bool MediaBridgeService::SetAudioMonitoringDevice(
    const AudioMonitoringDeviceInfo& device,
    std::string* error)
{
    std::lock_guard lock(mutex_);

    status_.lastError.clear();

    if (!EnsureObsInitialized(error)) {
        return false;
    }

    if (!obsController_->SetAudioMonitoringDevice(device, error)) {
        SetErrorLocked(error ? *error : "failed to set audio monitoring device");
        return false;
    }

    const auto normalizedId = device.id.empty() ? std::string("default") : device.id;
    const auto normalizedName = device.name.empty() ? (normalizedId == "default" ? "Default" : normalizedId) : device.name;
    status_.audioMonitoringDeviceName = normalizedName;
    status_.audioMonitoringDeviceId = normalizedId;
    Log(LogLevel::Info, "audio monitoring device changed: " + status_.audioMonitoringDeviceId);
    return true;
}

BridgeStatus MediaBridgeService::GetStatus() const
{
    std::lock_guard lock(mutex_);
    BridgeStatus status = status_;
    if (status_.obsInitialized && obsController_) {
        status.mediaSource = obsController_->GetMediaSourceStatus();
    }
    return status;
}

bool MediaBridgeService::EnsureObsInitialized(std::string* error)
{
    if (status_.obsInitialized) {
        return true;
    }

    if (!obsController_->Initialize(config_, error)) {
        SetErrorLocked(error ? *error : "failed to initialize obs");
        return false;
    }

    if (config_.virtualCamera.HasClsid()) {
        VirtualCameraRegistrationStatus registrationStatus;
        std::string registrationError;
        if (QueryVirtualCameraRegistration(config_.virtualCamera, &registrationStatus, &registrationError)) {
            status_.virtualCameraRegistration = registrationStatus;
            if (!registrationStatus.registered64) {
                SetErrorLocked("expected virtual camera is not registered for 64-bit clients");
                if (error) {
                    *error = status_.lastError;
                }
                return false;
            }

        } else {
            Log(LogLevel::Warning, "virtual camera registration check failed: " + registrationError);
        }
    }

    status_.obsInitialized = true;
    return true;
}

void MediaBridgeService::SetErrorLocked(const std::string& error)
{
    status_.lastError = error;
    status_.state = BridgeState::Failed;
    Log(LogLevel::Error, error);
}

} // namespace olb
