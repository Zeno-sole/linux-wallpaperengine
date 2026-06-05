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
