import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

ApplicationWindow {
    id: settingsWindow
    title: "Wallpaper Engine Settings"
    width: 800
    height: 600
    visible: false
    modality: Qt.ApplicationModal

    property var wallpaperModel: ListModel {}

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16

        // Category tabs
        TabBar {
            id: categoryBar
            Layout.fillWidth: true

            TabButton { text: "Scene" }
            TabButton { text: "Video" }
            TabButton { text: "Web" }
            TabButton { text: "Image" }
            TabButton { text: "Local" }
        }

        // Wallpaper grid
        GridView {
            id: wallpaperGrid
            Layout.fillWidth: true
            Layout.fillHeight: true
            cellWidth: 180
            cellHeight: 140

            model: wallpaperModel

            delegate: Item {
                width: wallpaperGrid.cellWidth
                height: wallpaperGrid.cellHeight

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 8
                    color: mouseArea.containsMouse ? "#333" : "#222"
                    radius: 8

                    Image {
                        anchors.fill: parent
                        anchors.margins: 4
                        source: model.thumbnail
                        fillMode: Image.PreserveAspectCrop
                    }

                    Text {
                        anchors.bottom: parent.bottom
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.margins: 8
                        color: "white"
                        text: model.name
                        font.pixelSize: 12
                    }

                    MouseArea {
                        id: mouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            dbusClient.setWallpaper(currentMonitor, model.path)
                        }
                    }
                }
            }
        }

        // Bottom control bar
        RowLayout {
            Layout.fillWidth: true

            Label { text: "Monitor:" }
            ComboBox {
                id: monitorCombo
                model: ["DP-1", "HDMI-1"]
            }

            Label { text: "Volume:" }
            Slider {
                id: volumeSlider
                from: 0
                to: 100
                value: 30
                onValueChanged: dbusClient.setVolume(value)
            }

            Label { text: "FPS:" }
            Slider {
                id: fpsSlider
                from: 1
                to: 60
                value: 30
                stepSize: 1
                onValueChanged: dbusClient.setFPS(value)
            }

            CheckBox {
                id: pauseFullscreen
                text: "Pause on fullscreen"
                checked: true
            }

            Button {
                text: "Close"
                onClicked: settingsWindow.close()
            }
        }
    }

    property string currentMonitor: monitorCombo.currentText

    function show() {
        visible = true;
        raise();
    }
}
