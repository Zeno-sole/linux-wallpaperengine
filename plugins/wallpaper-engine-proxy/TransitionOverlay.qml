import QtQuick 2.15

Item {
    id: root
    visible: false
    anchors.fill: parent

    property bool isTransitioning: false

    Rectangle {
        id: overlay
        anchors.fill: parent
        color: "black"
        opacity: 0

        Behavior on opacity {
            NumberAnimation {
                duration: 300
                easing.type: Easing.InOutQuad
            }
        }
    }

    function startTransition() {
        if (isTransitioning) return;
        isTransitioning = true;
        visible = true;
        overlay.opacity = 1;  // Fade in black mask
    }

    function endTransition() {
        overlay.opacity = 0;  // Fade out black mask
        // Wait for animation to finish then hide
        Qt.callLater(function() {
            isTransitioning = false;
            visible = false;
        });
    }

    // Connect DBusClient signals
    Connections {
        target: dbusClient  // Set by WallpaperEnginePlugin
        function onWallpaperChanged(monitor, path) {
            root.startTransition();
            // Give engine some time to switch wallpaper
            transitionTimer.start();
        }
    }

    Timer {
        id: transitionTimer
        interval: 500
        onTriggered: root.endTransition()
    }
}
