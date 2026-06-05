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

        // Create 32-bit ARGB window
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

        // Set window type to desktop
        Atom wmWindowType = XInternAtom(m_display, "_NET_WM_WINDOW_TYPE", False);
        Atom wmDesktopType = XInternAtom(m_display, "_NET_WM_WINDOW_TYPE_DESKTOP", False);
        XChangeProperty(m_display, win, wmWindowType, XA_ATOM, 32, PropModeReplace,
                        (unsigned char*)&wmDesktopType, 1);

        // Set _XROOTPMAP_ID for DDE compatibility
        Atom rootPmapId = XInternAtom(m_display, "_XROOTPMAP_ID", False);
        Pixmap pixmap = XCreatePixmap(m_display, win, crtc->width, crtc->height, 24);
        XChangeProperty(m_display, win, rootPmapId, XA_PIXMAP, 32, PropModeReplace,
                        (unsigned char*)&pixmap, 1);

        // Lower to bottom
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

bool DDEOverlayOutput::getWindowGeometry(const std::string& monitor, WindowGeometry& geom) const {
    for (const auto& w : m_windows) {
        if (w.monitorName == monitor) {
            geom.x = w.x;
            geom.y = w.y;
            geom.width = w.width;
            geom.height = w.height;
            return true;
        }
    }
    return false;
}

void DDEOverlayOutput::reset() {}
bool DDEOverlayOutput::renderVFlip() const { return false; }
bool DDEOverlayOutput::renderMultiple() const { return m_windows.size() > 1; }
bool DDEOverlayOutput::haveImageBuffer() const { return true; }
void* DDEOverlayOutput::getImageBuffer() const { return m_imageData; }
uint32_t DDEOverlayOutput::getImageBufferSize() const { return m_imageSize; }
void DDEOverlayOutput::updateRender() const {
    // TODO: write render result to X11 window
}

} // namespace WallpaperEngine::DDE
