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
