import QtQuick 2.0
// Deliberately imported after QtQuick to avoid missing restoreMode property in Binding. Fix in Qt 6.
import QtQml 2.14
import QtQuick.Window 2.2
import QtQuick.VirtualKeyboard 2.2
import QtQuick.VirtualKeyboard.Settings 2.2
import QtWebEngine 1.7

Item {

    Item {
        id: appContainer
        objectName: "appContainer"
        width: Screen.orientation === Qt.LandscapeOrientation ? parent.width : parent.height
        height: Screen.orientation === Qt.LandscapeOrientation ? parent.height : parent.width
        anchors.centerIn: parent

        WebEngineView {
            anchors.fill: parent
            id: virtualKeyboard
            objectName: "engineView"
        }
    }

        InputPanel {
        id: inputPanel
        y: Qt.inputMethod.visible ? parent.height - inputPanel.height : parent.height
        anchors.left: parent.left
        anchors.right: parent.right
    }

        Binding {
            target: VirtualKeyboardSettings
            property: "fullScreenMode"
            value: appContainer.height > 0 && (appContainer.width / appContainer.height) > (16.0 / 9.0)
            restoreMode: Binding.RestoreBinding
        }

    property bool inLandscapeOrientation: Screen.orientation === Qt.LandscapeOrientation

    Screen.orientationUpdateMask: Qt.LandscapeOrientation | Qt.PortraitOrientation

    Binding {
        target: appContainer.Window.window !== null ? appContainer.Window.window.contentItem : null
        property: "rotation"
        value: inLandscapeOrientation ? 0 : 90
    }
}