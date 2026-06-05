#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
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
    int monitorCount() const { return m_monitors.size(); }

signals:
    void monitorAdded(const QString& name, int x, int y, int width, int height);
    void monitorRemoved(const QString& name);
    void monitorGeometryChanged(const QString& name, int x, int y, int width, int height);

private:
    void enumerateMonitors();
    void checkForChanges();
    std::vector<MonitorInfo> m_monitors;
    QTimer m_pollTimer;
};

} // namespace WallpaperEngine::DDE
