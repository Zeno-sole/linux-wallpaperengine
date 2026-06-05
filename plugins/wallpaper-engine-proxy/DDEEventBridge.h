#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

class DBusClient;

class DDEEventBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString lockscreenMode READ lockscreenMode WRITE setLockscreenMode NOTIFY lockscreenModeChanged)
public:
    explicit DDEEventBridge(DBusClient* client, QObject* parent = nullptr);

    void connectDDESignals();
    void connectLockScreen();
    void connectFullscreen();

    QString lockscreenMode() const { return m_lockscreenMode; }
    void setLockscreenMode(const QString& mode);
    void setFullscreenIgnoreList(const QStringList& list);

signals:
    void workspaceSwitched(int from, int to);
    void monitorAdded(const QString& name);
    void monitorRemoved(const QString& name);
    void sessionLocked();
    void sessionUnlocked();
    void fullscreenAppDetected(const QString& appName);
    void fullscreenAppClosed();
    void lockscreenModeChanged(const QString& mode);

private slots:
    void onWorkspaceSwitched(int from, int to);
    void onWorkspaceBackgroundChanged(int workspace, const QString& path);
    void checkFullscreen();

private:
    DBusClient* m_client;
    QString m_lockscreenMode = "static"; // "static" or "dynamic"
    QStringList m_fullscreenIgnoreList;
    QTimer m_fullscreenTimer;
    bool m_isLocked = false;
    bool m_fullscreenActive = false;
};
