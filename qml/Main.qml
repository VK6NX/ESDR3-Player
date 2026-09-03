// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound

import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import ESDR3Player

ApplicationWindow {
    id: root
    width: 1200
    height: 760
    minimumWidth: 800
    minimumHeight: 480
    visible: true
    color: "#141b26"
    title: "ESDR3 Player " + Player.version

    property int editorFocus: 0
    readonly property bool editing: editorFocus > 0

    component TuneField: TextField {
        id: field
        property real value: 0
        property real divisor: 1
        property int decimals: 0
        property real minValue: 0
        property real maxValue: 0
        signal committed(real v)

        horizontalAlignment: Text.AlignRight
        selectByMouse: true
        validator: DoubleValidator {
            bottom: field.minValue; top: field.maxValue
            decimals: field.decimals; locale: "C"
            notation: DoubleValidator.StandardNotation
        }
        Binding {
            target: field
            property: "text"
            value: (field.value / field.divisor).toFixed(field.decimals)
            when: !field.activeFocus
            restoreMode: Binding.RestoreNone
        }
        onActiveFocusChanged: root.editorFocus += activeFocus ? 1 : -1
        onEditingFinished: {
            if (acceptableInput) field.committed(Number(text) * field.divisor)
            text = (field.value / field.divisor).toFixed(field.decimals)
            root.contentItem.forceActiveFocus()
        }
        Keys.onEscapePressed: {
            text = (field.value / field.divisor).toFixed(field.decimals)
            root.contentItem.forceActiveFocus()
        }
    }

    Settings {
        id: settings
        property alias windowWidth: root.width
        property alias windowHeight: root.height
        property string lastDir: ""
        property alias fftSize: fftCombo.currentIndex
        property alias averaging: avgSlider.value
        property alias dbMin: dbMinSlider.value
        property alias dbMax: dbMaxSlider.value
        property alias wfDbMin: wfMinSlider.value
        property alias wfDbMax: wfMaxSlider.value
        property alias waterfallRatio: wfRatioSlider.value
        property alias paletteIndex: paletteCombo.currentIndex
        property alias wfAuto: wfAutoCheck.checked
        property alias loop: loopButton.checked
    }

    FileDialog {
        id: fileDialog
        title: qsTr("Open IQ recording")
        nameFilters: [qsTr("IQ WAV (*.wav)"), qsTr("All files (*)")]
        currentFolder: settings.lastDir !== "" ? settings.lastDir : StandardPaths.writableLocation(StandardPaths.MusicLocation)
        onAccepted: {
            settings.lastDir = currentFolder
            Player.open(selectedFile)
        }
    }

    Shortcut { sequences: [StandardKey.Open]; onActivated: fileDialog.open() }
    Shortcut { enabled: !root.editing; sequence: "Space"; onActivated: Player.togglePlay() }
    Shortcut { enabled: !root.editing; sequence: "Left"; onActivated: Player.seekBy(-5) }
    Shortcut { enabled: !root.editing; sequence: "Right"; onActivated: Player.seekBy(5) }
    Shortcut { enabled: !root.editing; sequence: "Shift+Left"; onActivated: Player.seekBy(-60) }
    Shortcut { enabled: !root.editing; sequence: "Shift+Right"; onActivated: Player.seekBy(60) }
    Shortcut { enabled: !root.editing; sequence: "Home"; onActivated: Player.seek(0) }
    Shortcut { enabled: !root.editing; sequence: "Up"; onActivated: Player.tuneBy(10) }
    Shortcut { enabled: !root.editing; sequence: "Down"; onActivated: Player.tuneBy(-10) }
    Shortcut { enabled: !root.editing; sequence: "Shift+Up"; onActivated: Player.tuneBy(100) }
    Shortcut { enabled: !root.editing; sequence: "Shift+Down"; onActivated: Player.tuneBy(-100) }
    Shortcut { enabled: !root.editing; sequence: "M"; onActivated: Player.mute = !Player.mute }
    Shortcut { enabled: !root.editing; sequences: ["+", "="]; onActivated: panorama.zoomIn() }
    Shortcut { enabled: !root.editing; sequence: "-"; onActivated: panorama.zoomOut() }
    Shortcut { enabled: !root.editing; sequence: "0"; onActivated: panorama.resetZoom() }

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            spacing: 8

            ToolButton { text: qsTr("Open"); onClicked: fileDialog.open() }
            ToolSeparator {}
            ToolButton {
                text: Player.playing ? qsTr("Pause") : qsTr("Play")
                enabled: Player.hasFile
                onClicked: Player.togglePlay()
            }
            ToolButton { text: qsTr("Stop"); enabled: Player.hasFile; onClicked: Player.stop() }
            ToolButton {
                id: loopButton
                text: qsTr("Loop")
                checkable: true
                onCheckedChanged: Player.loop = checked
                Component.onCompleted: Player.loop = checked
            }
            ComboBox {
                id: speedCombo
                model: ["0.5x", "1x", "2x", "4x"]
                currentIndex: 1
                implicitWidth: 80
                onActivated: Player.speed = [0.5, 1, 2, 4][currentIndex]
            }
            ToolSeparator {}
            Label {
                Layout.fillWidth: true
                elide: Text.ElideMiddle
                text: Player.hasFile ? Player.fileName : qsTr("No file opened")
            }
            Label {
                text: Player.recTimeText
                font.family: "Menlo"
                visible: text !== ""
            }
            Label {
                text: Player.positionText
                font.family: "Menlo"
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 6
        spacing: 6

        PanoramaItem {
            id: panorama
            Layout.fillWidth: true
            Layout.fillHeight: true
            source: Player.spectrum
            centerHz: Player.ddsHz
            sampleRate: Player.sampleRate
            recStart: Player.recStart
            vfoHz: Player.vfoHz
            filterVisible: Player.hasFile
            filterLowHz: Player.filterLowHz
            filterHighHz: Player.filterHighHz
            wfAuto: wfAutoCheck.checked
            onVfoRequested: (hz) => Player.vfoHz = hz
            onFilterEdgeDragged: (low, high) => Player.setFilterEdges(low, high)
            dbMin: dbMinSlider.value
            dbMax: dbMaxSlider.value
            wfDbMin: wfMinSlider.value
            wfDbMax: wfMaxSlider.value
            waterfallRatio: wfRatioSlider.value
            paletteName: paletteCombo.currentText
        }

        Pane {
            Layout.preferredWidth: 220
            Layout.fillHeight: true
            padding: 8

            ColumnLayout {
                anchors.fill: parent
                spacing: 6

                Label { text: qsTr("Receiver"); font.bold: true }
                RowLayout {
                    spacing: 4
                    Row {
                        id: digits
                        readonly property string shown: Player.hasFile ? (Player.vfoHz / 1000).toFixed(3) : "—"
                        readonly property int dot: shown.indexOf(".")
                        enabled: Player.hasFile

                        Repeater {
                            model: digits.shown.length
                            Text {
                                id: digit
                                required property int index
                                readonly property string ch: digits.shown.charAt(index)
                                readonly property bool tunable: digits.dot >= 0 && ch >= "0" && ch <= "9"
                                readonly property real weightHz: index < digits.dot
                                        ? 1000 * Math.pow(10, digits.dot - index - 1)
                                        : 1000 / Math.pow(10, index - digits.dot)
                                property int wheelAcc: 0

                                text: ch
                                color: hover.hovered ? "#ffcc33" : "#e6edf5"
                                font.family: "Menlo"
                                font.pointSize: 15

                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.top: parent.bottom
                                    height: 2
                                    color: "#ffcc33"
                                    visible: hover.hovered
                                }
                                HoverHandler {
                                    id: hover
                                    enabled: digit.tunable
                                    cursorShape: Qt.SizeVerCursor
                                }
                                WheelHandler {
                                    enabled: digit.tunable
                                    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                                    onWheel: (event) => {
                                        digit.wheelAcc += event.angleDelta.y
                                        const steps = (digit.wheelAcc / 120) | 0
                                        if (steps !== 0) {
                                            digit.wheelAcc -= steps * 120
                                            Player.tuneBy(steps * digit.weightHz)
                                        }
                                    }
                                }
                            }
                        }
                    }
                    Label { text: qsTr("kHz"); font.pointSize: 12 }
                    Item { Layout.fillWidth: true }
                }
                RowLayout {
                    spacing: 2
                    Repeater {
                        model: ["CW", "USB", "LSB", "AM", "FM"]
                        Button {
                            required property int index
                            required property string modelData
                            text: modelData
                            checkable: true
                            checked: Player.mode === index
                            Layout.fillWidth: true
                            padding: 4
                            onClicked: Player.mode = index
                        }
                    }
                }
                RowLayout {
                    Label { text: qsTr("Width"); Layout.preferredWidth: 60 }
                    Slider {
                        id: bwSlider
                        Layout.fillWidth: true
                        from: Player.bandwidthMin; to: Player.bandwidthMax; stepSize: 10
                        value: Player.bandwidthHz
                        onMoved: Player.bandwidthHz = value
                    }
                    TuneField {
                        Layout.preferredWidth: 56
                        value: Player.bandwidthHz
                        minValue: Player.bandwidthMin
                        maxValue: Player.bandwidthMax
                        onCommitted: (v) => Player.bandwidthHz = v
                    }
                }
                RowLayout {
                    visible: Player.mode === 0
                    Label { text: qsTr("Pitch"); Layout.preferredWidth: 60 }
                    Slider {
                        Layout.fillWidth: true
                        from: 300; to: 1200; stepSize: 10
                        value: Player.cwPitchHz
                        onMoved: Player.cwPitchHz = value
                    }
                    TuneField {
                        Layout.preferredWidth: 56
                        value: Player.cwPitchHz
                        minValue: 300
                        maxValue: 1200
                        onCommitted: (v) => Player.cwPitchHz = v
                    }
                }
                RowLayout {
                    Label { text: qsTr("AGC"); Layout.preferredWidth: 60 }
                    ComboBox {
                        Layout.fillWidth: true
                        model: [qsTr("Off"), qsTr("Fast"), qsTr("Slow")]
                        currentIndex: Player.agcMode
                        onActivated: Player.agcMode = currentIndex
                    }
                }
                RowLayout {
                    Label { text: qsTr("Volume"); Layout.preferredWidth: 60 }
                    Slider {
                        Layout.fillWidth: true
                        from: 0; to: 1
                        value: Player.volume
                        onMoved: Player.volume = value
                    }
                    Button {
                        text: Player.mute ? qsTr("Unmute") : qsTr("Mute")
                        padding: 4
                        onClicked: Player.mute = !Player.mute
                    }
                }
                RowLayout {
                    Label { text: qsTr("Output"); Layout.preferredWidth: 60 }
                    ComboBox {
                        id: audioCombo
                        Layout.fillWidth: true
                        model: Player.audioDeviceNames
                        currentIndex: Math.max(0, Player.audioDeviceIds.indexOf(Player.audioDeviceId))
                        onActivated: Player.audioDeviceId = Player.audioDeviceIds[currentIndex]
                    }
                }
                Label {
                    color: "#9fb0c0"
                    font.pointSize: 10
                    text: Player.audioRate > 0
                          ? qsTr("Audio %1 Hz, underruns %2").arg(Player.audioRate).arg(Player.underruns)
                          : qsTr("No audio output")
                }
                Label {
                    text: panorama.cursorInside ? qsTr("cursor %1 kHz").arg((panorama.cursorHz / 1000).toFixed(3)) : " "
                    color: "#9fb0c0"
                    font.family: "Menlo"
                }

                MenuSeparator { Layout.fillWidth: true }

                Label { text: qsTr("Spectrum"); font.bold: true }
                RowLayout {
                    Label { text: qsTr("FFT"); Layout.preferredWidth: 60 }
                    ComboBox {
                        id: fftCombo
                        Layout.fillWidth: true
                        model: ["4096", "8192", "16384", "32768"]
                        currentIndex: 2
                        onCurrentIndexChanged: Player.fftSize = parseInt(model[currentIndex])
                        Component.onCompleted: Player.fftSize = parseInt(model[currentIndex])
                    }
                }
                Label {
                    text: qsTr("RBW %1 Hz, span %2 kHz").arg(panorama.rbwHz.toFixed(2)).arg((panorama.spanHz / 1000).toFixed(1))
                    color: "#9fb0c0"
                    font.pointSize: 10
                }
                RowLayout {
                    Label { text: qsTr("Average"); Layout.preferredWidth: 60 }
                    Slider {
                        id: avgSlider
                        Layout.fillWidth: true
                        from: 0; to: 0.95; value: 0.5
                        onValueChanged: Player.averaging = value
                        Component.onCompleted: Player.averaging = value
                    }
                }
                RowLayout {
                    Label { text: qsTr("Top"); Layout.preferredWidth: 60 }
                    Slider { id: dbMaxSlider; Layout.fillWidth: true; from: -120; to: 0; stepSize: 1; value: -50 }
                    Label { text: dbMaxSlider.value; Layout.preferredWidth: 32; horizontalAlignment: Text.AlignRight }
                }
                RowLayout {
                    Label { text: qsTr("Bottom"); Layout.preferredWidth: 60 }
                    Slider { id: dbMinSlider; Layout.fillWidth: true; from: -160; to: -40; stepSize: 1; value: -140 }
                    Label { text: dbMinSlider.value; Layout.preferredWidth: 32; horizontalAlignment: Text.AlignRight }
                }

                MenuSeparator { Layout.fillWidth: true }

                Label { text: qsTr("Waterfall"); font.bold: true }
                RowLayout {
                    Label { text: qsTr("Palette"); Layout.preferredWidth: 60 }
                    ComboBox {
                        id: paletteCombo
                        Layout.fillWidth: true
                        model: panorama.paletteNames
                    }
                }
                CheckBox {
                    id: wfAutoCheck
                    text: qsTr("Auto range") + (checked && panorama.wfAutoMax !== 0
                          ? "  " + Math.round(panorama.wfAutoMin) + " … " + Math.round(panorama.wfAutoMax) + " dB" : "")
                    checked: true
                }
                RowLayout {
                    enabled: !wfAutoCheck.checked
                    Label { text: qsTr("Top"); Layout.preferredWidth: 60 }
                    Slider { id: wfMaxSlider; Layout.fillWidth: true; from: -120; to: 0; stepSize: 1; value: -70 }
                    Label { text: wfMaxSlider.value; Layout.preferredWidth: 32; horizontalAlignment: Text.AlignRight }
                }
                RowLayout {
                    enabled: !wfAutoCheck.checked
                    Label { text: qsTr("Bottom"); Layout.preferredWidth: 60 }
                    Slider { id: wfMinSlider; Layout.fillWidth: true; from: -160; to: -40; stepSize: 1; value: -130 }
                    Label { text: wfMinSlider.value; Layout.preferredWidth: 32; horizontalAlignment: Text.AlignRight }
                }
                RowLayout {
                    Label { text: qsTr("Height"); Layout.preferredWidth: 60 }
                    Slider { id: wfRatioSlider; Layout.fillWidth: true; from: 0.2; to: 0.85; value: 0.6 }
                }
                Button {
                    text: qsTr("Clear waterfall")
                    Layout.fillWidth: true
                    onClicked: panorama.clearWaterfall()
                }

                Item { Layout.fillHeight: true }

                RowLayout {
                    Label { text: qsTr("Language"); Layout.preferredWidth: 60 }
                    ComboBox {
                        id: languageCombo
                        Layout.fillWidth: true
                        model: Player.languageNames
                        currentIndex: Math.max(0, Player.languages.indexOf(Player.language))
                        onActivated: Player.language = Player.languages[currentIndex]
                    }
                }

                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: "#ff8080"
                    font.pointSize: 10
                    visible: Player.lastError !== ""
                    text: Player.lastError
                }
            }
        }
    }

    footer: Pane {
        padding: 6
        RowLayout {
            anchors.fill: parent
            spacing: 8
            Slider {
                id: positionSlider
                Layout.fillWidth: true
                from: 0; to: 1
                enabled: Player.hasFile
                onPressedChanged: if (!pressed) Player.seek(value)
                Binding {
                    target: positionSlider
                    property: "value"
                    value: Player.position
                    when: !positionSlider.pressed
                }
            }
            Label {
                text: positionSlider.pressed
                      ? Player.formatSeconds(positionSlider.value * Player.durationSec)
                      : [qsTr("no file"), qsTr("playing"), qsTr("paused"), qsTr("end of file")][Player.state]
                Layout.preferredWidth: 140
                horizontalAlignment: Text.AlignRight
                color: "#9fb0c0"
            }
        }
    }
}
