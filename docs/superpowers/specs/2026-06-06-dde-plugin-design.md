# DDE v25 Wallpaper Engine 插件 - 设计规格书

## 概述

将 linux-wallpaperengine 适配到 DDE（深度桌面环境）v25 X11 会话，以插件形式深度集成，替换 DDE 原生壁纸系统，支持 Steam 创意工坊和本地壁纸。

**架构方案：** 独立进程 + DBus 桥接（方案 B）

## 架构

### 组件总览

```
dde-shell 进程
  └─ WallpaperEngineProxy（DApplet 插件）
       ├─ QML 动画层（过渡/淡入淡出）
       ├─ DDEEventBridge（DDE 信号桥接）
       ├─ DBusClient（DBus 通信客户端）
       └─ SettingsDialog（独立设置 UI）

独立进程 (linux-wallpaperengine --dde-plugin)
  ├─ DBusService（DBus 服务端）
  ├─ WorkspaceManager（工作区壁纸管理）
  ├─ MonitorTracker（显示器热插拔）
  └─ 渲染引擎（GLFW/OpenGL 渲染）
```

### 通信方式

- **DBus 双向通信：** 代理 → 引擎（设置壁纸/暂停/恢复），引擎 → 代理（壁纸变更/渲染错误）
- **X11 窗口属性：** 引擎通过 ARGB 窗口设置桌面叠加层
- **DConfig：** 共享配置（壁纸路径、工作区映射等）

## DBus 接口

### 服务定义

- **服务名：** `org.deepin.wallpaperengine`
- **对象路径：** `/org/deepin/wallpaperengine`
- **接口名：** `org.deepin.wallpaperengine.Controller`

### 方法

```
# 壁纸控制
SetWallpaper(monitor: string, wallpaperPath: string) → void
GetWallpaper(monitor: string) → string
ClearWallpaper(monitor: string) → void

# 工作区
SetWorkspaceWallpaper(workspace: int, monitor: string, wallpaperPath: string) → void
GetWorkspaceWallpaper(workspace: int, monitor: string) → string
SwitchWorkspace(workspace: int) → void

# 播放控制
Pause() → void
Resume() → void
NextWallpaper(monitor: string) → void
SetVolume(volume: int) → void
SetFPS(fps: int) → void

# 状态查询
GetStatus() → string                          # "running" | "paused" | "loading"
GetLoadedWallpapers() → dict{monitor: path}
GetSupportedTypes() → string[]                # ["scene", "video", "web", "image"]

# 生命周期
Reload() → void
Quit() → void
```

### 信号（引擎 → 代理）

```
WallpaperChanged(monitor: string, wallpaperPath: string)
WallpaperLoaded(monitor: string, success: bool, errorMsg: string)
WorkspaceChanged(oldWorkspace: int, newWorkspace: int)
RenderingError(monitor: string, error: string)
```

### DDE 信号桥接

代理插件将 DDE 信号桥接到引擎 DBus 调用：

| DDE 信号 | DBus 调用 |
|---|---|
| com.deepin.wm.WorkspaceBackgroundChanged | SwitchWorkspace() |
| com.deepin.wm.WorkspaceSwitched | SwitchWorkspace() |
| org.deepin.dde.Appearance1.Changed | 同步配置 |
| dde-shell screenAdded/Removed | 处理显示器变化 |
| dde-shell sessionLocked | Pause() 或切换锁屏壁纸 |
| dde-shell sessionUnlocked | Resume() 或恢复桌面壁纸 |

## 代理插件组件（WallpaperEngineProxy）

### 插件注册

- **插件 ID：** `org.deepin.ds.wallpaper-engine`
- **类型：** Applet
- **依赖：** dde-shell-frame, Qt6::DBus, Qt6::Quick, Dtk6::Widget

### metadata.json

```json
{
  "Plugin": {
    "Version": "1.0",
    "Id": "org.deepin.ds.wallpaper-engine",
    "Url": "main.qml",
    "ContainmentType": "Applet",
    "Category": "DDE"
  }
}
```

### 内部模块

```
WallpaperEngineProxy (DApplet)
├── main.qml                      # QML 入口
├── WallpaperEnginePlugin.cpp     # DApplet 子类，生命周期管理
├── DBusClient.cpp                # DBus 接口封装
├── DDEEventBridge.cpp            # DDE 信号桥接
├── TransitionOverlay.qml         # 壁纸切换过渡动画
├── SettingsDialog.qml            # 设置界面
└── WallpaperListModel.cpp        # 壁纸列表模型（扫描 Steam/本地目录）
```

### 核心类设计

**WallpaperEnginePlugin (DApplet)**
- `load()` — 检查 wallpaperengine 二进制是否已安装
- `init()` — 注册 DBus 监听，启动 DDEEventBridge
- `quit()` — 清理资源

**DBusClient**
- 封装所有 DBus 方法调用和信号订阅
- `isEngineRunning()` — 检测引擎进程是否存活
- `ensureEngineStarted()` — 自动拉起独立进程

**DDEEventBridge**
- `connectDDESignals()` — 订阅 com.deepin.wm / dde-shell 信号
- `connectLockScreen()` — 订阅锁屏信号
- `connectFullscreen()` — 全屏应用检测
- 发出信号：workspaceSwitched, monitorAdded, monitorRemoved, sessionLocked, sessionUnlocked, fullscreenAppDetected, fullscreenAppClosed

### 过渡动画（TransitionOverlay.qml）

壁纸切换时的 QML 覆盖层过渡动画：

```
┌──────────────────────────────┐
│  WallpaperEngineOverlay     │  ← 独立进程 X11 窗口（底层）
│  ┌────────────────────────┐ │
│  │ TransitionOverlay (QML)│ │  ← 半透明 QML 覆盖层（顶层）
│  │  Opacity: 1 → 0       │ │     淡出旧壁纸 → 淡入新壁纸
│  │  Duration: 300ms       │ │
│  └────────────────────────┘ │
└──────────────────────────────┘
```

触发时机：工作区切换、手动切换壁纸、播放列表自动轮换。

### 设置界面（SettingsDialog.qml）

壁纸选择界面，包含：
- 分类标签：场景、视频、网页、图片、本地目录
- 缩略图网格，支持预览
- 按显示器和工作区分别设置
- 音量/帧率滑块
- 选项：全屏时暂停、开机自启

数据来源：
- Steam 创意工坊：`~/.steam/steam/steamapps/workshop/content/431960/`
- 本地目录：用户配置的自定义路径
- 元数据解析：复用 linux-wallpaperengine 的 `Data/Parsers/` 代码

## 独立进程改造（linux-wallpaperengine）

### 新增模块

```
src/WallpaperEngine/DDE/
├── DBusService.h/cpp         # DBus 服务实现
├── DDEConfigBridge.h/cpp     # DDE 配置读写
├── WorkspaceManager.h/cpp    # 工作区壁纸管理
├── MonitorTracker.h/cpp      # 显示器热插拔检测
└── DDEOverlayOutput.h/cpp    # DDE 叠加层输出
```

### 启动流程

```
dde-shell 启动
  └─ WallpaperEngineProxy.load()
       ├─ 检查 linux-wallpaperengine 是否已安装
       ├─ 检查 DBus 服务 org.deepin.wallpaperengine 是否已注册
       │    ├─ 已注册 → 连接现有实例
       │    └─ 未注册 → QProcess::start("linux-wallpaperengine --dde-plugin")
       └─ 等待 DBus 服务就绪

linux-wallpaperengine --dde-plugin 启动
  └─ main()
       ├─ 解析参数，进入 DDE 插件模式
       ├─ 初始化 DBusService → 注册到 session bus
       ├─ 初始化 WorkspaceManager → 加载配置
       ├─ 初始化 MonitorTracker → 枚举显示器
       ├─ 为每个显示器创建渲染实例
       └─ 进入主循环
```

### 配置文件

**路径：** `~/.config/wallpaperengine/config.json`

```json
{
  "workspaces": {
    "0": {
      "DP-1": {
        "path": "/home/user/.steam/steam/steamapps/workshop/content/431960/123456/project.json",
        "type": "scene",
        "volume": 30,
        "fps": 30
      },
      "HDMI-1": {
        "path": "/home/user/wallpapers/custom-video.mp4",
        "type": "video",
        "volume": 0,
        "fps": 30
      }
    }
  },
  "settings": {
    "pauseOnFullscreen": true,
    "autostart": true,
    "steamWorkshopPath": "~/.steam/steam/steamapps/workshop/content/431960/",
    "localPaths": ["/home/user/wallpapers/"]
  },
  "performance": {
    "maxFps": 30,
    "pauseOnFullscreen": true,
    "reduceOnBattery": true,
    "batteryFps": 15,
    "vsync": true,
    "textureCacheSize": 256,
    "hardwareDecoding": true
  },
  "lockscreen": {
    "mode": "static",
    "wallpaperPath": "",
    "reduceFpsOnLock": true,
    "fpsOnLock": 10
  }
}
```

## X11 窗口定位

### 桌面叠加窗口

独立进程为每个显示器创建 ARGB 窗口，定位到桌面区域：

```
1. 通过 XRandR 查询显示器几何信息
2. 创建 X11 窗口，参数：
   - 尺寸 = 显示器分辨率
   - 位置 = 显示器原点 (x, y)
   - OverrideRedirect = true（绕过窗口管理器）
   - Visual = 32-bit ARGB（支持透明）
3. 设置窗口属性：
   - _NET_WM_WINDOW_TYPE = _NET_WM_WINDOW_TYPE_DESKTOP
   - _XROOTPMAP_ID = 窗口 pixmap ID（兼容 DDE）
4. 将窗口降到最底层（XLowerWindow）
5. 设置 _NET_WM_BYPASS_COMPOSITOR 提示，避免合成器开销
```

### 代理插件覆盖层协调

代理插件的 QML 过渡覆盖层需要与引擎的 X11 窗口对齐：

```
1. 引擎通过 DBus 暴露窗口几何信息：
   GetWindowGeometry(monitor: string) → {x, y, width, height}

2. 代理插件为每个显示器创建无边框透明 QML 窗口：
   - 位置与引擎的 X11 窗口匹配
   - 尺寸与引擎的 X11 窗口匹配
   - WindowType = Qt::Tool（不显示在任务栏）
   - WA_TranslucentBackground = true

3. 过渡覆盖层生命周期：
   - 开始：代理提升 QML 窗口，播放 opacity 1→0 动画
   - 过程：引擎在下方切换壁纸
   - 结束：代理降低 QML 窗口（或隐藏）
```

## 多工作区支持

### 工作区切换流程

```
用户切换工作区（Ctrl+Alt+方向键）
  │
  ▼
DDE com.deepin.wm 发出 WorkspaceSwitched 信号
  │
  ▼
DDEEventBridge 收到信号，转发给 DBusClient
  │
  ▼
DBusClient 调用 SwitchWorkspace(workspaceIndex)
  │
  ▼
DBusService 收到调用
  │
  ├─ WorkspaceManager.switchTo(workspaceIndex)
  │    ├─ 读取目标工作区的壁纸配置
  │    └─ 通知渲染引擎切换
  │
  ├─ 渲染引擎
  │    ├─ 暂停当前壁纸
  │    ├─ 加载目标工作区壁纸（如未缓存）
  │    └─ 渲染新壁纸
  │
  └─ 发出 WorkspaceChanged 信号
       │
       ▼
  代理收到信号 → 播放 QML 过渡动画
```

### 边界情况处理

| 场景 | 处理方式 |
|---|---|
| 目标工作区无壁纸配置 | 使用默认壁纸或保持当前壁纸 |
| 壁纸正在加载中 | 显示渐变色占位，加载完成后淡入 |
| 快速连续切换 | 取消前一次加载，直接跳到最新目标 |

## 多显示器支持

### 显示器生命周期

```
MonitorTracker（XRandR 监听）
  │
  ├─ 启动时枚举所有已连接显示器
  │    └─ 为每个显示器创建独立渲染实例
  │
  ├─ 显示器热插入
  │    ├─ monitorAdded 信号
  │    ├─ 查询该显示器的壁纸配置
  │    └─ 创建新渲染实例
  │
  ├─ 显示器拔出
  │    ├─ monitorRemoved 信号
  │    ├─ 暂停并销毁该显示器的渲染实例
  │    └─ 保留配置（下次插入时恢复）
  │
  └─ 显示器分辨率变化
       ├─ monitorGeometryChanged 信号
       └─ 调整渲染实例的 viewport 尺寸
```

### 渲染实例管理

```
WallpaperApplication
  ├── RenderInstance (DP-1)
  │    ├── X11 ARGB 窗口（覆盖 DP-1 区域）
  │    ├── OpenGL 上下文
  │    └── WallpaperRenderer（scene/video/web）
  │
  ├── RenderInstance (HDMI-1)
  │    ├── X11 ARGB 窗口（覆盖 HDMI-1 区域）
  │    ├── OpenGL 上下文
  │    └── WallpaperRenderer（scene/video/web）
  │
  └── 共享资源
       ├── TextureCache（共享纹理缓存）
       ├── AssetLocator（共享资源定位器）
       └── AudioContext（单实例音频）
```

## 锁屏集成

### 两种模式

```
模式 1：静态锁屏壁纸（安全模式）
  ├─ 锁屏时 Pause() 渲染引擎
  └─ 解锁后 Resume()

模式 2：动态锁屏壁纸（可选）
  ├─ 锁屏时切换到锁屏专用壁纸
  ├─ 引擎继续运行（降低 FPS/CPU）
  └─ 解锁后恢复桌面壁纸
```

### 信号流

```
dde-session-shell 锁屏
  │
  ├─ DBus 信号：org.deepin.dde.SessionManager.Locked
  │
  ▼
DDEEventBridge.sessionLocked()
  │
  ├─ DBusClient → Pause() 或 SetLockScreenWallpaper()
  │
  └─ QML TransitionOverlay → 显示锁屏过渡动画

dde-session-shell 解锁
  │
  ├─ DBus 信号：org.deepin.dde.SessionManager.Unlocked
  │
  ▼
DDEEventBridge.sessionUnlocked()
  │
  ├─ DBusClient → Resume() 或 RestoreDesktopWallpaper()
  │
  └─ QML TransitionOverlay → 淡入桌面壁纸
```

## 全屏应用检测

```
全屏应用启动（如游戏、视频播放器）
  │
  ├─ FullScreenDetector 检测到全屏窗口
  │
  ▼
DDEEventBridge.fullscreenAppDetected(appName)
  │
  ├─ DBusClient → Pause()   # 默认行为
  │
  └─ 用户可配置忽略列表（如 "mpv" 不触发暂停）

全屏应用退出
  │
  ▼
DDEEventBridge.fullscreenAppClosed()
  │
  └─ DBusClient → Resume()
```

## 壁纸内容加载

### 支持的类型

| 类型 | 渲染方式 | 来源 | 复用代码 |
|---|---|---|---|
| scene | GLSL shader 渲染 | Steam 创意工坊 project.json | `Render/Wallpapers/CScene` |
| video | MPV 播放 | 本地 .mp4/.webm | `Render/Wallpapers/CVideo` |
| web | CEF 浏览器 | HTML/JS 壁纸 | `Render/Wallpapers/CWeb` |
| image | 纹理加载 | 本地图片文件 | 新增简单实现 |

### 壁纸切换流程

```
SetWallpaper("DP-1", "/path/to/project.json")
  │
  ▼
WorkspaceManager.setWallpaper(currentWorkspace, "DP-1", path)
  │
  ▼
RenderInstance("DP-1")
  ├─ 停止当前壁纸渲染
  ├─ 保存当前状态（进度、帧位置）
  │
  ├─ 加载新壁纸
  │    ├─ 解析 project.json → 确定类型
  │    ├─ scene → 加载 materials/textures/effects
  │    ├─ video → 初始化 MPV 播放器
  │    └─ web   → 初始化 CEF 浏览器
  │
  ├─ 渲染新壁纸
  │    └─ 第一帧 ready → emit WallpaperLoaded(monitor, true, "")
  │
  └─ 通知代理插件
       └─ 代理播放 QML 过渡动画
```

## 性能优化

### 资源管理

```
多显示器共享：
├── TextureCache — 全局纹理缓存，相同壁纸复用纹理
├── AssetLocator — 共享资源定位器
└── AudioContext — 单实例音频（只有一个显示器播放声音）

空闲优化：
├── 不在当前工作区的壁纸 → 降至 1 FPS 或暂停
├── 被遮挡的显示器 → 暂停渲染
└── 锁屏时 → 暂停或降至最低帧率
```

### 性能配置

```json
{
  "performance": {
    "maxFps": 30,
    "pauseOnFullscreen": true,
    "reduceOnBattery": true,
    "batteryFps": 15,
    "batteryQuality": "low",
    "vsync": true,
    "textureCacheSize": 256,
    "hardwareDecoding": true
  }
}
```

## 开机自启与进程管理

### Systemd 服务

```ini
# /usr/lib/systemd/user/wallpaperengine.service
[Unit]
Description=Wallpaper Engine for DDE
After=dde-shell.service
PartOf=graphical-session.target

[Service]
ExecStart=/usr/bin/linux-wallpaperengine --dde-plugin
Restart=on-failure
RestartSec=3

[Install]
WantedBy=graphical-session.target
```

### 崩溃恢复

```
独立进程崩溃
  │
  ▼
DBusClient 检测到 DBus 服务断开
  │
  ├─ QML TransitionOverlay 显示 "壁纸引擎已停止" 提示
  │
  ├─ 等待 3 秒后自动重启
  │    └─ QProcess::start("linux-wallpaperengine --dde-plugin")
  │
  ├─ 重启成功 → 恢复之前的壁纸配置
  │    └─ WorkspaceManager.loadFromConfig() → 重建所有渲染实例
  │
  └─ 连续崩溃 3 次 → 停止重启，显示错误通知
```

## 打包与部署

### 两个包

**1. wallpaperengine-plugin（dde-shell 插件）**
```
/usr/lib/dde-shell/plugins/libds-wallpaper-engine.so
/usr/share/dde-shell/plugins/org.deepin.ds.wallpaper-engine/
├── metadata.json
├── main.qml
├── TransitionOverlay.qml
└── SettingsDialog.qml
/usr/lib/systemd/user/wallpaperengine.service
```

**2. linux-wallpaperengine（独立程序）**
```
/usr/bin/linux-wallpaperengine
/usr/lib/linux-wallpaperengine/
├── libcef.so
├── liblinux-wallpaper-engine-lib.so
└── ...（CEF 资源文件）
/usr/share/dbus-1/services/org.deepin.wallpaperengine.service
```

### 依赖关系

**wallpaperengine-plugin 依赖：**
- dde-shell (>= 1.99.0)
- qt6-base, qt6-declarative
- dtk6-widget
- linux-wallpaperengine (>= 匹配版本)

**linux-wallpaperengine 依赖：**
- libgl, libglew, libsdl2, libmpv
- libpulse, libavcodec, libavformat
- libfreetype, zlib, lz4
- libcef（捆绑）
- dbus-1

## 测试策略

| 测试类型 | 覆盖范围 | 方式 |
|---|---|---|
| 单元测试 | WorkspaceManager, MonitorTracker, DBusService | Catch2（已有框架） |
| 集成测试 | DBus 通信、代理 ↔ 引擎交互 | dbus-send / QDBus 脚本 |
| 手动测试 | 渲染效果、多显示器、工作区切换 | DDE 虚拟机环境 |
| 回归测试 | 现有 scene/video/web 壁纸兼容性 | 现有测试用例 |

## 实施顺序

1. **阶段 1：核心 DBus 通信** — DBusService + DBusClient + 基础 SetWallpaper/Pause/Resume
2. **阶段 2：独立进程模式** — `--dde-plugin` 参数、X11 叠加窗口、单显示器
3. **阶段 3：dde-shell 代理插件** — WallpaperEngineProxy, DDEEventBridge, 基础 QML
4. **阶段 4：多工作区** — WorkspaceManager, 工作区切换流程
5. **阶段 5：多显示器** — MonitorTracker, 每显示器渲染实例
6. **阶段 6：设置界面** — SettingsDialog.qml, 壁纸扫描器
7. **阶段 7：锁屏集成** — 锁屏/解锁信号处理
8. **阶段 8：性能与打磨** — 电池优化、崩溃恢复、systemd 服务
9. **阶段 9：打包** — deb 包、dbus 服务文件
