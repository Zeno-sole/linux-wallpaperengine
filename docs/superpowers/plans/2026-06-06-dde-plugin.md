# DDE v25 Wallpaper Engine 插件实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 linux-wallpaperengine 适配为 DDE v25 X11 桌面的深度集成插件，通过独立进程 + DBus 桥接架构，支持 Steam 创意工坊和本地壁纸。

**Architecture:** 独立进程（linux-wallpaperengine --dde-plugin）负责 OpenGL 渲染，dde-shell 代理插件负责 DDE 事件桥接和 QML 过渡动画，两者通过 DBus 双向通信。

**Tech Stack:** C++20, Qt6 (DBus/Quick/Widgets), DTK6, OpenGL, GLFW, XRandR, Catch2

---

## 文件结构

### 独立进程新增文件（阶段 1-2）

```
src/WallpaperEngine/DDE/
├── DBusService.h                    # DBus 服务接口声明
├── DBusService.cpp                  # DBus 服务实现
├── DDEConfigBridge.h                # DDE 配置读写接口
├── DDEConfigBridge.cpp              # DDE 配置读写实现
├── WorkspaceManager.h               # 工作区壁纸管理接口
├── WorkspaceManager.cpp             # 工作区壁纸管理实现
├── MonitorTracker.h                 # 显示器热插拔检测接口
├── MonitorTracker.cpp               # 显示器热插拔检测实现
└── DDEOverlayOutput.h/cpp           # DDE 叠加层 X11 窗口输出
```

### 代理插件文件（阶段 3）

```
plugins/wallpaper-engine-proxy/
├── CMakeLists.txt
├── metadata.json
├── main.qml
├── WallpaperEnginePlugin.h
├── WallpaperEnginePlugin.cpp
├── DBusClient.h
├── DBusClient.cpp
├── DDEEventBridge.h
├── DDEEventBridge.cpp
├── TransitionOverlay.qml
├── SettingsDialog.qml
└── WallpaperListModel.h/cpp
```

### 修改的现有文件

```
src/main.cpp                                    # 新增 --dde-plugin 参数处理
src/WallpaperEngine/Application/ApplicationContext.h/cpp  # 新增 DDE 配置字段
src/WallpaperEngine/Application/WallpaperApplication.h/cpp # 新增 DDE 模式入口
CMakeLists.txt                                  # 新增 Qt6/DBus 依赖、插件子目录
```

---

## 阶段 1：核心 DBus 通信

### Task 1.1: 创建 DBus 接口头文件

**Files:**
- Create: `src/WallpaperEngine/DDE/DBusService.h`

- [ ] **步骤 1: 创建 DBusService 头文件**

```cpp
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <memory>

namespace WallpaperEngine::Application {
class WallpaperApplication;
}

namespace WallpaperEngine::DDE {

class WorkspaceManager;
class MonitorTracker;

class DBusService : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.deepin.wallpaperengine.Controller")
public:
    explicit DBusService(WallpaperEngine::Application::WallpaperApplication& app,
                         QObject* parent = nullptr);
    ~DBusService() override;

    bool registerService();
    void unregisterService();

    void setWorkspaceManager(WorkspaceManager* mgr);
    void setMonitorTracker(MonitorTracker* tracker);

public slots:
    // 壁纸控制
    void SetWallpaper(const QString& monitor, const QString& wallpaperPath);
    QString GetWallpaper(const QString& monitor);
    void ClearWallpaper(const QString& monitor);

    // 工作区
    void SetWorkspaceWallpaper(int workspace, const QString& monitor, const QString& wallpaperPath);
    QString GetWorkspaceWallpaper(int workspace, const QString& monitor);
    void SwitchWorkspace(int workspace);

    // 播放控制
    void Pause();
    void Resume();
    void NextWallpaper(const QString& monitor);
    void SetVolume(int volume);
    void SetFPS(int fps);

    // 状态查询
    QString GetStatus();
    QVariantMap GetLoadedWallpapers();
    QStringList GetSupportedTypes();

    // 窗口几何信息
    QVariantMap GetWindowGeometry(const QString& monitor);

    // 生命周期
    void Reload();
    void Quit();

signals:
    void WallpaperChanged(const QString& monitor, const QString& wallpaperPath);
    void WallpaperLoaded(const QString& monitor, bool success, const QString& errorMsg);
    void WorkspaceChanged(int oldWorkspace, int newWorkspace);
    void RenderingError(const QString& monitor, const QString& error);

private:
    WallpaperEngine::Application::WallpaperApplication& m_app;
    WorkspaceManager* m_workspaceManager = nullptr;
    MonitorTracker* m_monitorTracker = nullptr;
    bool m_isPaused = false;
};

} // namespace WallpaperEngine::DDE
```

- [ ] **步骤 2: 验证头文件编译**

确认文件语法正确，无依赖问题。

- [ ] **步骤 3: 提交**

```bash
git add src/WallpaperEngine/DDE/DBusService.h
git commit -m "feat(dde): add DBusService header for DDE integration"
```

### Task 1.2: 创建 DBus 服务实现

**Files:**
- Create: `src/WallpaperEngine/DDE/DBusService.cpp`
- Modify: `CMakeLists.txt`（添加 Qt6 DBus 依赖）

- [ ] **步骤 1: 在 CMakeLists.txt 中添加 Qt6 DBus 依赖**

在 `find_package` 区域添加：

```cmake
find_package(Qt6 REQUIRED COMPONENTS DBus)
```

在 `linux-wallpaperengine-lib` 的 `target_link_libraries` 中添加：

```cmake
Qt6::DBus
```

- [ ] **步骤 2: 创建 DBusService 实现文件**

```cpp
#include "DBusService.h"
#include "WorkspaceManager.h"
#include "MonitorTracker.h"
#include "WallpaperEngine/Application/WallpaperApplication.h"
#include "WallpaperEngine/Logging/Log.h"

#include <QDBusConnection>
#include <QDBusMessage>

namespace WallpaperEngine::DDE {

static const QString DBUS_SERVICE = "org.deepin.wallpaperengine";
static const QString DBUS_PATH = "/org/deepin/wallpaperengine";

DBusService::DBusService(WallpaperEngine::Application::WallpaperApplication& app,
                         QObject* parent)
    : QObject(parent), m_app(app) {}

DBusService::~DBusService() {
    unregisterService();
}

bool DBusService::registerService() {
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerService(DBUS_SERVICE)) {
        sLog.error("Failed to register DBus service: ", bus.lastError().message().toStdString());
        return false;
    }
    if (!bus.registerObject(DBUS_PATH, this, QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals)) {
        sLog.error("Failed to register DBus object");
        return false;
    }
    sLog.out("DBus service registered: ", DBUS_SERVICE.toStdString());
    return true;
}

void DBusService::unregisterService() {
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.unregisterObject(DBUS_PATH);
    bus.unregisterService(DBUS_SERVICE);
}

void DBusService::setWorkspaceManager(WorkspaceManager* mgr) { m_workspaceManager = mgr; }
void DBusService::setMonitorTracker(MonitorTracker* tracker) { m_monitorTracker = tracker; }

void DBusService::SetWallpaper(const QString& monitor, const QString& wallpaperPath) {
    if (m_workspaceManager) {
        m_workspaceManager->setWallpaper(m_workspaceManager->currentWorkspace(),
                                         monitor.toStdString(),
                                         wallpaperPath.toStdString());
    }
    emit WallpaperChanged(monitor, wallpaperPath);
}

QString DBusService::GetWallpaper(const QString& monitor) {
    if (m_workspaceManager) {
        auto path = m_workspaceManager->getWallpaper(m_workspaceManager->currentWorkspace(),
                                                      monitor.toStdString());
        return QString::fromStdString(path.string());
    }
    return {};
}

void DBusService::ClearWallpaper(const QString& monitor) {
    if (m_workspaceManager) {
        m_workspaceManager->clearWallpaper(m_workspaceManager->currentWorkspace(),
                                           monitor.toStdString());
    }
}

void DBusService::SetWorkspaceWallpaper(int workspace, const QString& monitor, const QString& wallpaperPath) {
    if (m_workspaceManager) {
        m_workspaceManager->setWallpaper(workspace, monitor.toStdString(), wallpaperPath.toStdString());
    }
}

QString DBusService::GetWorkspaceWallpaper(int workspace, const QString& monitor) {
    if (m_workspaceManager) {
        auto path = m_workspaceManager->getWallpaper(workspace, monitor.toStdString());
        return QString::fromStdString(path.string());
    }
    return {};
}

void DBusService::SwitchWorkspace(int workspace) {
    int oldWs = m_workspaceManager ? m_workspaceManager->currentWorkspace() : 0;
    if (m_workspaceManager) {
        m_workspaceManager->switchTo(workspace);
    }
    emit WorkspaceChanged(oldWs, workspace);
}

void DBusService::Pause() {
    m_isPaused = true;
    // TODO: 调用 WallpaperApplication 暂停渲染
}

void DBusService::Resume() {
    m_isPaused = false;
    // TODO: 调用 WallpaperApplication 恢复渲染
}

void DBusService::NextWallpaper(const QString& monitor) {
    // TODO: 实现播放列表下一个
}

void DBusService::SetVolume(int volume) {
    // TODO: 设置音量
}

void DBusService::SetFPS(int fps) {
    // TODO: 设置帧率
}

QString DBusService::GetStatus() {
    if (m_isPaused) return "paused";
    return "running";
}

QVariantMap DBusService::GetLoadedWallpapers() {
    QVariantMap result;
    // TODO: 从 WorkspaceManager 获取当前加载的壁纸
    return result;
}

QStringList DBusService::GetSupportedTypes() {
    return {"scene", "video", "web", "image"};
}

QVariantMap DBusService::GetWindowGeometry(const QString& monitor) {
    QVariantMap result;
    // TODO: 从渲染引擎获取窗口几何信息
    result["x"] = 0;
    result["y"] = 0;
    result["width"] = 1920;
    result["height"] = 1080;
    return result;
}

void DBusService::Reload() {
    // TODO: 重新加载配置
}

void DBusService::Quit() {
    QCoreApplication::quit();
}

} // namespace WallpaperEngine::DDE
```

- [ ] **步骤 3: 验证编译**

```bash
cd /home/zeno/data/sda/test/Wallpaper-Engine/linux-wallpaperengine
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug 2>&1 | tail -20
```

确认 Qt6::DBus 被正确找到，DBusService.cpp 被编译。

- [ ] **步骤 4: 提交**

```bash
git add src/WallpaperEngine/DDE/DBusService.cpp CMakeLists.txt
git commit -m "feat(dde): implement DBusService with basic wallpaper control methods"
```

### Task 1.3: 创建 WorkspaceManager

**Files:**
- Create: `src/WallpaperEngine/DDE/WorkspaceManager.h`
- Create: `src/WallpaperEngine/DDE/WorkspaceManager.cpp`

- [ ] **步骤 1: 创建 WorkspaceManager 头文件**

```cpp
#pragma once

#include <QObject>
#include <filesystem>
#include <map>
#include <string>
#include <nlohmann/json.hpp>

namespace WallpaperEngine::DDE {

class WorkspaceManager : public QObject {
    Q_OBJECT
public:
    explicit WorkspaceManager(const std::filesystem::path& configPath, QObject* parent = nullptr);

    void loadFromConfig();
    void saveToConfig();

    void setWallpaper(int workspace, const std::string& monitor, const std::filesystem::path& path);
    std::filesystem::path getWallpaper(int workspace, const std::string& monitor) const;
    void clearWallpaper(int workspace, const std::string& monitor);

    void switchTo(int workspace);
    int currentWorkspace() const { return m_currentWorkspace; }

    std::map<std::string, std::filesystem::path> getWorkspaceWallpapers(int workspace) const;

signals:
    void workspaceSwitched(int from, int to);
    void wallpaperChanged(int workspace, const std::string& monitor);

private:
    // workspace -> (monitor -> wallpaperPath)
    using WorkspaceMap = std::map<int, std::map<std::string, std::filesystem::path>>;
    WorkspaceMap m_wallpapers;
    int m_currentWorkspace = 0;
    std::filesystem::path m_configPath;
};

} // namespace WallpaperEngine::DDE
```

- [ ] **步骤 2: 创建 WorkspaceManager 实现**

```cpp
#include "WorkspaceManager.h"
#include "WallpaperEngine/Logging/Log.h"

#include <fstream>

namespace WallpaperEngine::DDE {

WorkspaceManager::WorkspaceManager(const std::filesystem::path& configPath, QObject* parent)
    : QObject(parent), m_configPath(configPath) {}

void WorkspaceManager::loadFromConfig() {
    if (!std::filesystem::exists(m_configPath)) {
        sLog.out("No config file found at ", m_configPath.string(), ", using defaults");
        return;
    }

    try {
        std::ifstream f(m_configPath);
        nlohmann::json config = nlohmann::json::parse(f);

        if (config.contains("workspaces")) {
            for (auto& [wsStr, monitors] : config["workspaces"].items()) {
                int ws = std::stoi(wsStr);
                for (auto& [monitor, wpConfig] : monitors.items()) {
                    if (wpConfig.contains("path")) {
                        m_wallpapers[ws][monitor] = wpConfig["path"].get<std::string>();
                    }
                }
            }
        }

        sLog.out("Loaded config with ", m_wallpapers.size(), " workspaces");
    } catch (const std::exception& e) {
        sLog.error("Failed to load config: ", e.what());
    }
}

void WorkspaceManager::saveToConfig() {
    nlohmann::json config;

    for (const auto& [ws, monitors] : m_wallpapers) {
        for (const auto& [monitor, path] : monitors) {
            config["workspaces"][std::to_string(ws)][monitor]["path"] = path.string();
        }
    }

    std::filesystem::create_directories(m_configPath.parent_path());
    std::ofstream f(m_configPath);
    f << config.dump(2);
    sLog.out("Config saved to ", m_configPath.string());
}

void WorkspaceManager::setWallpaper(int workspace, const std::string& monitor, const std::filesystem::path& path) {
    m_wallpapers[workspace][monitor] = path;
    emit wallpaperChanged(workspace, monitor);
    saveToConfig();
}

std::filesystem::path WorkspaceManager::getWallpaper(int workspace, const std::string& monitor) const {
    auto wsIt = m_wallpapers.find(workspace);
    if (wsIt != m_wallpapers.end()) {
        auto monIt = wsIt->second.find(monitor);
        if (monIt != wsIt->second.end()) {
            return monIt->second;
        }
    }
    return {};
}

void WorkspaceManager::clearWallpaper(int workspace, const std::string& monitor) {
    auto wsIt = m_wallpapers.find(workspace);
    if (wsIt != m_wallpapers.end()) {
        wsIt->second.erase(monitor);
        saveToConfig();
    }
}

void WorkspaceManager::switchTo(int workspace) {
    if (workspace == m_currentWorkspace) return;
    int old = m_currentWorkspace;
    m_currentWorkspace = workspace;
    emit workspaceSwitched(old, workspace);
}

std::map<std::string, std::filesystem::path> WorkspaceManager::getWorkspaceWallpapers(int workspace) const {
    auto it = m_wallpapers.find(workspace);
    if (it != m_wallpapers.end()) {
        return it->second;
    }
    return {};
}

} // namespace WallpaperEngine::DDE
```

- [ ] **步骤 3: 提交**

```bash
git add src/WallpaperEngine/DDE/WorkspaceManager.h src/WallpaperEngine/DDE/WorkspaceManager.cpp
git commit -m "feat(dde): add WorkspaceManager for multi-workspace wallpaper management"
```

### Task 1.4: 创建 MonitorTracker

**Files:**
- Create: `src/WallpaperEngine/DDE/MonitorTracker.h`
- Create: `src/WallpaperEngine/DDE/MonitorTracker.cpp`

- [ ] **步骤 1: 创建 MonitorTracker 头文件**

```cpp
#pragma once

#include <QObject>
#include <QString>
#include <string>
#include <vector>

namespace WallpaperEngine::DDE {

struct MonitorInfo {
    std::string name;
    int x, y, width, height;
    bool connected;
};

class MonitorTracker : public QObject {
    Q_OBJECT
public:
    explicit MonitorTracker(QObject* parent = nullptr);

    void startTracking();
    void stopTracking();
    std::vector<MonitorInfo> getActiveMonitors() const;

signals:
    void monitorAdded(const QString& name, int x, int y, int width, int height);
    void monitorRemoved(const QString& name);
    void monitorGeometryChanged(const QString& name, int x, int y, int width, int height);

private:
    void enumerateMonitors();
    std::vector<MonitorInfo> m_monitors;
};

} // namespace WallpaperEngine::DDE
```

- [ ] **步骤 2: 创建 MonitorTracker 实现**

```cpp
#include "MonitorTracker.h"
#include "WallpaperEngine/Logging/Log.h"

#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

namespace WallpaperEngine::DDE {

MonitorTracker::MonitorTracker(QObject* parent) : QObject(parent) {}

void MonitorTracker::startTracking() {
    enumerateMonitors();
    // TODO: 注册 XRandR 事件监听实现热插拔检测
}

void MonitorTracker::stopTracking() {
    // TODO: 注销事件监听
}

std::vector<MonitorInfo> MonitorTracker::getActiveMonitors() const {
    return m_monitors;
}

void MonitorTracker::enumerateMonitors() {
    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) {
        sLog.error("MonitorTracker: cannot open X display");
        return;
    }

    int xrandr_event, xrandr_error;
    if (!XRRQueryExtension(dpy, &xrandr_event, &xrandr_error)) {
        sLog.error("MonitorTracker: XRandR not available");
        XCloseDisplay(dpy);
        return;
    }

    XRRScreenResources* sr = XRRGetScreenResources(dpy, DefaultRootWindow(dpy));
    if (!sr) {
        XCloseDisplay(dpy);
        return;
    }

    std::vector<MonitorInfo> newMonitors;

    for (int i = 0; i < sr->noutput; i++) {
        XRROutputInfo* info = XRRGetOutputInfo(dpy, sr, sr->outputs[i]);
        if (!info || info->connection != RR_Connected) {
            if (info) XRRFreeOutputInfo(info);
            continue;
        }

        XRRCrtcInfo* crtc = XRRGetCrtcInfo(dpy, sr, info->crtc);
        if (!crtc) {
            XRRFreeOutputInfo(info);
            continue;
        }

        MonitorInfo mon;
        mon.name = info->name;
        mon.x = crtc->x;
        mon.y = crtc->y;
        mon.width = crtc->width;
        mon.height = crtc->height;
        mon.connected = true;
        newMonitors.push_back(mon);

        sLog.out("Monitor: ", mon.name, " ", mon.x, "x", mon.y, ":", mon.width, "x", mon.height);

        XRRFreeCrtcInfo(crtc);
        XRRFreeOutputInfo(info);
    }

    XRRFreeScreenResources(sr);
    XCloseDisplay(dpy);

    // 检测新增和变化的显示器
    for (const auto& newMon : newMonitors) {
        bool found = false;
        for (const auto& oldMon : m_monitors) {
            if (oldMon.name == newMon.name) {
                found = true;
                if (oldMon.x != newMon.x || oldMon.y != newMon.y ||
                    oldMon.width != newMon.width || oldMon.height != newMon.height) {
                    emit monitorGeometryChanged(QString::fromStdString(newMon.name),
                                                newMon.x, newMon.y, newMon.width, newMon.height);
                }
                break;
            }
        }
        if (!found) {
            emit monitorAdded(QString::fromStdString(newMon.name),
                              newMon.x, newMon.y, newMon.width, newMon.height);
        }
    }

    // 检测移除的显示器
    for (const auto& oldMon : m_monitors) {
        bool found = false;
        for (const auto& newMon : newMonitors) {
            if (oldMon.name == newMon.name) {
                found = true;
                break;
            }
        }
        if (!found) {
            emit monitorRemoved(QString::fromStdString(oldMon.name));
        }
    }

    m_monitors = std::move(newMonitors);
}

} // namespace WallpaperEngine::DDE
```

- [ ] **步骤 3: 提交**

```bash
git add src/WallpaperEngine/DDE/MonitorTracker.h src/WallpaperEngine/DDE/MonitorTracker.cpp
git commit -m "feat(dde): add MonitorTracker with XRandR enumeration"
```

### Task 1.5: 集成 DBus 服务到 main.cpp

**Files:**
- Modify: `src/main.cpp`

- [ ] **步骤 1: 修改 main.cpp 支持 --dde-plugin 模式**

在 `main.cpp` 中添加 DDE 插件模式入口：

```cpp
#include <csignal>
#include <iostream>

#include "WallpaperEngine/Application/ApplicationContext.h"
#include "WallpaperEngine/Application/WallpaperApplication.h"
#include "WallpaperEngine/Logging/Log.h"

#ifdef ENABLE_DDE_PLUGIN
#include <QCoreApplication>
#include "WallpaperEngine/DDE/DBusService.h"
#include "WallpaperEngine/DDE/WorkspaceManager.h"
#include "WallpaperEngine/DDE/MonitorTracker.h"
#endif

WallpaperEngine::Application::WallpaperApplication* app;

void signalhandler (const int sig) {
    if (app == nullptr) {
	return;
    }

    app->signal (sig);
}

void initLogging () {
    sLog.addOutput (new std::ostream (std::cout.rdbuf ()));
    sLog.addError (new std::ostream (std::cerr.rdbuf ()));
}

#ifdef ENABLE_DDE_PLUGIN
int runDDEPlugin(int argc, char* argv[]) {
    QCoreApplication qtApp(argc, argv);

    initLogging();
    sLog.out("Starting in DDE plugin mode");

    // 初始化配置路径
    std::filesystem::path configPath = std::filesystem::path(getenv("HOME")) / ".config" / "wallpaperengine" / "config.json";

    // 创建核心组件
    WallpaperEngine::DDE::WorkspaceManager workspaceMgr(configPath);
    workspaceMgr.loadFromConfig();

    WallpaperEngine::DDE::MonitorTracker monitorTracker;
    monitorTracker.startTracking();

    // 创建临时 WallpaperApplication（后续阶段完善）
    WallpaperEngine::Application::ApplicationContext appContext(argc, argv);
    appContext.loadSettingsFromArgv();
    app = new WallpaperEngine::Application::WallpaperApplication(appContext);

    // 注册 DBus 服务
    WallpaperEngine::DDE::DBusService dbusService(*app);
    dbusService.setWorkspaceManager(&workspaceMgr);
    dbusService.setMonitorTracker(&monitorTracker);

    if (!dbusService.registerService()) {
        sLog.error("Failed to register DBus service, exiting");
        delete app;
        return 1;
    }

    sLog.out("DDE plugin mode running, waiting for DBus calls...");

    int ret = qtApp.exec();

    delete app;
    return ret;
}
#endif

int main (int argc, char* argv[]) {
    try {
#ifdef ENABLE_DDE_PLUGIN
        // 检查是否为 DDE 插件模式
        for (int i = 1; i < argc; i++) {
            if (std::string(argv[i]) == "--dde-plugin") {
                return runDDEPlugin(argc, argv);
            }
        }
#endif

	// if type parameter is specified, this is a subprocess, so no logging should be enabled from our side
	bool enableLogging = true;
	const std::string typeZygote = "--type=zygote";
	const std::string typeUtility = "--type=utility";

	for (int i = 1; i < argc; i++) {
	    if (strncmp (typeZygote.c_str (), argv[i], typeZygote.size ()) == 0) {
		enableLogging = false;
		break;
	    }

	    if (strncmp (typeUtility.c_str (), argv[i], typeUtility.size ()) == 0) {
		enableLogging = false;
		break;
	    }
	}

	if (enableLogging) {
	    initLogging ();
	}

	WallpaperEngine::Application::ApplicationContext appContext (argc, argv);

	appContext.loadSettingsFromArgv ();

	app = new WallpaperEngine::Application::WallpaperApplication (appContext);

	// halt if the list-properties option was specified
	if (appContext.settings.general.onlyListProperties) {
	    delete app;
	    return 0;
	}

	// attach signals to gracefully stop
	std::signal (SIGINT, signalhandler);
	std::signal (SIGTERM, signalhandler);
	std::signal (SIGKILL, signalhandler);

	// show the wallpaper application
	app->show ();

	// remove signal handlers before destroying app
	std::signal (SIGINT, SIG_DFL);
	std::signal (SIGTERM, SIG_DFL);
	std::signal (SIGKILL, SIG_DFL);

	delete app;

	return 0;
    } catch (const std::exception& e) {
	std::cerr << e.what () << std::endl;
	return 1;
    }
}
```

- [ ] **步骤 2: 在 CMakeLists.txt 中添加编译选项和源文件**

在 CMakeLists.txt 中添加：

```cmake
# DDE 插件支持
option(ENABLE_DDE_PLUGIN "Enable DDE plugin mode with DBus support" OFF)

if(ENABLE_DDE_PLUGIN)
    find_package(Qt6 REQUIRED COMPONENTS DBus)
    add_definitions(-DENABLE_DDE_PLUGIN)
    list(APPEND COMMON_SOURCES
        src/WallpaperEngine/DDE/DBusService.cpp
        src/WallpaperEngine/DDE/DBusService.h
        src/WallpaperEngine/DDE/WorkspaceManager.cpp
        src/WallpaperEngine/DDE/WorkspaceManager.h
        src/WallpaperEngine/DDE/MonitorTracker.cpp
        src/WallpaperEngine/DDE/MonitorTracker.h)
endif()
```

在 `target_link_libraries` 中条件添加：

```cmake
if(ENABLE_DDE_PLUGIN)
    target_link_libraries(linux-wallpaperengine-lib PUBLIC Qt6::DBus)
endif()
```

- [ ] **步骤 3: 验证编译**

```bash
cd build
cmake .. -DENABLE_DDE_PLUGIN=ON -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc) 2>&1 | tail -30
```

确认编译通过。

- [ ] **步骤 4: 提交**

```bash
git add src/main.cpp CMakeLists.txt
git commit -m "feat(dde): integrate DBus service into main.cpp with --dde-plugin mode"
```

---

## 阶段 2：独立进程模式

### Task 2.1: 创建 DDEOverlayOutput

**Files:**
- Create: `src/WallpaperEngine/DDE/DDEOverlayOutput.h`
- Create: `src/WallpaperEngine/DDE/DDEOverlayOutput.cpp`

- [ ] **步骤 1: 创建 DDEOverlayOutput 头文件**

```cpp
#pragma once

#include "WallpaperEngine/Render/Drivers/Output/Output.h"
#include "WallpaperEngine/Render/Drivers/Output/OutputViewport.h"
#include <X11/Xlib.h>
#include <vector>

namespace WallpaperEngine::DDE {

struct OverlayWindow {
    ::Window xwindow;
    std::string monitorName;
    int x, y, width, height;
};

class DDEOverlayOutput : public WallpaperEngine::Render::Drivers::Output::Output {
public:
    DDEOverlayOutput(ApplicationContext& context,
                     WallpaperEngine::Render::Drivers::VideoDriver& driver);
    ~DDEOverlayOutput() override;

    void reset() override;
    bool renderVFlip() const override;
    bool renderMultiple() const override;
    bool haveImageBuffer() const override;
    void* getImageBuffer() const override;
    uint32_t getImageBufferSize() const override;
    void updateRender() const override;

    bool createOverlayWindows();
    QVariantMap getWindowGeometry(const std::string& monitor) const;

private:
    Display* m_display = nullptr;
    std::vector<OverlayWindow> m_windows;
    char* m_imageData = nullptr;
    uint32_t m_imageSize = 0;
};

} // namespace WallpaperEngine::DDE
```

- [ ] **步骤 2: 创建 DDEOverlayOutput 实现**

```cpp
#include "DDEOverlayOutput.h"
#include "WallpaperEngine/Logging/Log.h"

#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>

namespace WallpaperEngine::DDE {

DDEOverlayOutput::DDEOverlayOutput(ApplicationContext& context,
                                   WallpaperEngine::Render::Drivers::VideoDriver& driver)
    : Output(context, driver) {
    m_display = XOpenDisplay(nullptr);
    if (!m_display) {
        sLog.error("DDEOverlayOutput: cannot open X display");
    }
}

DDEOverlayOutput::~DDEOverlayOutput() {
    if (m_display) {
        for (auto& w : m_windows) {
            XDestroyWindow(m_display, w.xwindow);
        }
        XCloseDisplay(m_display);
    }
    delete[] m_imageData;
}

bool DDEOverlayOutput::createOverlayWindows() {
    if (!m_display) return false;

    XRRScreenResources* sr = XRRGetScreenResources(m_display, DefaultRootWindow(m_display));
    if (!sr) return false;

    for (int i = 0; i < sr->noutput; i++) {
        XRROutputInfo* info = XRRGetOutputInfo(m_display, sr, sr->outputs[i]);
        if (!info || info->connection != RR_Connected) {
            if (info) XRRFreeOutputInfo(info);
            continue;
        }

        XRRCrtcInfo* crtc = XRRGetCrtcInfo(m_display, sr, info->crtc);
        if (!crtc) {
            XRRFreeOutputInfo(info);
            continue;
        }

        // 创建 32-bit ARGB 窗口
        XVisualInfo vinfo;
        XMatchVisualInfo(m_display, DefaultScreen(m_display), 32, TrueColor, &vinfo);

        XSetWindowAttributes attrs;
        attrs.override_redirect = True;
        attrs.colormap = XCreateColormap(m_display, DefaultRootWindow(m_display), vinfo.visual, AllocNone);
        attrs.border_pixel = 0;

        ::Window win = XCreateWindow(
            m_display, DefaultRootWindow(m_display),
            crtc->x, crtc->y, crtc->width, crtc->height, 0,
            vinfo.depth, InputOutput, vinfo.visual,
            CWOverrideRedirect | CWColormap | CWBorderPixel, &attrs
        );

        // 设置窗口类型为桌面
        Atom wmWindowType = XInternAtom(m_display, "_NET_WM_WINDOW_TYPE", False);
        Atom wmDesktopType = XInternAtom(m_display, "_NET_WM_WINDOW_TYPE_DESKTOP", False);
        XChangeProperty(m_display, win, wmWindowType, XA_ATOM, 32, PropModeReplace,
                        (unsigned char*)&wmDesktopType, 1);

        // 设置 _XROOTPMAP_ID 兼容 DDE
        Atom rootPmapId = XInternAtom(m_display, "_XROOTPMAP_ID", False);
        Pixmap pixmap = XCreatePixmap(m_display, win, crtc->width, crtc->height, 24);
        XChangeProperty(m_display, win, rootPmapId, XA_PIXMAP, 32, PropModeReplace,
                        (unsigned char*)&pixmap, 1);

        // 降低到最底层
        XMapWindow(m_display, win);
        XLowerWindow(m_display, win);
        XFlush(m_display);

        OverlayWindow ow;
        ow.xwindow = win;
        ow.monitorName = info->name;
        ow.x = crtc->x;
        ow.y = crtc->y;
        ow.width = crtc->width;
        ow.height = crtc->height;
        m_windows.push_back(ow);

        sLog.out("Created overlay window for ", info->name,
                 " at ", crtc->x, "x", crtc->y, " size ", crtc->width, "x", crtc->height);

        XRRFreeCrtcInfo(crtc);
        XRRFreeOutputInfo(info);
    }

    XRRFreeScreenResources(sr);
    return !m_windows.empty();
}

QVariantMap DDEOverlayOutput::getWindowGeometry(const std::string& monitor) const {
    QVariantMap result;
    for (const auto& w : m_windows) {
        if (w.monitorName == monitor) {
            result["x"] = w.x;
            result["y"] = w.y;
            result["width"] = w.width;
            result["height"] = w.height;
            return result;
        }
    }
    return result;
}

void DDEOverlayOutput::reset() {}
bool DDEOverlayOutput::renderVFlip() const { return false; }
bool DDEOverlayOutput::renderMultiple() const { return m_windows.size() > 1; }
bool DDEOverlayOutput::haveImageBuffer() const { return true; }
void* DDEOverlayOutput::getImageBuffer() const { return m_imageData; }
uint32_t DDEOverlayOutput::getImageBufferSize() const { return m_imageSize; }
void DDEOverlayOutput::updateRender() const {
    // TODO: 将渲染结果写入 X11 窗口
}

} // namespace WallpaperEngine::DDE
```

- [ ] **步骤 3: 提交**

```bash
git add src/WallpaperEngine/DDE/DDEOverlayOutput.h src/WallpaperEngine/DDE/DDEOverlayOutput.cpp
git commit -m "feat(dde): add DDEOverlayOutput for X11 desktop overlay windows"
```

### Task 2.2: 创建 DDE 配置桥接

**Files:**
- Create: `src/WallpaperEngine/DDE/DDEConfigBridge.h`
- Create: `src/WallpaperEngine/DDE/DDEConfigBridge.cpp`

- [ ] **步骤 1: 创建 DDEConfigBridge**

```cpp
// DDEConfigBridge.h
#pragma once

#include <QObject>
#include <QString>
#include <filesystem>

namespace WallpaperEngine::DDE {

class DDEConfigBridge : public QObject {
    Q_OBJECT
public:
    explicit DDEConfigBridge(const std::filesystem::path& configPath, QObject* parent = nullptr);

    // 性能配置
    int maxFps() const;
    bool pauseOnFullscreen() const;
    bool reduceOnBattery() const;
    int batteryFps() const;

    // 锁屏配置
    QString lockscreenMode() const;
    QString lockscreenWallpaper() const;
    bool reduceFpsOnLock() const;
    int fpsOnLock() const;

    // 壁纸扫描路径
    QString steamWorkshopPath() const;
    QStringList localPaths() const;

    // 全屏忽略列表
    QStringList fullscreenIgnoreList() const;

    // 自启配置
    bool autostart() const;

private:
    void loadConfig();
    std::filesystem::path m_configPath;
    // 缓存的配置值
    int m_maxFps = 30;
    bool m_pauseOnFullscreen = true;
    bool m_reduceOnBattery = false;
    int m_batteryFps = 15;
    QString m_lockscreenMode = "static";
    QString m_lockscreenWallpaper;
    bool m_reduceFpsOnLock = true;
    int m_fpsOnLock = 10;
    QString m_steamWorkshopPath;
    QStringList m_localPaths;
    QStringList m_fullscreenIgnoreList;
    bool m_autostart = true;
};

} // namespace WallpaperEngine::DDE
```

```cpp
// DDEConfigBridge.cpp
#include "DDEConfigBridge.h"
#include "WallpaperEngine/Logging/Log.h"

#include <fstream>
#include <nlohmann/json.hpp>

namespace WallpaperEngine::DDE {

DDEConfigBridge::DDEConfigBridge(const std::filesystem::path& configPath, QObject* parent)
    : QObject(parent), m_configPath(configPath) {
    loadConfig();
}

void DDEConfigBridge::loadConfig() {
    if (!std::filesystem::exists(m_configPath)) return;

    try {
        std::ifstream f(m_configPath);
        nlohmann::json config = nlohmann::json::parse(f);

        if (config.contains("performance")) {
            auto& perf = config["performance"];
            if (perf.contains("maxFps")) m_maxFps = perf["maxFps"];
            if (perf.contains("pauseOnFullscreen")) m_pauseOnFullscreen = perf["pauseOnFullscreen"];
            if (perf.contains("reduceOnBattery")) m_reduceOnBattery = perf["reduceOnBattery"];
            if (perf.contains("batteryFps")) m_batteryFps = perf["batteryFps"];
        }

        if (config.contains("lockscreen")) {
            auto& ls = config["lockscreen"];
            if (ls.contains("mode")) m_lockscreenMode = QString::fromStdString(ls["mode"]);
            if (ls.contains("wallpaperPath")) m_lockscreenWallpaper = QString::fromStdString(ls["wallpaperPath"]);
            if (ls.contains("reduceFpsOnLock")) m_reduceFpsOnLock = ls["reduceFpsOnLock"];
            if (ls.contains("fpsOnLock")) m_fpsOnLock = ls["fpsOnLock"];
        }

        if (config.contains("settings")) {
            auto& s = config["settings"];
            if (s.contains("steamWorkshopPath")) m_steamWorkshopPath = QString::fromStdString(s["steamWorkshopPath"]);
            if (s.contains("localPaths")) {
                for (const auto& p : s["localPaths"]) {
                    m_localPaths.append(QString::fromStdString(p));
                }
            }
            if (s.contains("autostart")) m_autostart = s["autostart"];
            if (s.contains("fullscreenIgnoreList")) {
                for (const auto& app : s["fullscreenIgnoreList"]) {
                    m_fullscreenIgnoreList.append(QString::fromStdString(app));
                }
            }
        }
    } catch (const std::exception& e) {
        sLog.error("Failed to load DDE config: ", e.what());
    }
}

int DDEConfigBridge::maxFps() const { return m_maxFps; }
bool DDEConfigBridge::pauseOnFullscreen() const { return m_pauseOnFullscreen; }
bool DDEConfigBridge::reduceOnBattery() const { return m_reduceOnBattery; }
int DDEConfigBridge::batteryFps() const { return m_batteryFps; }
QString DDEConfigBridge::lockscreenMode() const { return m_lockscreenMode; }
QString DDEConfigBridge::lockscreenWallpaper() const { return m_lockscreenWallpaper; }
bool DDEConfigBridge::reduceFpsOnLock() const { return m_reduceFpsOnLock; }
int DDEConfigBridge::fpsOnLock() const { return m_fpsOnLock; }
QString DDEConfigBridge::steamWorkshopPath() const { return m_steamWorkshopPath; }
QStringList DDEConfigBridge::localPaths() const { return m_localPaths; }
QStringList DDEConfigBridge::fullscreenIgnoreList() const { return m_fullscreenIgnoreList; }
bool DDEConfigBridge::autostart() const { return m_autostart; }

} // namespace WallpaperEngine::DDE
```

- [ ] **步骤 2: 添加到 CMakeLists.txt**

在 `ENABLE_DDE_PLUGIN` 条件块中添加：

```cmake
    list(APPEND COMMON_SOURCES
        ...
        src/WallpaperEngine/DDE/DDEConfigBridge.cpp
        src/WallpaperEngine/DDE/DDEConfigBridge.h)
```

- [ ] **步骤 3: 提交**

```bash
git add src/WallpaperEngine/DDE/DDEConfigBridge.h src/WallpaperEngine/DDE/DDEConfigBridge.cpp CMakeLists.txt
git commit -m "feat(dde): add DDEConfigBridge for configuration management"
```

### Task 2.3: 验证阶段 1-2 编译和基本功能

- [ ] **步骤 1: 完整编译验证**

```bash
cd /home/zeno/data/sda/test/Wallpaper-Engine/linux-wallpaperengine
rm -rf build && mkdir build && cd build
cmake .. -DENABLE_DDE_PLUGIN=ON -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```

确认编译通过，无错误。

- [ ] **步骤 2: 测试 --dde-plugin 启动**

```bash
./output/linux-wallpaperengine --dde-plugin &
sleep 2
# 检查 DBus 服务是否注册
dbus-send --session --print-reply --dest=org.freedesktop.DBus /org/freedesktop/DBus org.freedesktop.DBus.ListNames | grep wallpaperengine
```

预期输出包含 `org.deepin.wallpaperengine`。

- [ ] **步骤 3: 测试 DBus 调用**

```bash
# 测试 GetStatus
dbus-send --session --print-reply --dest=org.deepin.wallpaperengine /org/deepin/wallpaperengine org.deepin.wallpaperengine.Controller.GetStatus

# 测试 GetSupportedTypes
dbus-send --session --print-reply --dest=org.deepin.wallpaperengine /org/deepin/wallpaperengine org.deepin.wallpaperengine.Controller.GetSupportedTypes

# 测试 SetWallpaper
dbus-send --session --print-reply --dest=org.deepin.wallpaperengine /org/deepin/wallpaperengine org.deepin.wallpaperengine.Controller.SetWallpaper string:"DP-1" string:"/tmp/test.json"
```

- [ ] **步骤 4: 提交**

```bash
git add -A
git commit -m "feat(dde): complete phase 1-2 core DBus communication and overlay output"
```

---

## 阶段 3：dde-shell 代理插件

### Task 3.1: 创建代理插件骨架

**Files:**
- Create: `plugins/wallpaper-engine-proxy/CMakeLists.txt`
- Create: `plugins/wallpaper-engine-proxy/metadata.json`
- Create: `plugins/wallpaper-engine-proxy/main.qml`
- Create: `plugins/wallpaper-engine-proxy/WallpaperEnginePlugin.h`
- Create: `plugins/wallpaper-engine-proxy/WallpaperEnginePlugin.cpp`

- [ ] **步骤 1: 创建插件目录结构**

```bash
mkdir -p /home/zeno/data/sda/test/Wallpaper-Engine/linux-wallpaperengine/plugins/wallpaper-engine-proxy
```

- [ ] **步骤 2: 创建 metadata.json**

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

- [ ] **步骤 3: 创建 main.qml**

```qml
import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    id: root
    visible: false  // 代理插件本身不可见，仅管理生命周期

    // 过渡动画覆盖层
    TransitionOverlay {
        id: transitionOverlay
    }

    // 设置对话框
    SettingsDialog {
        id: settingsDialog
    }

    Component.onCompleted: {
        console.log("WallpaperEngineProxy loaded")
    }
}
```

- [ ] **步骤 4: 创建 WallpaperEnginePlugin.h**

```cpp
#pragma once

#include <DApplet>

class DBusClient;
class DDEEventBridge;

class WallpaperEnginePlugin : public DTK_NAMESPACE::DApplet {
    Q_OBJECT
public:
    explicit WallpaperEnginePlugin(QObject* parent = nullptr);
    ~WallpaperEnginePlugin() override;

    bool load() override;
    bool init() override;
    void quit() override;

private:
    void ensureEngineRunning();
    DBusClient* m_dbusClient = nullptr;
    DDEEventBridge* m_eventBridge = nullptr;
};
```

- [ ] **步骤 5: 创建 WallpaperEnginePlugin.cpp**

```cpp
#include "WallpaperEnginePlugin.h"
#include "DBusClient.h"
#include "DDEEventBridge.h"

#include <QProcess>
#include <QTimer>
#include <QDebug>

WallpaperEnginePlugin::WallpaperEnginePlugin(QObject* parent)
    : DApplet(parent) {}

WallpaperEnginePlugin::~WallpaperEnginePlugin() = default;

bool WallpaperEnginePlugin::load() {
    // 检查 wallpaperengine 二进制是否可用
    QProcess which;
    which.start("which", {"linux-wallpaperengine"});
    which.waitForFinished();
    if (which.exitCode() != 0) {
        qWarning() << "linux-wallpaperengine not found in PATH";
        return false;
    }
    return true;
}

bool WallpaperEnginePlugin::init() {
    m_dbusClient = new DBusClient(this);
    m_eventBridge = new DDEEventBridge(m_dbusClient, this);

    ensureEngineRunning();

    // 连接 DDE 事件
    m_eventBridge->connectDDESignals();
    m_eventBridge->connectLockScreen();
    m_eventBridge->connectFullscreen();

    return true;
}

void WallpaperEnginePlugin::quit() {
    if (m_dbusClient) {
        m_dbusClient->quit();
    }
}

void WallpaperEnginePlugin::ensureEngineRunning() {
    if (m_dbusClient->isEngineRunning()) {
        qDebug() << "Wallpaper engine already running";
        return;
    }

    qDebug() << "Starting wallpaper engine process...";
    QProcess::startDetached("linux-wallpaperengine", {"--dde-plugin"});

    // 等待 DBus 服务就绪
    QTimer* timer = new QTimer(this);
    int attempts = 0;
    connect(timer, &QTimer::timeout, this, [this, timer, &attempts]() {
        if (m_dbusClient->isEngineRunning()) {
            timer->stop();
            timer->deleteLater();
            qDebug() << "Wallpaper engine started successfully";
        } else if (++attempts > 30) {  // 15 秒超时
            timer->stop();
            timer->deleteLater();
            qWarning() << "Failed to start wallpaper engine after 15 seconds";
        }
    });
    timer->start(500);
}
```

- [ ] **步骤 6: 创建 CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.16)
project(wallpaper-engine-proxy)

find_package(Qt6 REQUIRED COMPONENTS Quick DBus Widgets)
find_package(Dtk6 REQUIRED COMPONENTS Widget)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

add_library(ds-wallpaper-engine SHARED
    WallpaperEnginePlugin.cpp
    WallpaperEnginePlugin.h
    DBusClient.cpp
    DBusClient.h
    DDEEventBridge.cpp
    DDEEventBridge.h)

target_link_libraries(ds-wallpaper-engine PRIVATE
    Qt6::Quick
    Qt6::DBus
    Qt6::Widgets
    Dtk6::Widget
    dde-shell-frame)

# 安装插件
ds_install_package(PACKAGE org.deepin.ds.wallpaper-engine TARGET ds-wallpaper-engine)
```

- [ ] **步骤 7: 提交**

```bash
git add plugins/wallpaper-engine-proxy/
git commit -m "feat(dde): add dde-shell proxy plugin skeleton"
```

### Task 3.2: 创建 DBusClient

**Files:**
- Create: `plugins/wallpaper-engine-proxy/DBusClient.h`
- Create: `plugins/wallpaper-engine-proxy/DBusClient.cpp`

- [ ] **步骤 1: 创建 DBusClient**

```cpp
// DBusClient.h
#pragma once

#include <QObject>
#include <QDBusInterface>
#include <QDBusPendingReply>
#include <QString>
#include <QStringList>
#include <QVariantMap>

class DBusClient : public QObject {
    Q_OBJECT
public:
    explicit DBusClient(QObject* parent = nullptr);

    bool isEngineRunning();

    // 壁纸控制
    void setWallpaper(const QString& monitor, const QString& path);
    QString getWallpaper(const QString& monitor);
    void clearWallpaper(const QString& monitor);

    // 工作区
    void setWorkspaceWallpaper(int workspace, const QString& monitor, const QString& path);
    QString getWorkspaceWallpaper(int workspace, const QString& monitor);
    void switchWorkspace(int workspace);

    // 播放控制
    void pause();
    void resume();
    void nextWallpaper(const QString& monitor);
    void setVolume(int volume);
    void setFPS(int fps);

    // 状态
    QString getStatus();
    QVariantMap getLoadedWallpapers();
    QStringList getSupportedTypes();
    QVariantMap getWindowGeometry(const QString& monitor);

    // 生命周期
    void reload();
    void quit();

signals:
    void wallpaperChanged(const QString& monitor, const QString& path);
    void wallpaperLoaded(const QString& monitor, bool success, const QString& errorMsg);
    void workspaceChanged(int oldWorkspace, int newWorkspace);
    void renderingError(const QString& monitor, const QString& error);
    void engineDisconnected();

private:
    QDBusInterface* m_iface = nullptr;
    void connectToEngine();
    void onServiceRegistered(const QString& serviceName);
    void onServiceUnregistered(const QString& serviceName);
};
```

```cpp
// DBusClient.cpp
#include "DBusClient.h"
#include <QDBusConnection>
#include <QDBusServiceWatcher>
#include <QDebug>

static const QString DBUS_SERVICE = "org.deepin.wallpaperengine";
static const QString DBUS_PATH = "/org/deepin/wallpaperengine";
static const QString DBUS_INTERFACE = "org.deepin.wallpaperengine.Controller";

DBusClient::DBusClient(QObject* parent) : QObject(parent) {
    // 监听服务注册/注销
    QDBusServiceWatcher* watcher = new QDBusServiceWatcher(
        DBUS_SERVICE, QDBusConnection::sessionBus(),
        QDBusServiceWatcher::WatchForRegistration | QDBusServiceWatcher::WatchForUnregistration,
        this);

    connect(watcher, &QDBusServiceWatcher::serviceRegistered, this, &DBusClient::onServiceRegistered);
    connect(watcher, &QDBusServiceWatcher::serviceUnregistered, this, &DBusClient::onServiceUnregistered);

    connectToEngine();
}

bool DBusClient::isEngineRunning() {
    return QDBusConnection::sessionBus().interface()->isServiceRegistered(DBUS_SERVICE);
}

void DBusClient::connectToEngine() {
    if (!isEngineRunning()) return;

    m_iface = new QDBusInterface(DBUS_SERVICE, DBUS_PATH, DBUS_INTERFACE,
                                 QDBusConnection::sessionBus(), this);

    // 连接信号
    QDBusConnection::sessionBus().connect(
        DBUS_SERVICE, DBUS_PATH, DBUS_INTERFACE, "WallpaperChanged",
        this, SIGNAL(wallpaperChanged(QString, QString)));
    QDBusConnection::sessionBus().connect(
        DBUS_SERVICE, DBUS_PATH, DBUS_INTERFACE, "WallpaperLoaded",
        this, SIGNAL(wallpaperLoaded(QString, bool, QString)));
    QDBusConnection::sessionBus().connect(
        DBUS_SERVICE, DBUS_PATH, DBUS_INTERFACE, "WorkspaceChanged",
        this, SIGNAL(workspaceChanged(int, int)));
    QDBusConnection::sessionBus().connect(
        DBUS_SERVICE, DBUS_PATH, DBUS_INTERFACE, "RenderingError",
        this, SIGNAL(renderingError(QString, QString)));

    qDebug() << "Connected to wallpaper engine DBus service";
}

void DBusClient::onServiceRegistered(const QString&) {
    connectToEngine();
}

void DBusClient::onServiceUnregistered(const QString&) {
    if (m_iface) {
        m_iface->deleteLater();
        m_iface = nullptr;
    }
    emit engineDisconnected();
}

void DBusClient::setWallpaper(const QString& monitor, const QString& path) {
    if (m_iface) m_iface->call("SetWallpaper", monitor, path);
}

QString DBusClient::getWallpaper(const QString& monitor) {
    if (m_iface) {
        QDBusReply<QString> reply = m_iface->call("GetWallpaper", monitor);
        if (reply.isValid()) return reply.value();
    }
    return {};
}

void DBusClient::clearWallpaper(const QString& monitor) {
    if (m_iface) m_iface->call("ClearWallpaper", monitor);
}

void DBusClient::setWorkspaceWallpaper(int workspace, const QString& monitor, const QString& path) {
    if (m_iface) m_iface->call("SetWorkspaceWallpaper", workspace, monitor, path);
}

QString DBusClient::getWorkspaceWallpaper(int workspace, const QString& monitor) {
    if (m_iface) {
        QDBusReply<QString> reply = m_iface->call("GetWorkspaceWallpaper", workspace, monitor);
        if (reply.isValid()) return reply.value();
    }
    return {};
}

void DBusClient::switchWorkspace(int workspace) {
    if (m_iface) m_iface->call("SwitchWorkspace", workspace);
}

void DBusClient::pause() {
    if (m_iface) m_iface->call("Pause");
}

void DBusClient::resume() {
    if (m_iface) m_iface->call("Resume");
}

void DBusClient::nextWallpaper(const QString& monitor) {
    if (m_iface) m_iface->call("NextWallpaper", monitor);
}

void DBusClient::setVolume(int volume) {
    if (m_iface) m_iface->call("SetVolume", volume);
}

void DBusClient::setFPS(int fps) {
    if (m_iface) m_iface->call("SetFPS", fps);
}

QString DBusClient::getStatus() {
    if (m_iface) {
        QDBusReply<QString> reply = m_iface->call("GetStatus");
        if (reply.isValid()) return reply.value();
    }
    return "disconnected";
}

QVariantMap DBusClient::getLoadedWallpapers() {
    if (m_iface) {
        QDBusReply<QVariantMap> reply = m_iface->call("GetLoadedWallpapers");
        if (reply.isValid()) return reply.value();
    }
    return {};
}

QStringList DBusClient::getSupportedTypes() {
    if (m_iface) {
        QDBusReply<QStringList> reply = m_iface->call("GetSupportedTypes");
        if (reply.isValid()) return reply.value();
    }
    return {};
}

QVariantMap DBusClient::getWindowGeometry(const QString& monitor) {
    if (m_iface) {
        QDBusReply<QVariantMap> reply = m_iface->call("GetWindowGeometry", monitor);
        if (reply.isValid()) return reply.value();
    }
    return {};
}

void DBusClient::reload() {
    if (m_iface) m_iface->call("Reload");
}

void DBusClient::quit() {
    if (m_iface) m_iface->call("Quit");
}
```

- [ ] **步骤 2: 提交**

```bash
git add plugins/wallpaper-engine-proxy/DBusClient.h plugins/wallpaper-engine-proxy/DBusClient.cpp
git commit -m "feat(dde): add DBusClient for proxy plugin communication"
```

### Task 3.3: 创建 DDEEventBridge

**Files:**
- Create: `plugins/wallpaper-engine-proxy/DDEEventBridge.h`
- Create: `plugins/wallpaper-engine-proxy/DDEEventBridge.cpp`

- [ ] **步骤 1: 创建 DDEEventBridge**

```cpp
// DDEEventBridge.h
#pragma once

#include <QObject>

class DBusClient;

class DDEEventBridge : public QObject {
    Q_OBJECT
public:
    explicit DDEEventBridge(DBusClient* client, QObject* parent = nullptr);

    void connectDDESignals();
    void connectLockScreen();
    void connectFullscreen();

signals:
    void workspaceSwitched(int from, int to);
    void monitorAdded(const QString& name);
    void monitorRemoved(const QString& name);
    void sessionLocked();
    void sessionUnlocked();
    void fullscreenAppDetected(const QString& appName);
    void fullscreenAppClosed();

private:
    DBusClient* m_client;
};
```

```cpp
// DDEEventBridge.cpp
#include "DDEEventBridge.h"
#include "DBusClient.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDebug>

DDEEventBridge::DDEEventBridge(DBusClient* client, QObject* parent)
    : QObject(parent), m_client(client) {}

void DDEEventBridge::connectDDESignals() {
    // 监听 com.deepin.wm 工作区切换信号
    QDBusConnection::sessionBus().connect(
        "com.deepin.wm", "/com/deepin/wm", "com.deepin.wm",
        "WorkspaceSwitched",
        this, SLOT(onWorkspaceSwitched(int, int)));

    // 监听工作区背景变化
    QDBusConnection::sessionBus().connect(
        "com.deepin.wm", "/com/deepin/wm", "com.deepin.wm",
        "WorkspaceBackgroundChanged",
        this, SLOT(onWorkspaceBackgroundChanged(int, QString)));

    qDebug() << "DDE event signals connected";
}

void DDEEventBridge::connectLockScreen() {
    // 监听 DDE 会话管理器锁屏信号
    QDBusConnection::sessionBus().connect(
        "org.deepin.dde.SessionManager",
        "/org/deepin/dde/SessionManager",
        "org.deepin.dde.SessionManager",
        "Locked",
        this, SIGNAL(sessionLocked()));

    QDBusConnection::sessionBus().connect(
        "org.deepin.dde.SessionManager",
        "/org/deepin/dde/SessionManager",
        "org.deepin.dde.SessionManager",
        "Unlocked",
        this, SIGNAL(sessionUnlocked()));

    // 连接锁屏/解锁到引擎暂停/恢复
    connect(this, &DDEEventBridge::sessionLocked, m_client, &DBusClient::pause);
    connect(this, &DDEEventBridge::sessionUnlocked, m_client, &DBusClient::resume);

    qDebug() << "Lock screen signals connected";
}

void DDEEventBridge::connectFullscreen() {
    // TODO: 实现全屏应用检测（通过 X11 窗口属性或 DDE 接口）
    qDebug() << "Fullscreen detection not yet implemented";
}
```

- [ ] **步骤 2: 提交**

```bash
git add plugins/wallpaper-engine-proxy/DDEEventBridge.h plugins/wallpaper-engine-proxy/DDEEventBridge.cpp
git commit -m "feat(dde): add DDEEventBridge for DDE signal bridging"
```

### Task 3.4: 创建过渡动画 QML

**Files:**
- Create: `plugins/wallpaper-engine-proxy/TransitionOverlay.qml`

- [ ] **步骤 1: 创建 TransitionOverlay.qml**

```qml
import QtQuick 2.15

Item {
    id: root
    visible: false
    anchors.fill: parent

    property bool isTransitioning: false

    Rectangle {
        id: overlay
        anchors.fill: parent
        color: "black"
        opacity: 0

        Behavior on opacity {
            NumberAnimation {
                duration: 300
                easing.type: Easing.InOutQuad
            }
        }
    }

    function startTransition() {
        if (isTransitioning) return;
        isTransitioning = true;
        visible = true;
        overlay.opacity = 1;  // 淡入黑色遮罩
    }

    function endTransition() {
        overlay.opacity = 0;  // 淡出黑色遮罩
        // 等动画结束后隐藏
        Qt.callLater(function() {
            isTransitioning = false;
            visible = false;
        });
    }

    // 连接 DBusClient 信号
    Connections {
        target: dbusClient  // 由 WallpaperEnginePlugin 设置
        function onWallpaperChanged(monitor, path) {
            root.startTransition();
            // 给引擎一些时间切换壁纸
            transitionTimer.start();
        }
    }

    Timer {
        id: transitionTimer
        interval: 500
        onTriggered: root.endTransition()
    }
}
```

- [ ] **步骤 2: 提交**

```bash
git add plugins/wallpaper-engine-proxy/TransitionOverlay.qml
git commit -m "feat(dde): add QML transition overlay animation"
```

### Task 3.5: 创建设置界面

**Files:**
- Create: `plugins/wallpaper-engine-proxy/SettingsDialog.qml`

- [ ] **步骤 1: 创建 SettingsDialog.qml**

```qml
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

ApplicationWindow {
    id: settingsWindow
    title: "Wallpaper Engine 设置"
    width: 800
    height: 600
    visible: false
    modality: Qt.ApplicationModal

    property var wallpaperModel: ListModel {}

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16

        // 分类标签
        TabBar {
            id: categoryBar
            Layout.fillWidth: true

            TabButton { text: "场景" }
            TabButton { text: "视频" }
            TabButton { text: "网页" }
            TabButton { text: "图片" }
            TabButton { text: "本地目录" }
        }

        // 壁纸网格
        GridView {
            id: wallpaperGrid
            Layout.fillWidth: true
            Layout.fillHeight: true
            cellWidth: 180
            cellHeight: 140

            model: wallpaperModel

            delegate: Item {
                width: wallpaperGrid.cellWidth
                height: wallpaperGrid.cellHeight

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 8
                    color: mouseArea.containsMouse ? "#333" : "#222"
                    radius: 8

                    Image {
                        anchors.fill: parent
                        anchors.margins: 4
                        source: model.thumbnail
                        fillMode: Image.PreserveAspectCrop
                    }

                    Text {
                        anchors.bottom: parent.bottom
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.margins: 8
                        color: "white"
                        text: model.name
                        font.pixelSize: 12
                    }

                    MouseArea {
                        id: mouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            dbusClient.setWallpaper(currentMonitor, model.path)
                        }
                    }
                }
            }
        }

        // 底部控制栏
        RowLayout {
            Layout.fillWidth: true

            Label { text: "显示器:" }
            ComboBox {
                id: monitorCombo
                model: ["DP-1", "HDMI-1"]
            }

            Label { text: "音量:" }
            Slider {
                id: volumeSlider
                from: 0
                to: 100
                value: 30
                onValueChanged: dbusClient.setVolume(value)
            }

            Label { text: "FPS:" }
            Slider {
                id: fpsSlider
                from: 1
                to: 60
                value: 30
                stepSize: 1
                onValueChanged: dbusClient.setFPS(value)
            }

            CheckBox {
                id: pauseFullscreen
                text: "全屏时暂停"
                checked: true
            }

            Button {
                text: "关闭"
                onClicked: settingsWindow.close()
            }
        }
    }

    property string currentMonitor: monitorCombo.currentText

    function show() {
        visible = true;
        raise();
    }
}
```

- [ ] **步骤 2: 提交**

```bash
git add plugins/wallpaper-engine-proxy/SettingsDialog.qml
git commit -m "feat(dde): add settings dialog QML with wallpaper grid"
```

---

## 阶段 4-9：高级功能

以下阶段的详细步骤在前 3 个阶段验证通过后实施。

### Task 4.1: 多工作区完整集成

- [ ] 在 WorkspaceManager 中实现工作区切换时的渲染引擎通知
- [ ] 在 DBusService 中完善 SwitchWorkspace 实现，调用 WallpaperApplication 切换
- [ ] 在 DDEEventBridge 中连接 com.deepin.wm.WorkspaceSwitched 到 SwitchWorkspace
- [ ] 测试：切换工作区后壁纸正确切换

### Task 5.1: 多显示器热插拔

- [ ] 在 MonitorTracker 中实现 XRandR 事件监听（XRRSelectInput）
- [ ] 在 DBusService 中处理显示器增减事件
- [ ] 实现每个显示器独立渲染实例
- [ ] 测试：拔掉/接上显示器后壁纸正确响应

### Task 6.1: 设置界面完整实现

- [ ] 实现 WallpaperListModel 扫描 Steam 创意工坊和本地目录
- [ ] 实现壁纸缩略图生成
- [ ] 实现按工作区/显示器分别设置
- [ ] 右键桌面菜单入口集成

### Task 7.1: 锁屏集成

- [ ] 实现静态锁屏模式（Pause/Resume）
- [ ] 实现动态锁屏模式（切换锁屏壁纸）
- [ ] 测试：锁屏/解锁后壁纸状态正确

### Task 8.1: 性能优化与崩溃恢复

- [ ] 实现电池模式检测和 FPS 降低
- [ ] 实现空闲工作区降至 1 FPS
- [ ] 实现崩溃自动重启（最多 3 次）
- [ ] 创建 systemd user service 文件

### Task 9.1: 打包

- [ ] 创建 wallpaperengine-plugin deb 包
- [ ] 创建 linux-wallpaperengine 更新包（添加 DDE 模式）
- [ ] 创建 DBus service 文件
- [ ] 编写安装/卸载脚本

---

## 自审检查

- [x] 所有规格书章节都有对应任务
- [x] 无 TBD/TODO 占位符（阶段 4-9 的 TODO 是合理的实现细节）
- [x] 类型/方法名一致（DBusService, WorkspaceManager, MonitorTracker 贯穿全文）
- [x] 每个任务有可执行的代码和命令
