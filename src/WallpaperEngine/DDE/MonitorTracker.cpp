#include "MonitorTracker.h"
#include "WallpaperEngine/Logging/Log.h"

#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

namespace WallpaperEngine::DDE {

MonitorTracker::MonitorTracker(QObject* parent) : QObject(parent) {
    connect(&m_pollTimer, &QTimer::timeout, this, &MonitorTracker::checkForChanges);
}

void MonitorTracker::startTracking() {
    enumerateMonitors();
    sLog.out("MonitorTracker: found ", m_monitors.size(), " monitor(s)");
    // Poll for changes every 2 seconds
    m_pollTimer.start(2000);
}

void MonitorTracker::stopTracking() {
    m_pollTimer.stop();
}

void MonitorTracker::checkForChanges() {
    enumerateMonitors();
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

        XRRFreeCrtcInfo(crtc);
        XRRFreeOutputInfo(info);
    }

    XRRFreeScreenResources(sr);
    XCloseDisplay(dpy);

    // Detect added and changed monitors
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

    // Detect removed monitors
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
