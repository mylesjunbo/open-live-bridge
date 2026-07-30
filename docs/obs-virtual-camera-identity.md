# OBS 虚拟摄像头身份说明

这份文档说明 OpenLiveBridge 如何接入一个自定义的 OBS 虚拟摄像头名称和设备 ID。

## 结论先说

不要把“虚拟摄像头名称”和“CLSID”当成安装后的注册表修补项来改。长期方案应该是：fork OBS 的 `win-dshow` 虚拟摄像头模块，在源码里改它的 DirectShow COM 身份，然后自己编译 64 位 DLL，在安装时完成注册。

OpenLiveBridge 这边负责的是：

- 加载包含你自定义虚拟摄像头模块的 OBS 运行时。
- 继续使用 OBS 的输出类型 `virtualcam_output`。
- 启动前检查预期的 DirectShow CLSID 和 FriendlyName。
- 通过 `/status` 暴露注册状态。

## 两个容易混淆的 ID

```text
DirectShow 虚拟摄像头 CLSID
  - Windows 里注册的 COM / 设备身份。
  - 这里才是你应该自定义的值。
  - 它决定别的 Windows 程序看到的是哪一个摄像头设备。

OBS 输出类型：virtualcam_output
  - OBS 的 win-dshow 插件注册的输出类型。
  - OpenLiveBridge 通过 obs_output_create("virtualcam_output", ...) 调用它。
  - 除非你同时改 OBS 插件和 OpenLiveBridge，否则不要改这个名字。
```

## 你的 OBS fork 里要改什么

在 OBS Studio fork 里，搜索 `win-dshow` 虚拟摄像头模块相关的这些关键词或代码点：

```text
obs-virtualcam-module
virtualcam
CLSID
FriendlyName
DllRegisterServer
DllUnregisterServer
DllInstall
VideoInputDeviceCategory
{860BB310-5D01-11d0-BD3B-00A0C911CE86}
virtualcam_output
```

当前 OBS 源码里，虚拟摄像头注册逻辑主要在：

```text
plugins/win-dshow/virtualcam-module/virtualcam-module.cpp
```

最小二开改法：

```cpp
// 注册 COM CLSID 时显示的名称
return RegServer(CLSID_OBS_VirtualVideo, L"OBS Virtual Camera", file);

// 注册到 DirectShow VideoInputDeviceCategory 时显示的名称
hr = fm->RegisterFilter(
    CLSID_OBS_VirtualVideo,
    L"OBS Virtual Camera",
    &moniker,
    &CLSID_VideoInputDeviceCategory,
    nullptr,
    &rf2);
```

把上面的 `L"OBS Virtual Camera"` 改成你的名称，例如：

```cpp
L"OpenLiveBridge Virtual Camera"
```

为了少改代码，你也可以直接运行仓库里附带的脚本，只替换虚拟摄像头名称：

```powershell
.\packaging\obs\set-virtualcam-name.ps1 -ObsSourceRoot "D:\path\to\obs-studio"
```

CLSID 不建议直接硬写在 `virtualcam-module.cpp` 里。当前 OBS 的虚拟摄像头模块已经通过 CMake 变量 `VIRTUALCAM_GUID` 生成 `CLSID_OBS_VirtualVideo`：

```text
plugins/win-dshow/virtualcam-module/CMakeLists.txt
plugins/win-dshow/virtualcam-module/virtualcam-guid.h.in
```

因此推荐构建时传入你自己的 GUID：

```powershell
$guid = "634ed3f8-eaa2-4e70-93f0-185337ef9b48"
cmake --preset windows-x64 -DVIRTUALCAM_GUID:STRING=$guid
cmake --build --preset windows-x64 --target obs-virtualcam-module
```

注意：`VIRTUALCAM_GUID` 传给 CMake 时不要带 `{}`，注册表里看到时才会带大括号。

需要修改的内容：

- 虚拟摄像头 FriendlyName，例如 `OpenLiveBridge Virtual Camera`。
- 虚拟摄像头 COM CLSID，自己生成一个新的 GUID，不要复用 OBS 默认值，也不要复用竞品值。
- 任何会被 DirectShow 客户端显示出来的模块名、产品名或元数据。

本项目当前只支持 x64，所以只需要维护 64 位虚拟摄像头模块。

如果系统里另外安装了 OBS，只要你继续使用项目自己的 `obs-runtime`，并且 CLSID 不复用官方默认值，通常不会影响 OpenLiveBridge 的运行。

补充一句：`obs-runtime` 是**运行时目录**，不要拿它去当编译 OpenLiveBridge 的 `OBS_ROOT`。编译时要指向 OBS 的源码/安装目录，那里必须能找到 `libobs\obs.h` 和对应的导入库。

OpenLiveBridge 当前默认使用：

```text
FriendlyName: OpenLiveBridge Virtual Camera
CLSID: {634ed3f8-eaa2-4e70-93f0-185337ef9b48}
CMake VIRTUALCAM_GUID: 634ed3f8-eaa2-4e70-93f0-185337ef9b48
```

生成 GUID：

```powershell
[guid]::NewGuid().ToString("B")
```

## 建议的发布目录

推荐的运行时目录结构：

```text
OpenLiveBridge/
  bin/
    OpenLiveBridge.exe
  obs-runtime/
    bin/64bit/
    obs-plugins/64bit/
    data/obs-studio/
    data/obs-plugins/win-dshow/
      obs-virtualcam-module32.dll
      obs-virtualcam-module64.dll
  licenses/
    GPL-2.0.txt
    OBS-SOURCE.txt
    THIRD_PARTY_NOTICES.md
```

虚拟摄像头 COM 模块注册路径：

```text
obs-runtime\data\obs-plugins\win-dshow\obs-virtualcam-module32.dll
obs-runtime\data\obs-plugins\win-dshow\obs-virtualcam-module64.dll
```

## 安装时注册

安装过程需要管理员权限，示例脚本如下：

```powershell
.\packaging\windows\register-virtual-camera.ps1 `
  -ObsRuntimeRoot "D:\path\to\OpenLiveBridge\obs-runtime" `
  -VirtualCameraName "OpenLiveBridge Virtual Camera" `
  -VirtualCameraClsid "{634ed3f8-eaa2-4e70-93f0-185337ef9b48}"
```

卸载时反注册：

```powershell
.\packaging\windows\register-virtual-camera.ps1 -ObsRuntimeRoot "D:\path\to\OpenLiveBridge\obs-runtime" -Unregister
```

如果你发布新版本时更换了 CLSID，先反注册旧模块，再注册新模块，否则 Windows 里可能残留旧设备条目。
如果你真的改了 FriendlyName 或 CLSID，安装和卸载时都要传同一组参数。

## 运行时校验

安装完成后，可以用 OpenLiveBridge 校验预期的虚拟摄像头身份：

```powershell
.\OpenLiveBridge.exe `
  --virtualcam-name "OpenLiveBridge Virtual Camera" `
  --virtualcam-clsid "{634ed3f8-eaa2-4e70-93f0-185337ef9b48}" `
  --check-virtualcam
```

示例输出：

```json
{
  "name": "OpenLiveBridge Virtual Camera",
  "clsid": "{634ed3f8-eaa2-4e70-93f0-185337ef9b48}",
  "checked": true,
  "registered64": true,
  "registered32": true,
  "friendlyName64": "OpenLiveBridge Virtual Camera",
  "friendlyName32": "OpenLiveBridge Virtual Camera",
  "modulePath64": "D:\\path\\obs-virtualcam-module64.dll",
  "modulePath32": "D:\\path\\obs-virtualcam-module32.dll"
}
```

带着注册校验启动：

```powershell
.\OpenLiveBridge.exe `
  --obs-root "D:\path\to\OpenLiveBridge\obs-runtime" `
  --virtualcam-name "OpenLiveBridge Virtual Camera" `
  --virtualcam-clsid "{634ed3f8-eaa2-4e70-93f0-185337ef9b48}"
```

如果提供了 `--virtualcam-clsid`，OpenLiveBridge 会在启动虚拟摄像头前检查 32 位和 64 位注册视图。

## 系统里已经装了 OBS 会怎样

一般不会有问题，前提是：

- 你始终让 OpenLiveBridge 指向项目内的 `obs-runtime`。
- 你自己的虚拟摄像头 CLSID 和官方 OBS 默认值不同。
- 你没有把两个不同版本的 OBS 虚拟摄像头模块注册到同一个 CLSID 上。

真正会互相影响的场景，通常只有两种：

1. 你手动让项目去用系统 OBS 的运行时目录。
2. 你复用了和系统 OBS 一样的 CLSID，导致注册表被后注册的 DLL 覆盖。

## 合规提醒

因为虚拟摄像头模块是从 OBS Studio 代码派生出来的，分发二进制时要公开对应修改源码、构建说明、许可证文件和修改说明。

不要复制竞品 DLL、名称、GUID、证书或他们自己的注册代码。
