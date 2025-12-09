import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    height: 85
    radius: 9
    color: highlighted ? "#CC2a2a2a" : (hovered ? "#CC242424" : "#CC1e1e1e")  // Semi-transparent

    property bool hovered: false
    property bool highlighted: false
    property string fontFamily: "Segoe UI"
    
    signal clicked()
    signal middleClicked()  // Middle click to open without closing window

    Behavior on color {
        ColorAnimation { duration: 100 }
    }

    // Left accent border
    Rectangle {
        id: leftBorder
        width: 3
        height: parent.height - 16
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        radius: 2
        color: "#09d7d0"
        opacity: highlighted ? 1.0 : 0.0
        
        Behavior on opacity {
            NumberAnimation { duration: 100 }
        }
    }

    MouseArea {
        id: itemMouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        acceptedButtons: Qt.LeftButton | Qt.MiddleButton  // Accept both left and middle clicks
        
        onPositionChanged: function(mouse) {
            // Map to window coordinates and check if mouse really moved
            var windowPos = mapToItem(null, mouse.x, mouse.y)
            if (typeof searchWindow !== 'undefined' && searchWindow.checkMouseMoved) {
                searchWindow.checkMouseMoved(windowPos.x, windowPos.y)
            }
        }
        
        onEntered: root.hovered = true
        onExited: root.hovered = false
        onClicked: function(mouse) {
            if (mouse.button === Qt.MiddleButton) {
                root.middleClicked()
            } else {
                root.clicked()
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 15
        anchors.rightMargin: 15
        spacing: 16

        // Weapon icon with dark background
        Rectangle {
            Layout.preferredWidth: 68
            Layout.preferredHeight: 68
            color: "#151515"
            radius: 7

            Image {
                anchors.fill: parent
                anchors.margins: 8
                source: model.icon ? "https://www.bungie.net" + model.icon : ""
                fillMode: Image.PreserveAspectFit
                smooth: true
                mipmap: true
                antialiasing: true
                cache: true
                asynchronous: true
            }
        }

        // Weapon info
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 5

            // Name row with icons
            RowLayout {
                spacing: 8
                Layout.fillWidth: true
                
                Text {
                    text: model.name
                    font.family: root.fontFamily
                    font.pixelSize: 20
                    font.weight: Font.Bold
                    color: highlighted ? "#ffffff" : "#cccccc"
                    elide: Text.ElideRight
                    Layout.maximumWidth: implicitWidth
                    
                    Behavior on color {
                        ColorAnimation { duration: 100 }
                    }
                }
                
                // Holofoil badge with rainbow animation
                Rectangle {
                    id: holofoilBadge
                    visible: model.isHolofoil === true
                    color: "#2d1f4e"  // Dark purple background
                    border.color: "#8b5cf6"
                    border.width: 1
                    radius: 4
                    Layout.preferredWidth: holofoilText.implicitWidth + 12
                    Layout.preferredHeight: holofoilText.implicitHeight + 6
                    clip: true
                    
                    // Rainbow gradient property for animation
                    property real gradientOffset: 0
                    
                    NumberAnimation on gradientOffset {
                        from: 0
                        to: 1
                        duration: 5000
                        loops: Animation.Infinite
                        running: holofoilBadge.visible
                    }
                    
                    // Animated rainbow gradient overlay
                    Rectangle {
                        anchors.fill: parent
                        radius: parent.radius
                        opacity: 0.3
                        
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: Qt.hsla((holofoilBadge.gradientOffset + 0.0) % 1.0, 0.8, 0.6, 1) }
                            GradientStop { position: 0.2; color: Qt.hsla((holofoilBadge.gradientOffset + 0.2) % 1.0, 0.8, 0.6, 1) }
                            GradientStop { position: 0.4; color: Qt.hsla((holofoilBadge.gradientOffset + 0.4) % 1.0, 0.8, 0.6, 1) }
                            GradientStop { position: 0.6; color: Qt.hsla((holofoilBadge.gradientOffset + 0.6) % 1.0, 0.8, 0.6, 1) }
                            GradientStop { position: 0.8; color: Qt.hsla((holofoilBadge.gradientOffset + 0.8) % 1.0, 0.8, 0.6, 1) }
                            GradientStop { position: 1.0; color: Qt.hsla((holofoilBadge.gradientOffset + 1.0) % 1.0, 0.8, 0.6, 1) }
                        }
                    }
                    
                    Text {
                        id: holofoilText
                        anchors.centerIn: parent
                        text: "HOLOFOIL"
                        font.family: root.fontFamily
                        font.pixelSize: 10
                        font.weight: Font.Bold
                        color: "#ffffff"
                    }
                }
                
                // Damage type icon
                Image {
                    visible: model.damageTypeIcon && model.damageTypeIcon.length > 0
                    source: model.damageTypeIcon ? "https://www.bungie.net" + model.damageTypeIcon : ""
                    Layout.preferredWidth: 18
                    Layout.preferredHeight: 18
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    mipmap: true
                    antialiasing: true
                    cache: true
                    asynchronous: true
                }
                
                // Ammo type icon
                Image {
                    visible: model.ammoTypeIcon && model.ammoTypeIcon.length > 0
                    source: model.ammoTypeIcon ? "https://www.bungie.net" + model.ammoTypeIcon : ""
                    Layout.preferredWidth: 18
                    Layout.preferredHeight: 18
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    mipmap: true
                    antialiasing: true
                    cache: true
                    asynchronous: true
                }
                
                // Spacer to push everything to left
                Item {
                    Layout.fillWidth: true
                }
            }

            // Info row
            RowLayout {
                spacing: 7
                
                // Helper function to check if field is matched (supports comma-separated list)
                function isFieldMatched(fieldName) {
                    if (!model.matchedField) return false
                    return model.matchedField.indexOf(fieldName) !== -1
                }

                // Weapon Type badge
                Rectangle {
                    visible: model.weaponType && model.weaponType.length > 0
                    property bool isMatched: parent.isFieldMatched("weaponType")
                    color: isMatched ? "#5a4a2d" : "transparent"
                    radius: 4
                    Layout.preferredWidth: weaponTypeText.implicitWidth + (isMatched ? 12 : 0)
                    Layout.preferredHeight: weaponTypeText.implicitHeight + (isMatched ? 6 : 0)
                    
                    Text {
                        id: weaponTypeText
                        anchors.centerIn: parent
                        text: model.weaponType ? model.weaponType.toUpperCase() : ""
                        font.family: root.fontFamily
                        font.pixelSize: 14
                        font.weight: Font.Medium
                        color: parent.isMatched ? "#d7a909" : "#999999"
                    }
                }

                Text {
                    text: "•"
                    font.pixelSize: 12
                    color: "#555555"
                    visible: model.frameType && model.frameType.length > 0
                }

                // Frame Type badge
                Rectangle {
                    visible: model.frameType && model.frameType.length > 0
                    property bool isMatched: parent.isFieldMatched("frameType")
                    color: isMatched ? "#5a4a2d" : "transparent"
                    radius: 4
                    Layout.preferredWidth: frameTypeText.implicitWidth + (isMatched ? 12 : 0)
                    Layout.preferredHeight: frameTypeText.implicitHeight + (isMatched ? 6 : 0)
                    
                    Text {
                        id: frameTypeText
                        anchors.centerIn: parent
                        text: model.frameType || ""
                        font.family: root.fontFamily
                        font.pixelSize: 14
                        color: parent.isMatched ? "#d7a909" : "#888888"
                    }
                }

                Text {
                    text: "•"
                    font.pixelSize: 12
                    color: "#555555"
                    visible: model.seasonNumber > 0
                }

                // Season Number badge
                Rectangle {
                    visible: model.seasonNumber > 0
                    property bool isMatched: parent.isFieldMatched("seasonNumber")
                    color: isMatched ? "#5a4a2d" : "transparent"
                    radius: 4
                    Layout.preferredWidth: seasonText.implicitWidth + (isMatched ? 12 : 0)
                    Layout.preferredHeight: seasonText.implicitHeight + (isMatched ? 6 : 0)
                    
                    Text {
                        id: seasonText
                        anchors.centerIn: parent
                        text: model.seasonNumber > 0 ? "Season " + model.seasonNumber : ""
                        font.family: root.fontFamily
                        font.pixelSize: 14
                        color: parent.isMatched ? "#d7a909" : "#666666"
                    }
                }

                Text {
                    text: "•"
                    font.pixelSize: 12
                    color: "#555555"
                    visible: model.seasonName && model.seasonName.length > 0
                }

                // Season Name badge
                Rectangle {
                    visible: model.seasonName && model.seasonName.length > 0
                    property bool isMatched: parent.isFieldMatched("seasonName")
                    color: isMatched ? "#5a4a2d" : "transparent"
                    radius: 4
                    Layout.preferredWidth: seasonNameText.implicitWidth + (isMatched ? 12 : 0)
                    Layout.preferredHeight: seasonNameText.implicitHeight + (isMatched ? 6 : 0)
                    
                    Text {
                        id: seasonNameText
                        anchors.centerIn: parent
                        text: model.seasonName || ""
                        font.family: root.fontFamily
                        font.pixelSize: 14
                        color: parent.isMatched ? "#d7a909" : "#666666"
                    }
                }
            }
        }
    }
}
