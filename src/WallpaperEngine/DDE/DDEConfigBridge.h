#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <filesystem>

namespace WallpaperEngine::DDE {

class DDEConfigBridge : public QObject {
    Q_OBJECT
public:
    explicit DDEConfigBridge(const std::filesystem::path& configPath, QObject* parent = nullptr);

    // Performance config
    int maxFps() const;
    bool pauseOnFullscreen() const;
    bool reduceOnBattery() const;
    int batteryFps() const;

    // Lock screen config
    QString lockscreenMode() const;
    QString lockscreenWallpaper() const;
    bool reduceFpsOnLock() const;
    int fpsOnLock() const;

    // Wallpaper scan paths
    QString steamWorkshopPath() const;
    QStringList localPaths() const;

    // Fullscreen ignore list
    QStringList fullscreenIgnoreList() const;

    // Autostart config
    bool autostart() const;

private:
    void loadConfig();
    std::filesystem::path m_configPath;
    // Cached config values
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
