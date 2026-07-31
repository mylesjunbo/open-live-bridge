// SPDX-License-Identifier: GPL-2.0-or-later

#include "open_live_bridge/api_types.h"
#include "open_live_bridge/media_bridge_service.h"
#include "open_live_bridge/obs/libobs_controller.h"
#include "open_live_bridge/server/local_api_server.h"
#include "open_live_bridge/util/logger.h"
#include "open_live_bridge/virtual_camera/virtual_camera_registry.h"

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct RuntimeOptions {
    std::string host = "127.0.0.1";
    std::uint16_t port = 27177;
    olb::BridgeConfig bridgeConfig;
    bool checkVirtualCamera = false;
    bool listAudioMonitoringDevices = false;
};

void PrintHelp()
{
    std::cout
        << "OpenLiveBridge " << OPENLIVEBRIDGE_VERSION << "\n"
        << "\n"
        << "用法：\n"
        << "  OpenLiveBridge.exe [options]\n"
        << "\n"
        << "参数：\n"
        << "  --host <ip>              本地 API 监听主机，默认：127.0.0.1\n"
        << "  --port <port>            本地 API 监听端口，默认：27177\n"
        << "  --obs-root <path>        便携版 OBS 运行时根目录。\n"
        << "  --width <px>             OBS 基础/输出宽度，默认：720\n"
        << "  --height <px>            OBS 基础/输出高度，默认：1280\n"
        << "  --fps <fps>              OBS 输出帧率，默认：30\n"
        << "  --fit-mode <mode>        Video fit mode: contain, cover, stretch. Default: contain\n"
        << "  --audio-monitoring-mode <mode> Audio route: none, monitor, both. Default: both\n"
        << "  --audio-monitoring-device <id>  Audio monitoring device id. Default uses OBS/system default.\n"
        << "  --list-audio-monitoring-devices  Print available audio monitoring devices and exit.\n"
        << "  --match-source-size      Match OBS output size to the media source.\n"
        << "  --scene-name <name>      OBS 场景名称，默认：OLB_MAIN_SCENE\n"
        << "  --source-name <name>     OBS 媒体源名称，默认：OLB_MEDIA_SOURCE\n"
        << "  --virtualcam-name <name> 虚拟摄像头预期名称。\n"
        << "  --virtualcam-clsid <id>  虚拟摄像头 COM 模块的 CLSID。\n"
        << "  --virtualcam-module64 <p> 64 位模块路径，仅用于说明和状态展示。\n"
        << "  --virtualcam-module32 <p> 32 位模块路径，仅用于说明和状态展示。\n"
        << "  --check-virtualcam       输出虚拟摄像头注册状态后退出。\n"
        << "  --no-auto-virtualcam     只配置媒体源，不自动启动虚拟摄像头。\n"
        << "  --help                   显示本帮助信息。\n";
}

bool ParseUInt(const std::string& value, std::uint32_t* out)
{
    try {
        const auto parsed = std::stoul(value);
        *out = static_cast<std::uint32_t>(parsed);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool ParseOptions(int argc, char** argv, RuntimeOptions* options)
{
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const auto requireValue = [&](const char* name) -> std::optional<std::string> {
            if (i + 1 >= argc) {
                std::cerr << "参数缺少值：" << name << "\n";
                return std::nullopt;
            }
            return std::string(argv[++i]);
        };

        if (arg == "--help" || arg == "-h") {
            PrintHelp();
            std::exit(0);
        }

        if (arg == "--host") {
            auto value = requireValue("--host");
            if (!value) {
                return false;
            }
            options->host = *value;
        } else if (arg == "--port") {
            auto value = requireValue("--port");
            std::uint32_t port = 0;
            if (!value || !ParseUInt(*value, &port) || port > 65535) {
                std::cerr << "端口无效\n";
                return false;
            }
            options->port = static_cast<std::uint16_t>(port);
        } else if (arg == "--obs-root") {
            auto value = requireValue("--obs-root");
            if (!value) {
                return false;
            }
            options->bridgeConfig.obsRoot = *value;
        } else if (arg == "--width") {
            auto value = requireValue("--width");
            if (!value || !ParseUInt(*value, &options->bridgeConfig.width)) {
                std::cerr << "宽度无效\n";
                return false;
            }
        } else if (arg == "--height") {
            auto value = requireValue("--height");
            if (!value || !ParseUInt(*value, &options->bridgeConfig.height)) {
                std::cerr << "高度无效\n";
                return false;
            }
        } else if (arg == "--fps") {
            auto value = requireValue("--fps");
            if (!value || !ParseUInt(*value, &options->bridgeConfig.fps)) {
                std::cerr << "帧率无效\n";
                return false;
            }
        } else if (arg == "--fit-mode") {
            auto value = requireValue("--fit-mode");
            if (!value || !olb::ParseVideoFitMode(*value, &options->bridgeConfig.fitMode)) {
                std::cerr << "invalid fit mode; use contain, cover, or stretch\n";
                return false;
            }
        } else if (arg == "--audio-monitoring-mode") {
            auto value = requireValue("--audio-monitoring-mode");
            if (!value || !olb::ParseAudioMonitoringMode(*value, &options->bridgeConfig.audioMonitoringMode)) {
                std::cerr << "invalid audio monitoring mode; use none, monitor, or both\n";
                return false;
            }
        } else if (arg == "--audio-monitoring-device") {
            auto value = requireValue("--audio-monitoring-device");
            if (!value) {
                return false;
            }
            options->bridgeConfig.audioMonitoringDeviceId = *value;
        } else if (arg == "--list-audio-monitoring-devices") {
            options->listAudioMonitoringDevices = true;
        } else if (arg == "--match-source-size") {
            options->bridgeConfig.matchSourceSize = true;
        } else if (arg == "--scene-name") {
            auto value = requireValue("--scene-name");
            if (!value) {
                return false;
            }
            options->bridgeConfig.sceneName = *value;
        } else if (arg == "--source-name") {
            auto value = requireValue("--source-name");
            if (!value) {
                return false;
            }
            options->bridgeConfig.sourceName = *value;
        } else if (arg == "--virtualcam-name") {
            auto value = requireValue("--virtualcam-name");
            if (!value) {
                return false;
            }
            options->bridgeConfig.virtualCamera.name = *value;
        } else if (arg == "--virtualcam-clsid") {
            auto value = requireValue("--virtualcam-clsid");
            if (!value) {
                return false;
            }
            options->bridgeConfig.virtualCamera.clsid = *value;
        } else if (arg == "--virtualcam-module64") {
            auto value = requireValue("--virtualcam-module64");
            if (!value) {
                return false;
            }
            options->bridgeConfig.virtualCamera.module64Path = *value;
        } else if (arg == "--virtualcam-module32") {
            auto value = requireValue("--virtualcam-module32");
            if (!value) {
                return false;
            }
            options->bridgeConfig.virtualCamera.module32Path = *value;
        } else if (arg == "--check-virtualcam") {
            options->checkVirtualCamera = true;
        } else if (arg == "--no-auto-virtualcam") {
            options->bridgeConfig.autoStartVirtualCamera = false;
        } else {
            std::cerr << "未知参数：" << arg << "\n";
            return false;
        }
    }

    return true;
}

std::optional<std::size_t> FindJsonValueStart(const std::string& body, const std::string& key)
{
    const std::string quotedKey = "\"" + key + "\"";
    const auto keyPos = body.find(quotedKey);
    if (keyPos == std::string::npos) {
        return std::nullopt;
    }

    const auto colonPos = body.find(':', keyPos + quotedKey.size());
    if (colonPos == std::string::npos) {
        return std::nullopt;
    }

    std::size_t valueStart = colonPos + 1;
    while (valueStart < body.size() && std::isspace(static_cast<unsigned char>(body[valueStart]))) {
        ++valueStart;
    }

    if (valueStart >= body.size()) {
        return std::nullopt;
    }

    return valueStart;
}

std::optional<std::string> ExtractJsonStringValue(const std::string& body, const std::string& key)
{
    const auto valueStart = FindJsonValueStart(body, key);
    if (!valueStart || body[*valueStart] != '"') {
        return std::nullopt;
    }

    std::string value;
    bool escaping = false;
    for (std::size_t i = *valueStart + 1; i < body.size(); ++i) {
        const char ch = body[i];
        if (escaping) {
            switch (ch) {
            case '"':
            case '\\':
            case '/':
                value.push_back(ch);
                break;
            case 'n':
                value.push_back('\n');
                break;
            case 'r':
                value.push_back('\r');
                break;
            case 't':
                value.push_back('\t');
                break;
            default:
                value.push_back(ch);
                break;
            }
            escaping = false;
            continue;
        }

        if (ch == '\\') {
            escaping = true;
            continue;
        }

        if (ch == '"') {
            return value;
        }

        value.push_back(ch);
    }

    return std::nullopt;
}

std::string ExtractJsonString(const std::string& body, const std::string& key)
{
    return ExtractJsonStringValue(body, key).value_or(std::string{});
}

std::optional<bool> ExtractJsonBool(const std::string& body, const std::string& key)
{
    const auto valueStart = FindJsonValueStart(body, key);
    if (!valueStart) {
        return std::nullopt;
    }

    if (body.compare(*valueStart, 4, "true") == 0) {
        return true;
    }

    if (body.compare(*valueStart, 5, "false") == 0) {
        return false;
    }

    return std::nullopt;
}

std::optional<std::uint32_t> ExtractJsonUInt(const std::string& body, const std::string& key)
{
    const auto valueStart = FindJsonValueStart(body, key);
    if (!valueStart || !std::isdigit(static_cast<unsigned char>(body[*valueStart]))) {
        return std::nullopt;
    }

    std::size_t end = *valueStart;
    while (end < body.size() && std::isdigit(static_cast<unsigned char>(body[end]))) {
        ++end;
    }

    try {
        return static_cast<std::uint32_t>(std::stoul(body.substr(*valueStart, end - *valueStart)));
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::optional<bool> ExtractJsonBoolAlias(
    const std::string& body,
    const std::string& primaryKey,
    const std::string& fallbackKey)
{
    if (auto value = ExtractJsonBool(body, primaryKey)) {
        return value;
    }

    return ExtractJsonBool(body, fallbackKey);
}

std::string ExtractJsonStringAlias(
    const std::string& body,
    const std::string& primaryKey,
    const std::string& fallbackKey)
{
    std::string value = ExtractJsonString(body, primaryKey);
    if (!value.empty()) {
        return value;
    }

    return ExtractJsonString(body, fallbackKey);
}

olb::HttpResponse JsonResponse(int statusCode, const std::string& body)
{
    olb::HttpResponse response;
    response.statusCode = statusCode;
    response.body = body;
    return response;
}

olb::HttpResponse HandleRequest(const olb::HttpRequest& request, olb::MediaBridgeService* service)
{
    if (request.method == "GET" && request.path == "/health") {
        return JsonResponse(200, "{\"ok\":true}");
    }

    if (request.method == "GET" && request.path == "/status") {
        return JsonResponse(200, olb::StatusToJson(service->GetStatus()));
    }

    if (request.method == "GET" && request.path == "/api/v1/audio-monitoring-devices") {
        std::vector<olb::AudioMonitoringDeviceInfo> devices;
        std::string error;
        if (!service->ListAudioMonitoringDevices(&devices, &error)) {
            std::ostringstream body;
            body << "{\"ok\":false,\"error\":\"" << olb::JsonEscape(error) << "\"}";
            return JsonResponse(500, body.str());
        }

        const auto status = service->GetStatus();
        std::ostringstream body;
        body << "{\"ok\":true,\"current\":" << olb::AudioMonitoringDeviceToJson(
            {status.audioMonitoringDeviceName, status.audioMonitoringDeviceId})
             << ",\"devices\":" << olb::AudioMonitoringDevicesToJson(devices) << "}";
        return JsonResponse(200, body.str());
    }

    if (request.method == "POST" && request.path == "/api/v1/audio-monitoring-device") {
        const auto deviceId = ExtractJsonStringAlias(request.body, "id", "deviceId");
        if (deviceId.empty()) {
            return JsonResponse(400, "{\"ok\":false,\"error\":\"missing audio monitoring device id\"}");
        }

        const auto deviceName = ExtractJsonStringAlias(request.body, "name", "deviceName");
        olb::AudioMonitoringDeviceInfo device;
        device.id = deviceId;
        device.name = deviceName;

        std::string error;
        if (!service->SetAudioMonitoringDevice(device, &error)) {
            std::ostringstream body;
            body << "{\"ok\":false,\"error\":\"" << olb::JsonEscape(error) << "\"}";
            return JsonResponse(500, body.str());
        }

        const auto status = service->GetStatus();
        std::ostringstream body;
        body << "{\"ok\":true,\"current\":" << olb::AudioMonitoringDeviceToJson(
            {status.audioMonitoringDeviceName, status.audioMonitoringDeviceId})
             << "}";
        return JsonResponse(200, body.str());
    }

    if (request.method == "POST" && request.path == "/api/v1/start") {
        olb::StartRequest startRequest;
        startRequest.url = ExtractJsonString(request.body, "url");
        if (startRequest.url.empty()) {
            startRequest.url = ExtractJsonString(request.body, "flvUrl");
        }

        if (auto value = ExtractJsonBoolAlias(request.body, "startVirtualCamera", "start_virtual_camera")) {
            startRequest.startVirtualCamera = *value;
        }

        std::string error;
        if (!service->Start(startRequest, &error)) {
            std::ostringstream body;
            body << "{\"ok\":false,\"error\":\"" << olb::JsonEscape(error) << "\"}";
            return JsonResponse(500, body.str());
        }

        return JsonResponse(200, "{\"ok\":true}");
    }

    if (request.method == "POST" && request.path == "/api/v1/stop") {
        std::string error;
        if (!service->Stop(&error)) {
            std::ostringstream body;
            body << "{\"ok\":false,\"error\":\"" << olb::JsonEscape(error) << "\"}";
            return JsonResponse(500, body.str());
        }

        return JsonResponse(200, "{\"ok\":true}");
    }

    return JsonResponse(404, "{\"ok\":false,\"error\":\"not found\"}");
}

} // namespace

int main(int argc, char** argv)
{
    RuntimeOptions options;
    if (!ParseOptions(argc, argv, &options)) {
        PrintHelp();
        return 2;
    }

    if (options.checkVirtualCamera) {
        olb::VirtualCameraRegistrationStatus registrationStatus;
        std::string error;
        if (!olb::QueryVirtualCameraRegistration(options.bridgeConfig.virtualCamera, &registrationStatus, &error)) {
            std::cerr << error << "\n";
            return 1;
        }

        std::cout << olb::RegistrationStatusToJson(options.bridgeConfig.virtualCamera, registrationStatus) << "\n";
        return 0;
    }

    auto obsController = std::make_unique<olb::LibObsController>();

    if (options.listAudioMonitoringDevices) {
        olb::BridgeConfig listConfig = options.bridgeConfig;
        listConfig.autoStartVirtualCamera = false;
        listConfig.audioMonitoringDeviceId.clear();

        std::string initError;
        if (!obsController->Initialize(listConfig, &initError)) {
            std::cerr << initError << "\n";
            return 1;
        }

        std::vector<olb::AudioMonitoringDeviceInfo> devices;
        std::string listError;
        if (!obsController->GetAudioMonitoringDevices(&devices, &listError)) {
            std::cerr << listError << "\n";
            obsController->Shutdown();
            return 1;
        }

        std::cout << olb::AudioMonitoringDevicesToJson(devices) << "\n";
        obsController->Shutdown();
        return 0;
    }

    olb::MediaBridgeService service(std::move(obsController), options.bridgeConfig);

    if (options.bridgeConfig.autoStartVirtualCamera) {
        std::string warmupError;
        if (!service.Warmup(&warmupError)) {
            olb::Log(olb::LogLevel::Error, warmupError);
            return 1;
        }
    }

    olb::LocalApiServer server(options.host, options.port, [&service](const olb::HttpRequest& request) {
        return HandleRequest(request, &service);
    });

    std::string error;
    if (!server.Run(&error)) {
        olb::Log(olb::LogLevel::Error, error);
        return 1;
    }

    return 0;
}
