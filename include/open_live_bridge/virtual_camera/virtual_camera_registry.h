// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

namespace olb {

inline constexpr const char* kDefaultVirtualCameraName = "OpenLiveBridge Virtual Camera";
inline constexpr const char* kDefaultVirtualCameraClsid = "{634ed3f8-eaa2-4e70-93f0-185337ef9b48}";

struct VirtualCameraIdentity {
    std::string name = kDefaultVirtualCameraName;
    std::string clsid = kDefaultVirtualCameraClsid;
    std::string module64Path;
    std::string module32Path;

    bool HasClsid() const;
};

struct VirtualCameraRegistrationStatus {
    bool checked = false;
    bool registered64 = false;
    bool registered32 = false;
    std::string friendlyName64;
    std::string friendlyName32;
    std::string modulePath64;
    std::string modulePath32;
};

bool QueryVirtualCameraRegistration(
    const VirtualCameraIdentity& identity,
    VirtualCameraRegistrationStatus* status,
    std::string* error);

std::string RegistrationStatusToJson(
    const VirtualCameraIdentity& identity,
    const VirtualCameraRegistrationStatus& status);

} // namespace olb
