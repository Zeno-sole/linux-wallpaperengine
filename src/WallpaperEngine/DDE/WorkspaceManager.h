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
