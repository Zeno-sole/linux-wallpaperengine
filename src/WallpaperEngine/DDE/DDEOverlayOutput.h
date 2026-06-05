#pragma once

#include "WallpaperEngine/Render/Drivers/Output/Output.h"
#include "WallpaperEngine/Render/Drivers/Output/OutputViewport.h"
#include <X11/Xlib.h>
#include <string>
#include <vector>

namespace WallpaperEngine::DDE {

struct OverlayWindow {
    ::Window xwindow;
    std::string monitorName;
    int x, y, width, height;
};

struct WindowGeometry {
    int x = 0, y = 0, width = 0, height = 0;
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
    bool getWindowGeometry(const std::string& monitor, WindowGeometry& geom) const;

private:
    Display* m_display = nullptr;
    std::vector<OverlayWindow> m_windows;
    char* m_imageData = nullptr;
    uint32_t m_imageSize = 0;
};

} // namespace WallpaperEngine::DDE
