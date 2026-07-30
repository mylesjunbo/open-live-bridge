# OpenLiveBridge

OpenLiveBridge 是一个基于 `libobs` 的 Windows 直播媒体桥接程序。它通过本地控制 API 提供场景、媒体源、推流、录制和虚拟摄像头等能力，方便其他程序不直接嵌入 OBS 就能调用这些功能。

## 项目定位

推荐的产品结构如下：

```text
业务主程序，例如 pc-crawler.exe
  - 授权
  - 定时与状态机
  - 直播地址获取

OpenLiveBridge.exe
  - 本地 API
  - libobs 初始化
  - 场景与媒体源管理
  - 虚拟摄像头控制
  - 未来的推流/录制控制

OBS 运行时与插件
  - libobs
  - obs-ffmpeg
  - win-dshow 虚拟摄像头模块
```

把 OpenLiveBridge 单独做成进程，可以让业务主程序保留清晰的 IPC 边界，也能把 OBS/GPL 相关代码控制在这个开源组件里。

如果系统里已经安装了 OBS，也不会影响这个项目的正常运行，只要始终使用项目自带的 `obs-runtime`，并保持自己的虚拟摄像头 CLSID 不和别的安装复用。

## 当前状态

这个仓库目前已经有一版基础 CMake/C++ 骨架：

- 本地 HTTP 控制 API，默认监听 `127.0.0.1`。
- 媒体桥接服务层。
- OBS 控制抽象层。
- `OPENLIVEBRIDGE_WITH_LIBOBS` 开关控制的 `libobs` 后端入口。
- 没有 OBS SDK 时也能编译的 stub 后端。

当前还不是生产版。接下来要重点验证的是你打包时的 OBS 运行时目录结构、虚拟摄像头注册方式，以及 `libobs` 虚拟摄像头输出路径是否与你实际捆绑的 OBS 版本一致。

默认情况下，程序会自动向上寻找打包好的 `obs-runtime`；找不到时才需要显式传入 `--obs-root`。

## 虚拟摄像头身份

如果你需要自定义虚拟摄像头名称和设备 ID，长期方案应该是在你 fork 的 OBS `win-dshow` 虚拟摄像头模块源码里修改，而不是安装后再改注册表。

当前发行版只支持 x64，不再打包 32 位虚拟摄像头模块。

默认虚拟摄像头身份：

- 名称：`OpenLiveBridge Virtual Camera`
- CLSID：`{634ed3f8-eaa2-4e70-93f0-185337ef9b48}`

OpenLiveBridge 可以校验你期望的摄像头身份：

```powershell
.\OpenLiveBridge.exe `
  --virtualcam-name "OpenLiveBridge Virtual Camera" `
  --virtualcam-clsid "{634ed3f8-eaa2-4e70-93f0-185337ef9b48}" `
  --check-virtualcam
```

安装时使用的注册脚本：

```powershell
.\packaging\windows\register-virtual-camera.ps1 `
  -ObsRuntimeRoot "D:\path\to\obs-runtime" `
  -VirtualCameraName "OpenLiveBridge Virtual Camera" `
  -VirtualCameraClsid "{634ed3f8-eaa2-4e70-93f0-185337ef9b48}"
```

如果后续更换 FriendlyName 或 CLSID，安装、卸载和运行时校验都要使用同一组值。

完整集成方案请看 [docs/obs-virtual-camera-identity.md](docs/obs-virtual-camera-identity.md)。

## 接口草案

默认监听地址：

```text
http://127.0.0.1:27177
```

健康检查：

```http
GET /health
```

状态查询：

```http
GET /status
```

开始或更新直播源并启动虚拟摄像头：

```http
POST /api/v1/start
Content-Type: application/json

{
  "url": "https://example.com/live.flv"
}
```

当前版本的开始接口只需要 `url`，其他媒体参数已回退到程序内部默认值。

停止虚拟摄像头输出：

```http
POST /api/v1/stop
```

## 构建方式

OBS 源码基线版本：

- OBS Studio 32.1.2
- 官方 Latest 发布日期：2026-04-21
- 源码来源：https://github.com/obsproject/obs-studio/releases/tag/32.1.2

OBS 虚拟摄像头二开参数：

```powershell
.\packaging\obs\set-virtualcam-name.ps1 -ObsSourceRoot "D:\path\to\obs-studio"
cmake --preset windows-x64 -DVIRTUALCAM_GUID:STRING=634ed3f8-eaa2-4e70-93f0-185337ef9b48
cmake --build --preset windows-x64 --target obs-virtualcam-module
```

推荐的源码放置方式：

```text
D:\project\open-live-bridge
D:\project\obs-studio-openlivebridge
```

OpenLiveBridge 负责控制与编排，OBS 源码 fork 单独放在兄弟目录里，后续拉上游、切分支、编译虚拟摄像头 DLL 会更顺手。

本地 OBS 运行时已集成到：

```text
D:\project\open-live-bridge\obs-runtime
```

其中除 `obs-virtualcam-module32.dll` / `obs-virtualcam-module64.dll` 外，其余运行时文件均来自官方 OBS Studio 32.1.2 x64 发布包。

先构建不依赖 OBS SDK 的基础版本：

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

运行：

```powershell
.\build\Release\OpenLiveBridge.exe --port 27177
```

启用 `libobs` 集成时：

```powershell
cmake -S . -B build -A x64 -DOPENLIVEBRIDGE_WITH_LIBOBS=ON -DOBS_ROOT=D:\path\to\obs-build-or-install
cmake --build build --config Release
```

注意：这里的 `OBS_ROOT` 是**编译期**路径，必须能找到 `libobs\obs.h` 和对应的 `obs.lib` / `libobs.lib`。你现在分发给程序运行的 `obs-runtime` 只负责运行时加载，不够拿来编译。

当前假定的 OBS 运行时目录结构类似于便携版 OBS：

```text
obs-runtime/
  bin/64bit/
  obs-plugins/64bit/
  data/obs-plugins/
  data/obs-studio/
```

## 许可证

除非某个文件明确写了其他许可证，OpenLiveBridge 统一使用 `GPL-2.0-or-later`。见 [LICENSE](LICENSE)。

本项目会链接 `libobs`，而 OBS Studio 也是按 GPL 规则发布的。你分发本项目二进制时，建议同时提供：

- OpenLiveBridge 的完整对应源码。
- 构建说明和修改记录。
- GPL 许可证副本。
- OBS Studio 以及随包插件/库的第三方声明。
- 任何被修改过的 OBS 组件的源码或源码获取方式。

如果另一个闭源程序只是通过 HTTP、WebSocket、NamedPipe 或命令行参数等简单本地边界控制 OpenLiveBridge，设计目标是让那个闭源程序不直接链接 `libobs`，也不嵌入 OBS 代码。

这份 README 只是工程建议，不构成法律意见。

## 品牌说明

OpenLiveBridge 与 OBS Project 没有隶属或官方背书关系。不要用 OBS 的名称、Logo 或表述方式暗示官方合作。
