// SPDX-License-Identifier: GPL-2.0-or-later

#include "open_live_bridge/virtual_camera/virtual_camera_registry.h"

#include "open_live_bridge/api_types.h"

#include <sstream>
#include <utility>

#if defined(_WIN32)
#include <windows.h>
#else
#error "Virtual camera registry checks currently support Windows only."
#endif

namespace olb {

namespace {

constexpr const char* kClsidPrefix = "SOFTWARE\\Classes\\CLSID\\";
constexpr const char* kVideoInputDeviceCategory = "{860BB310-5D01-11d0-BD3B-00A0C911CE86}";

std::string NormalizeClsid(std::string clsid)
{
    if (clsid.empty()) {
        return {};
    }

    if (clsid.front() != '{') {
        clsid.insert(clsid.begin(), '{');
    }

    if (clsid.back() != '}') {
        clsid.push_back('}');
    }

    return clsid;
}

bool QueryRegistryString(HKEY root, const std::string& subkey, const char* valueName, REGSAM view, std::string* value)
{
    HKEY key = nullptr;
    if (RegOpenKeyExA(root, subkey.c_str(), 0, KEY_READ | view, &key) != ERROR_SUCCESS) {
        return false;
    }

    DWORD type = 0;
    DWORD size = 0;
    if (RegQueryValueExA(key, valueName, nullptr, &type, nullptr, &size) != ERROR_SUCCESS || size == 0) {
        RegCloseKey(key);
        return false;
    }

    if (type != REG_SZ && type != REG_EXPAND_SZ) {
        RegCloseKey(key);
        return false;
    }

    std::string buffer(size, '\0');
    const LONG result = RegQueryValueExA(key, valueName, nullptr, &type, reinterpret_cast<LPBYTE>(buffer.data()), &size);
    RegCloseKey(key);
    if (result != ERROR_SUCCESS) {
        return false;
    }

    while (!buffer.empty() && buffer.back() == '\0') {
        buffer.pop_back();
    }

    *value = std::move(buffer);
    return true;
}

void QueryView(const std::string& clsid, REGSAM view, bool* registered, std::string* friendlyName, std::string* modulePath)
{
    const std::string comKey = std::string(kClsidPrefix) + clsid;
    const std::string inprocKey = comKey + "\\InprocServer32";
    const std::string categoryKey = std::string(kClsidPrefix) + kVideoInputDeviceCategory + "\\Instance\\" + clsid;

    std::string categoryName;
    std::string comName;
    std::string path;

    const bool hasCategoryName = QueryRegistryString(HKEY_LOCAL_MACHINE, categoryKey, "FriendlyName", view, &categoryName);
    const bool hasComName = QueryRegistryString(HKEY_LOCAL_MACHINE, comKey, nullptr, view, &comName);
    const bool hasModulePath = QueryRegistryString(HKEY_LOCAL_MACHINE, inprocKey, nullptr, view, &path);

    *registered = hasCategoryName || hasComName || hasModulePath;

    if (hasCategoryName) {
        *friendlyName = std::move(categoryName);
    } else if (hasComName) {
        *friendlyName = std::move(comName);
    } else {
        friendlyName->clear();
    }

    if (hasModulePath) {
        *modulePath = std::move(path);
    } else {
        modulePath->clear();
    }
}

} // namespace

bool VirtualCameraIdentity::HasClsid() const
{
    return !clsid.empty();
}

bool QueryVirtualCameraRegistration(
    const VirtualCameraIdentity& identity,
    VirtualCameraRegistrationStatus* status,
    std::string* error)
{
    if (!status) {
        if (error) {
            *error = "missing status output";
        }
        return false;
    }

    *status = {};
    status->checked = true;

    const auto normalizedClsid = NormalizeClsid(identity.clsid);
    if (normalizedClsid.empty()) {
        if (error) {
            *error = "virtual camera clsid is empty";
        }
        return false;
    }

    QueryView(normalizedClsid, KEY_WOW64_64KEY, &status->registered64, &status->friendlyName64, &status->modulePath64);
    QueryView(normalizedClsid, KEY_WOW64_32KEY, &status->registered32, &status->friendlyName32, &status->modulePath32);
    return true;
}

std::string RegistrationStatusToJson(
    const VirtualCameraIdentity& identity,
    const VirtualCameraRegistrationStatus& status)
{
    std::ostringstream out;
    out << "{";
    out << "\"name\":\"" << JsonEscape(identity.name) << "\",";
    out << "\"clsid\":\"" << JsonEscape(identity.clsid) << "\",";
    out << "\"checked\":" << (status.checked ? "true" : "false") << ",";
    out << "\"registered64\":" << (status.registered64 ? "true" : "false") << ",";
    out << "\"registered32\":" << (status.registered32 ? "true" : "false") << ",";
    out << "\"friendlyName64\":\"" << JsonEscape(status.friendlyName64) << "\",";
    out << "\"friendlyName32\":\"" << JsonEscape(status.friendlyName32) << "\",";
    out << "\"modulePath64\":\"" << JsonEscape(status.modulePath64) << "\",";
    out << "\"modulePath32\":\"" << JsonEscape(status.modulePath32) << "\"";
    out << "}";
    return out.str();
}

} // namespace olb

