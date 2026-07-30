// SPDX-License-Identifier: GPL-2.0-or-later

#include "open_live_bridge/obs/libobs_controller.h"

#include "open_live_bridge/util/logger.h"

#include <filesystem>
#include <chrono>
#include <sstream>
#include <utility>
#include <thread>

#if defined(_WIN32)
#include <windows.h>
#endif

#if OPENLIVEBRIDGE_WITH_LIBOBS
#include <obs.h>
#endif

namespace olb {

struct LibObsController::Impl {
    BridgeConfig config;
    bool initialized = false;

#if OPENLIVEBRIDGE_WITH_LIBOBS
    obs_output_t* virtualCameraOutput = nullptr;
#endif
};

namespace {

std::string NormalizePath(const std::string& value)
{
    if (value.empty()) {
        return {};
    }

    return std::filesystem::path(value).make_preferred().string();
}

#if defined(_WIN32)
std::string GetExecutableDirectory()
{
    wchar_t buffer[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return {};
    }

    return std::filesystem::path(std::wstring(buffer, length)).parent_path().string();
}
#endif

#if OPENLIVEBRIDGE_WITH_LIBOBS

std::string JoinPath(const std::string& left, const std::string& right)
{
    return (std::filesystem::path(left) / right).make_preferred().string();
}

std::string JoinObsPath(const std::string& left, const std::string& right)
{
    return (std::filesystem::path(left) / right).generic_string();
}

std::string EnsureTrailingSlash(std::string value)
{
    if (!value.empty() && value.back() != '/' && value.back() != '\\') {
        value.push_back('/');
    }
    return value;
}

std::string ResolveDefaultObsRoot()
{
#if defined(_WIN32)
    const auto executableDirectory = GetExecutableDirectory();
    if (executableDirectory.empty()) {
        return {};
    }

    const auto candidate = JoinPath(executableDirectory, "obs-runtime");
    if (std::filesystem::exists(candidate)) {
        return candidate;
    }

    const auto parentPath = std::filesystem::path(executableDirectory).parent_path();
    const auto parentCandidate = (parentPath / "obs-runtime").make_preferred().string();
    if (std::filesystem::exists(parentCandidate)) {
        return parentCandidate;
    }

    const auto grandParentCandidate =
        (parentPath.parent_path() / "obs-runtime").make_preferred().string();
    if (std::filesystem::exists(grandParentCandidate)) {
        return grandParentCandidate;
    }
#endif

    return {};
}

bool ValidateObsRoot(const std::string& obsRoot, std::string* error)
{
    if (!std::filesystem::exists(obsRoot)) {
        if (error) {
            *error = "OBS runtime root does not exist: " + obsRoot;
        }
        return false;
    }

    const auto pluginBin = JoinPath(obsRoot, "obs-plugins/64bit");
    const auto pluginData = JoinPath(obsRoot, "data/obs-plugins");
    const auto libObsData = JoinPath(obsRoot, "data/libobs");
    const auto studioData = JoinPath(obsRoot, "data/obs-studio");

    if (!std::filesystem::exists(pluginBin) ||
        !std::filesystem::exists(pluginData) ||
        !std::filesystem::exists(libObsData) ||
        !std::filesystem::exists(studioData)) {
        if (error) {
            *error = "OBS runtime root is incomplete: " + obsRoot;
        }
        return false;
    }

    return true;
}

std::string ModuleLoadCodeToString(int code)
{
    switch (code) {
    case MODULE_SUCCESS:
        return "success";
    case MODULE_ERROR:
        return "generic error";
    case MODULE_FAILED_TO_OPEN:
        return "failed to open";
    case MODULE_MISSING_EXPORTS:
        return "missing exports";
    case MODULE_INCOMPATIBLE_VER:
        return "incompatible libobs version";
    case MODULE_HARDCODED_SKIP:
        return "hardcoded skip";
    default:
        return "unknown error " + std::to_string(code);
    }
}

bool LoadObsModule(const std::string& obsRoot, const char* moduleName, std::string* error)
{
    const auto binaryPath = JoinObsPath(obsRoot, std::string("obs-plugins/64bit/") + moduleName + ".dll");
    const auto dataPath = JoinObsPath(obsRoot, std::string("data/obs-plugins/") + moduleName);

    if (!std::filesystem::exists(binaryPath)) {
        if (error) {
            *error = "OBS module was not found: " + binaryPath;
        }
        return false;
    }

    obs_module_t* module = nullptr;
    const int result = obs_open_module(&module, binaryPath.c_str(), dataPath.c_str());
    if (result != MODULE_SUCCESS) {
        if (error) {
            *error = "obs_open_module failed for " + std::string(moduleName) + ": " + ModuleLoadCodeToString(result);
        }
        return false;
    }

    if (!obs_init_module(module)) {
        if (error) {
            *error = "obs_init_module failed for " + std::string(moduleName);
        }
        return false;
    }

    Log(LogLevel::Info, "loaded OBS module: " + std::string(moduleName));
    return true;
}

bool LoadRequiredObsModules(const std::string& obsRoot, std::string* error)
{
    if (!LoadObsModule(obsRoot, "obs-ffmpeg", error)) {
        return false;
    }

    if (!LoadObsModule(obsRoot, "win-dshow", error)) {
        return false;
    }

    obs_post_load_modules();

    if (obs_source_load_state("ffmpeg_source") != OBS_MODULE_ENABLED) {
        if (error) {
            *error = "ffmpeg_source is unavailable after loading obs-ffmpeg.";
        }
        return false;
    }

    if (obs_output_load_state("virtualcam_output") != OBS_MODULE_ENABLED) {
        if (error) {
            *error = "virtualcam_output is unavailable after loading win-dshow. Check virtual camera registration and win-dshow build.";
        }
        return false;
    }

    return true;
}

bool ResetObsVideo(const BridgeConfig& config, std::string* error)
{
    obs_video_info videoInfo{};
    videoInfo.adapter = 0;
    videoInfo.graphics_module = "libobs-d3d11";
    videoInfo.fps_num = config.fps;
    videoInfo.fps_den = 1;
    videoInfo.base_width = config.width;
    videoInfo.base_height = config.height;
    videoInfo.output_width = config.width;
    videoInfo.output_height = config.height;
    videoInfo.output_format = VIDEO_FORMAT_NV12;
    videoInfo.colorspace = VIDEO_CS_709;
    videoInfo.range = VIDEO_RANGE_PARTIAL;
    videoInfo.gpu_conversion = true;
    videoInfo.scale_type = OBS_SCALE_BICUBIC;

    const auto result = obs_reset_video(&videoInfo);
    if (result != OBS_VIDEO_SUCCESS) {
        if (error) {
            *error = "obs_reset_video failed with code " + std::to_string(static_cast<int>(result));
        }
        return false;
    }

    return true;
}

bool ResetObsAudio(std::string* error)
{
    obs_audio_info audioInfo{};
    audioInfo.samples_per_sec = 48000;
    audioInfo.speakers = SPEAKERS_STEREO;

    if (!obs_reset_audio(&audioInfo)) {
        if (error) {
            *error = "obs_reset_audio failed";
        }
        return false;
    }

    return true;
}

std::string ObsMediaStateToString(enum obs_media_state state)
{
    switch (state) {
    case OBS_MEDIA_STATE_NONE:
        return "none";
    case OBS_MEDIA_STATE_PLAYING:
        return "playing";
    case OBS_MEDIA_STATE_OPENING:
        return "opening";
    case OBS_MEDIA_STATE_BUFFERING:
        return "buffering";
    case OBS_MEDIA_STATE_PAUSED:
        return "paused";
    case OBS_MEDIA_STATE_STOPPED:
        return "stopped";
    case OBS_MEDIA_STATE_ENDED:
        return "ended";
    case OBS_MEDIA_STATE_ERROR:
        return "error";
    }

    return "unknown";
}

bool WaitForSourceDimensions(obs_source_t* source, uint32_t* width, uint32_t* height)
{
    if (!source || !width || !height) {
        return false;
    }

    for (int i = 0; i < 100; ++i) {
        const uint32_t sourceWidth = obs_source_get_width(source);
        const uint32_t sourceHeight = obs_source_get_height(source);
        if (sourceWidth > 0 && sourceHeight > 0) {
            *width = sourceWidth;
            *height = sourceHeight;
            return true;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return false;
}

enum obs_bounds_type ToObsBoundsType(VideoFitMode fitMode)
{
    switch (fitMode) {
    case VideoFitMode::Contain:
        return OBS_BOUNDS_SCALE_INNER;
    case VideoFitMode::Cover:
        return OBS_BOUNDS_SCALE_OUTER;
    case VideoFitMode::Stretch:
        return OBS_BOUNDS_STRETCH;
    }

    return OBS_BOUNDS_SCALE_INNER;
}

void ApplySceneItemFit(obs_sceneitem_t* sceneItem, const BridgeConfig& config)
{
    if (!sceneItem) {
        return;
    }

    obs_transform_info itemInfo{};
    vec2_set(&itemInfo.pos, 0.0f, 0.0f);
    vec2_set(&itemInfo.scale, 1.0f, 1.0f);
    vec2_set(&itemInfo.bounds, static_cast<float>(config.width), static_cast<float>(config.height));
    itemInfo.rot = 0.0f;
    itemInfo.alignment = OBS_ALIGN_LEFT | OBS_ALIGN_TOP;
    itemInfo.bounds_type = ToObsBoundsType(config.fitMode);
    itemInfo.bounds_alignment = OBS_ALIGN_CENTER;
    itemInfo.crop_to_bounds = config.fitMode == VideoFitMode::Cover;
    obs_sceneitem_set_info2(sceneItem, &itemInfo);
}

bool MatchVideoSizeToSource(uint32_t sourceWidth, uint32_t sourceHeight, BridgeConfig* config, std::string* error)
{
    if (!config || !config->matchSourceSize || sourceWidth == 0 || sourceHeight == 0) {
        return true;
    }

    const uint32_t matchedWidth = sourceWidth & 0xFFFFFFFC;
    const uint32_t matchedHeight = sourceHeight & 0xFFFFFFFE;
    if (matchedWidth == 0 || matchedHeight == 0) {
        Log(LogLevel::Warning, "media source dimensions are too small; keeping configured output size");
        return true;
    }

    if (matchedWidth == config->width && matchedHeight == config->height) {
        return true;
    }

    config->width = matchedWidth;
    config->height = matchedHeight;
    Log(LogLevel::Info, "using media source size: " + std::to_string(config->width) + "x" +
                            std::to_string(config->height));

    return ResetObsVideo(*config, error);
}

#endif

} // namespace

LibObsController::LibObsController()
    : impl_(std::make_unique<Impl>())
{
}

LibObsController::~LibObsController()
{
    Shutdown();
}

bool LibObsController::Initialize(const BridgeConfig& config, std::string* error)
{
    impl_->config = config;
    impl_->config.obsRoot = NormalizePath(config.obsRoot);

#if !OPENLIVEBRIDGE_WITH_LIBOBS
    if (error) {
        *error = "OpenLiveBridge was built without libobs. Reconfigure with -DOPENLIVEBRIDGE_WITH_LIBOBS=ON.";
    }
    return false;
#else
    if (impl_->initialized) {
        return true;
    }

    if (impl_->config.obsRoot.empty()) {
        impl_->config.obsRoot = ResolveDefaultObsRoot();
    }

    if (impl_->config.obsRoot.empty()) {
        if (error) {
            *error = "OBS runtime root was not found. Pass --obs-root or place obs-runtime next to the executable.";
        }
        return false;
    }

    if (!ValidateObsRoot(impl_->config.obsRoot, error)) {
        return false;
    }

    if (!obs_startup("en-US", nullptr, nullptr)) {
        if (error) {
            *error = "obs_startup failed";
        }
        return false;
    }

    if (!impl_->config.obsRoot.empty()) {
        Log(LogLevel::Info, "using OBS runtime root: " + impl_->config.obsRoot);
        const auto pluginBin = JoinPath(impl_->config.obsRoot, "obs-plugins/64bit");
        const auto pluginData = JoinPath(impl_->config.obsRoot, "data/obs-plugins/%module%");
        const auto libObsData = EnsureTrailingSlash(JoinObsPath(impl_->config.obsRoot, "data/libobs"));
        const auto studioData = EnsureTrailingSlash(JoinObsPath(impl_->config.obsRoot, "data/obs-studio"));

        obs_add_module_path(pluginBin.c_str(), pluginData.c_str());
        obs_add_data_path(libObsData.c_str());
        obs_add_data_path(studioData.c_str());
    }

    if (!LoadRequiredObsModules(impl_->config.obsRoot, error)) {
        obs_shutdown();
        return false;
    }

    if (!ResetObsVideo(impl_->config, error)) {
        obs_shutdown();
        return false;
    }

    if (!ResetObsAudio(error)) {
        obs_shutdown();
        return false;
    }

    impl_->initialized = true;
    Log(LogLevel::Info, "libobs initialized");
    return true;
#endif
}

bool LibObsController::SetMediaSource(const std::string& url, std::string* error)
{
#if !OPENLIVEBRIDGE_WITH_LIBOBS
    if (error) {
        *error = "libobs backend is not enabled";
    }
    return false;
#else
    if (!impl_->initialized) {
        if (error) {
            *error = "libobs is not initialized";
        }
        return false;
    }

    obs_data_t* settings = obs_data_create();
    obs_data_set_bool(settings, "is_local_file", false);
    obs_data_set_string(settings, "input", url.c_str());
    obs_data_set_bool(settings, "clear_on_media_end", true);
    obs_data_set_bool(settings, "restart_on_activate", true);
    obs_data_set_bool(settings, "linear_alpha", false);
    obs_data_set_int(settings, "reconnect_delay_sec", 10);
    obs_data_set_int(settings, "buffering_mb", 2);
    obs_data_set_int(settings, "speed_percent", 100);
    obs_data_set_bool(settings, "log_changes", true);
    obs_data_set_bool(settings, "close_when_inactive", false);
    obs_data_set_bool(settings, "hw_decode", true);
    obs_data_set_bool(settings, "full_decode", false);
    obs_data_set_bool(settings, "seekable", false);
    obs_data_set_bool(settings, "is_stinger", false);
    obs_data_set_bool(settings, "is_track_matte", false);

    obs_source_t* mediaSource = obs_get_source_by_name(impl_->config.sourceName.c_str());
    if (mediaSource) {
        obs_source_update(mediaSource, settings);
    } else {
        mediaSource = obs_source_create("ffmpeg_source", impl_->config.sourceName.c_str(), settings, nullptr);
    }

    obs_data_release(settings);

    if (!mediaSource) {
        if (error) {
            *error = "failed to create ffmpeg_source. Check that obs-ffmpeg is loaded.";
        }
        return false;
    }

    obs_source_t* sceneSource = obs_get_source_by_name(impl_->config.sceneName.c_str());
    obs_scene_t* scene = nullptr;
    bool createdScene = false;

    if (sceneSource) {
        scene = obs_scene_from_source(sceneSource);
    } else {
        scene = obs_scene_create(impl_->config.sceneName.c_str());
        createdScene = scene != nullptr;
        if (scene) {
            obs_source_t* newSceneSource = obs_scene_get_source(scene);
            obs_set_output_source(0, newSceneSource);
        }
    }

    if (!scene) {
        obs_source_release(mediaSource);
        if (sceneSource) {
            obs_source_release(sceneSource);
        }
        if (error) {
            *error = "failed to create obs scene";
        }
        return false;
    }

    if (!obs_scene_find_source(scene, impl_->config.sourceName.c_str())) {
        obs_scene_add(scene, mediaSource);
    }

    obs_sceneitem_t* sceneItem = obs_scene_find_source(scene, impl_->config.sourceName.c_str());
    obs_set_output_source(0, obs_scene_get_source(scene));
    obs_source_media_restart(mediaSource);

    uint32_t sourceWidth = 0;
    uint32_t sourceHeight = 0;
    if (!WaitForSourceDimensions(mediaSource, &sourceWidth, &sourceHeight)) {
        Log(LogLevel::Warning, "media source dimensions are not available yet; keeping configured output size");
    }

    if (!MatchVideoSizeToSource(sourceWidth, sourceHeight, &impl_->config, error)) {
        obs_source_release(mediaSource);
        if (sceneSource) {
            obs_source_release(sceneSource);
        }
        if (createdScene) {
            obs_scene_release(scene);
        }
        return false;
    }

    obs_set_output_source(0, obs_scene_get_source(scene));
    ApplySceneItemFit(sceneItem, impl_->config);
    obs_source_media_restart(mediaSource);

    obs_source_release(mediaSource);
    if (sceneSource) {
        obs_source_release(sceneSource);
    }
    if (createdScene) {
        obs_scene_release(scene);
    }

    Log(LogLevel::Info, "media source configured");
    return true;
#endif
}

bool LibObsController::RestartMediaSource(std::string* error)
{
#if !OPENLIVEBRIDGE_WITH_LIBOBS
    if (error) {
        *error = "libobs backend is not enabled";
    }
    return false;
#else
    if (!impl_->initialized) {
        if (error) {
            *error = "libobs is not initialized";
        }
        return false;
    }

    obs_source_t* mediaSource = obs_get_source_by_name(impl_->config.sourceName.c_str());
    if (!mediaSource) {
        if (error) {
            *error = "media source not found: " + impl_->config.sourceName;
        }
        return false;
    }

    obs_source_media_restart(mediaSource);
    obs_source_release(mediaSource);

    Log(LogLevel::Info, "media source restarted");
    return true;
#endif
}

MediaSourceRuntimeStatus LibObsController::GetMediaSourceStatus() const
{
    MediaSourceRuntimeStatus status;
    status.name = impl_->config.sourceName;

#if !OPENLIVEBRIDGE_WITH_LIBOBS
    status.state = "libobs_disabled";
    return status;
#else
    if (!impl_->initialized) {
        status.state = "not_initialized";
        return status;
    }

    obs_source_t* source = obs_get_source_by_name(impl_->config.sourceName.c_str());
    if (!source) {
        status.state = "missing";
        return status;
    }

    status.exists = true;
    status.active = obs_source_active(source);
    status.width = obs_source_get_width(source);
    status.height = obs_source_get_height(source);
    status.state = ObsMediaStateToString(obs_source_media_get_state(source));

    obs_source_release(source);
    return status;
#endif
}

bool LibObsController::StartVirtualCamera(std::string* error)
{
#if !OPENLIVEBRIDGE_WITH_LIBOBS
    if (error) {
        *error = "libobs backend is not enabled";
    }
    return false;
#else
    if (!impl_->initialized) {
        if (error) {
            *error = "libobs is not initialized";
        }
        return false;
    }

    if (!impl_->virtualCameraOutput) {
        obs_data_t* settings = obs_data_create();
        impl_->virtualCameraOutput = obs_output_create(
            "virtualcam_output",
            impl_->config.virtualCamera.name.c_str(),
            settings,
            nullptr);
        obs_data_release(settings);
    }

    if (!impl_->virtualCameraOutput) {
        if (error) {
            *error = "failed to create virtualcam_output. Check that win-dshow is loaded and registered.";
        }
        return false;
    }

    if (obs_output_active(impl_->virtualCameraOutput)) {
        return true;
    }

    obs_output_set_media(impl_->virtualCameraOutput, obs_get_video(), obs_get_audio());

    if (!obs_output_start(impl_->virtualCameraOutput)) {
        if (error) {
            *error = "obs_output_start failed for virtual camera";
        }
        return false;
    }

    Log(LogLevel::Info, "virtual camera started");
    return true;
#endif
}

bool LibObsController::StopVirtualCamera(std::string* error)
{
#if !OPENLIVEBRIDGE_WITH_LIBOBS
    if (error) {
        *error = "libobs backend is not enabled";
    }
    return false;
#else
    if (!impl_->virtualCameraOutput) {
        return true;
    }

    if (obs_output_active(impl_->virtualCameraOutput)) {
        obs_output_stop(impl_->virtualCameraOutput);
    }

    Log(LogLevel::Info, "virtual camera stopped");
    return true;
#endif
}

void LibObsController::Shutdown()
{
#if OPENLIVEBRIDGE_WITH_LIBOBS
    if (impl_->virtualCameraOutput) {
        if (obs_output_active(impl_->virtualCameraOutput)) {
            obs_output_stop(impl_->virtualCameraOutput);
        }
        obs_output_release(impl_->virtualCameraOutput);
        impl_->virtualCameraOutput = nullptr;
    }

    if (impl_->initialized) {
        obs_shutdown();
        impl_->initialized = false;
    }
#endif
}

} // namespace olb
