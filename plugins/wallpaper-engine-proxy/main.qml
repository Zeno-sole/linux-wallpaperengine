import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    id: root
    visible: false  // Proxy plugin itself is invisible, only manages lifecycle

    // Transition animation overlay
    TransitionOverlay {
        id: transitionOverlay
    }

    // Settings dialog
    SettingsDialog {
        id: settingsDialog
    }

    Component.onCompleted: {
        console.log("WallpaperEngineProxy loaded")
    }
}
