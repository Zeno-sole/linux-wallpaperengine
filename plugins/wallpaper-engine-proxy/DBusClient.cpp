#include "DBusClient.h"
#include <QDBusConnection>
#include <QDBusServiceWatcher>
#include <QDebug>

static const QString DBUS_SERVICE = "org.deepin.wallpaperengine";
static const QString DBUS_PATH = "/org/deepin/wallpaperengine";
static const QString DBUS_INTERFACE = "org.deepin.wallpaperengine.Controller";

DBusClient::DBusClient(QObject* parent) : QObject(parent) {
    // Watch for service registration/unregistration
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

    // Connect signals
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
