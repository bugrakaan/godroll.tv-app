import QtQuick
import QtQuick.Window

Window {
    id: root
    width: 700
    height: searchWindowComponent.height + 20  // Dynamic height based on content
    visible: !startHidden  // Start hidden if --hidden flag was passed
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool
    color: "transparent"
    opacity: 0  // Start invisible, animate in

    // Track loading state
    property bool isLoading: true
    
    // Track if we're in the process of hiding (for animation)
    property bool isHiding: false
    // Track if ESC was pressed (to do reset after hide animation)
    property bool escPressed: false

    // Opacity animation for smooth show/hide
    Behavior on opacity {
        NumberAnimation {
            duration: 200
            easing.type: Easing.OutCubic
        }
    }
    
    // When opacity animation finishes and we're hiding, actually hide the window
    onOpacityChanged: {
        if (opacity === 0 && isHiding) {
            if (escPressed) {
                // ESC was pressed, do reset
                searchModel.clearSearch()
                searchWindowComponent.resetScrollPosition()
                escPressed = false
            }
            root.hide()
            isHiding = false
        }
    }

    // Center on screen
    Component.onCompleted: {
        x = (Screen.width - width) / 2
        y = (Screen.height - height) / 3
        if (!startHidden) {
            root.opacity = 1  // Animate in on first show
            root.raise()
            root.requestActivate()
        }
    }

    // Keep window vertically centered when height changes
    onHeightChanged: {
        y = (Screen.height - height) / 3
    }

    // Update loading state when weapons are loaded
    Connections {
        target: searchModel
        function onWeaponsLoaded() {
            root.isLoading = false
            searchWindowComponent.focusSearchInput()
        }
    }
    
    // Handle weapon reload (F5)
    Connections {
        target: weaponLoader
        function onReloadStarted() {
            root.isLoading = true
        }
        function onWeaponsLoaded() {
            root.isLoading = false
            searchWindowComponent.focusSearchInput()
        }
    }

    // Hide when focus is lost (no reset, just hide with animation)
    onActiveChanged: {
        if (!active && visible && !ignoreFocusLoss && !isHiding) {
            isHiding = true
            root.opacity = 0
        }
    }

    Connections {
        target: hotkey
        function onActivated() {
            toggleWindow()
        }
    }

    Connections {
        target: trayIcon
        function onShowHideRequested() {
            toggleWindow()
        }
    }

    function toggleWindow() {
        if (root.visible && !isHiding) {
            // Just hide with animation, no reset (reset only happens on ESC)
            isHiding = true
            root.opacity = 0
        } else if (!root.visible) {
            root.show()
            root.raise()
            root.requestActivate()
            searchWindowComponent.refocusOnly()
            root.opacity = 1
        }
    }

    // Click outside to close (no reset, just hide with animation)
    MouseArea {
        anchors.fill: parent
        onClicked: {
            if (!root.isHiding) {
                root.isHiding = true
                root.opacity = 0
            }
        }
    }
    
    // Temporarily ignore focus loss when middle-clicking to open URLs
    property bool ignoreFocusLoss: false
    
    // Timer to refocus window after browser opens
    Timer {
        id: refocusTimer
        interval: 150  // Short delay to let browser open
        onTriggered: {
            root.ignoreFocusLoss = false
            root.show()
            root.raise()
            root.requestActivate()
            searchWindowComponent.refocusOnly()  // Just refocus, don't reset scroll/selection
        }
    }

    SearchWindow {
        id: searchWindowComponent
        anchors.centerIn: parent
        isLoading: root.isLoading
        onClose: {
            // ESC triggers reset + hide with animation
            if (!root.isHiding) {
                root.escPressed = true
                root.isHiding = true
                root.opacity = 0
            }
        }
        onRefocusNeeded: {
            // Set flag to ignore the focus loss that will happen
            root.ignoreFocusLoss = true
            refocusTimer.restart()
        }
    }

}
