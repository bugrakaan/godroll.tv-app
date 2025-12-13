import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: updateDialog
    width: 450
    height: contentColumn.height + 48
    radius: 14
    color: "#1a1a1a"
    
    visible: false
    
    // Font
    property string mainFont: "Space Grotesk"
    
    signal accepted()
    signal rejected()
    signal skipped()
    
    function show() {
        visible = true
        opacity = 1
    }
    
    function hide() {
        visible = false
    }
    
    // Semi-transparent overlay background
    Rectangle {
        anchors.fill: parent
        color: "transparent"
    }
    
    ColumnLayout {
        id: contentColumn
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 24
        spacing: 16
        
        // Header with icon
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            
            // App logo icon
            Image {
                width: 48
                height: 48
                source: "qrc:/qt/qml/GodrollLauncher/resources/logo.svg"
                fillMode: Image.PreserveAspectFit
                sourceSize: Qt.size(48, 48)
            }
            
            ColumnLayout {
                spacing: 4
                
                Text {
                    text: updateChecker.updateAvailable ? "Update Available" : "You're Up to Date!"
                    font.family: updateDialog.mainFont
                    font.pixelSize: 20
                    font.weight: Font.Bold
                    color: "white"
                }
                
                Text {
                    text: updateChecker.updateAvailable 
                        ? "v" + updateChecker.currentVersion + " → v" + updateChecker.latestVersion
                        : "Current version: v" + updateChecker.currentVersion
                    font.family: updateDialog.mainFont
                    font.pixelSize: 14
                    color: "#09d7d0"
                }
            }
        }
        
        // Release notes (scrollable)
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(releaseNotesText.contentHeight + 16, 150)
            color: "#252525"
            radius: 8
            clip: true
            visible: updateChecker.releaseNotes.length > 0
            
            Flickable {
                id: notesFlickable
                anchors.fill: parent
                anchors.margins: 8
                contentHeight: releaseNotesText.height
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                
                // Prevent wheel events from propagating to parent
                MouseArea {
                    anchors.fill: parent
                    onWheel: function(wheel) {
                        if (wheel.angleDelta.y > 0) {
                            notesFlickable.contentY = Math.max(0, notesFlickable.contentY - 30)
                        } else {
                            notesFlickable.contentY = Math.min(
                                notesFlickable.contentHeight - notesFlickable.height,
                                notesFlickable.contentY + 30
                            )
                        }
                        wheel.accepted = true
                    }
                }
                
                Text {
                    id: releaseNotesText
                    width: notesFlickable.width
                    text: updateChecker.releaseNotes
                    font.family: updateDialog.mainFont
                    font.pixelSize: 13
                    color: "#aaaaaa"
                    wrapMode: Text.WordWrap
                    textFormat: Text.MarkdownText
                }
            }
            
            // Scroll indicator
            Rectangle {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.margins: 2
                width: 4
                radius: 2
                color: "#333333"
                visible: notesFlickable.contentHeight > notesFlickable.height
                
                Rectangle {
                    width: parent.width
                    height: Math.max(20, parent.height * (notesFlickable.height / notesFlickable.contentHeight))
                    y: (parent.height - height) * (notesFlickable.contentY / (notesFlickable.contentHeight - notesFlickable.height))
                    radius: 2
                    color: "#09d7d0"
                    opacity: 0.7
                }
            }
        }
        
        // Buttons row - different buttons based on update availability
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            visible: updateChecker.updateAvailable
            
            // Skip button
            Rectangle {
                Layout.preferredWidth: 80
                Layout.preferredHeight: 40
                color: skipMouse.containsMouse ? "#333333" : "transparent"
                border.color: "#555555"
                border.width: 1
                radius: 8
                
                Text {
                    anchors.centerIn: parent
                    text: "Skip"
                    font.family: updateDialog.mainFont
                    font.pixelSize: 14
                    color: "#888888"
                }
                
                MouseArea {
                    id: skipMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        updateChecker.skipVersion(updateChecker.latestVersion)
                        updateDialog.skipped()
                        updateDialog.hide()
                    }
                }
            }
            
            Item { Layout.fillWidth: true }
            
            // Later button
            Rectangle {
                Layout.preferredWidth: 100
                Layout.preferredHeight: 40
                color: laterMouse.containsMouse ? "#333333" : "#252525"
                border.color: "#555555"
                border.width: 1
                radius: 8
                
                Text {
                    anchors.centerIn: parent
                    text: "Later"
                    font.family: updateDialog.mainFont
                    font.pixelSize: 14
                    color: "white"
                }
                
                MouseArea {
                    id: laterMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        updateDialog.rejected()
                        updateDialog.hide()
                    }
                }
            }
            
            // Download button
            Rectangle {
                id: downloadBtn
                Layout.preferredWidth: updateChecker.downloading ? 180 : 120
                Layout.preferredHeight: 40
                color: updateChecker.downloading ? "#666666" : (downloadMouse.containsMouse ? "#0bc5bf" : "#09d7d0")
                radius: 8
                clip: true
                
                Behavior on Layout.preferredWidth {
                    NumberAnimation { duration: 150 }
                }
                
                RowLayout {
                    anchors.centerIn: parent
                    spacing: 6
                    
                    // Loading spinner (visible when downloading)
                    Rectangle {
                        id: spinner
                        width: 16
                        height: 16
                        radius: 8
                        color: "transparent"
                        border.color: "#1a1a1a"
                        border.width: 2
                        visible: updateChecker.downloading
                        
                        Rectangle {
                            width: 6
                            height: 6
                            radius: 3
                            color: "#1a1a1a"
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: parent.top
                            anchors.topMargin: 1
                        }
                        
                        RotationAnimation on rotation {
                            running: updateChecker.downloading
                            from: 0
                            to: 360
                            duration: 1000
                            loops: Animation.Infinite
                        }
                    }
                    
                    Text {
                        text: updateChecker.downloading ? updateChecker.statusText : "Download"
                        font.family: updateDialog.mainFont
                        font.pixelSize: 14
                        font.weight: Font.Bold
                        color: "#1a1a1a"
                    }
                }
                
                MouseArea {
                    id: downloadMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: updateChecker.downloading ? Qt.BusyCursor : Qt.PointingHandCursor
                    enabled: !updateChecker.downloading
                    onClicked: {
                        updateChecker.downloadAndInstall()
                    }
                }
            }
        }
        
        // Buttons row for "Up to Date" state
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            visible: !updateChecker.updateAvailable
            
            Item { Layout.fillWidth: true }
            
            // Close button
            Rectangle {
                Layout.preferredWidth: 100
                Layout.preferredHeight: 40
                color: closeBtnMouse.containsMouse ? "#333333" : "#252525"
                border.color: "#555555"
                border.width: 1
                radius: 8
                
                Text {
                    anchors.centerIn: parent
                    text: "Close"
                    font.family: updateDialog.mainFont
                    font.pixelSize: 14
                    color: "white"
                }
                
                MouseArea {
                    id: closeBtnMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        updateDialog.rejected()
                        updateDialog.hide()
                    }
                }
            }
            
            // Reinstall button
            Rectangle {
                id: reinstallBtn
                Layout.preferredWidth: updateChecker.downloading ? 180 : 140
                Layout.preferredHeight: 40
                color: updateChecker.downloading ? "#666666" : (reinstallMouse.containsMouse ? "#0bc5bf" : "#09d7d0")
                radius: 8
                clip: true
                
                Behavior on Layout.preferredWidth {
                    NumberAnimation { duration: 150 }
                }
                
                RowLayout {
                    anchors.centerIn: parent
                    spacing: 6
                    
                    // Loading spinner (visible when downloading)
                    Rectangle {
                        id: reinstallSpinner
                        width: 16
                        height: 16
                        radius: 8
                        color: "transparent"
                        border.color: "#1a1a1a"
                        border.width: 2
                        visible: updateChecker.downloading
                        
                        Rectangle {
                            width: 6
                            height: 6
                            radius: 3
                            color: "#1a1a1a"
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: parent.top
                            anchors.topMargin: 1
                        }
                        
                        RotationAnimation on rotation {
                            running: updateChecker.downloading
                            from: 0
                            to: 360
                            duration: 1000
                            loops: Animation.Infinite
                        }
                    }
                    
                    Text {
                        text: updateChecker.downloading ? updateChecker.statusText : "Reinstall"
                        font.family: updateDialog.mainFont
                        font.pixelSize: 14
                        font.weight: Font.Bold
                        color: "#1a1a1a"
                    }
                }
                
                MouseArea {
                    id: reinstallMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: updateChecker.downloading ? Qt.BusyCursor : Qt.PointingHandCursor
                    enabled: !updateChecker.downloading
                    onClicked: {
                        updateChecker.downloadAndInstall()
                    }
                }
            }
        }
    }
    
    // Close button (X)
    Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 8
        width: 28
        height: 28
        radius: 14
        color: closeMouse.containsMouse ? "#333333" : "transparent"
        
        Text {
            anchors.centerIn: parent
            text: "✕"
            font.pixelSize: 14
            color: closeMouse.containsMouse ? "white" : "#666666"
        }
        
        MouseArea {
            id: closeMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                updateDialog.rejected()
                updateDialog.hide()
            }
        }
    }
}
