import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtMultimedia
import AedstDesignSystem

// Video (or audio) player on Qt Multimedia: MediaPlayer + VideoOutput + AudioOutput,
// with keyboard-accessible controls. `source` is a presigned URL (playbackUrl role:
// the OPTIMIZED rendition when the server produced one, else the original).
//
// Keys (when the player has focus): Space / K play-pause, Left / Right seek 5 s,
// M mute. Playback stops when the item is destroyed or hidden.
FocusScope {
    id: root

    property url source
    /// Poster shown before the first frame (image://media/... from previewSource).
    property url posterSource
    property bool autoPlay: false

    readonly property bool playing: player.playbackState === MediaPlayer.PlayingState
    readonly property bool hasError: player.error !== MediaPlayer.NoError

    function formatTime(ms) {
        const total = Math.max(0, Math.floor(ms / 1000))
        const minutes = Math.floor(total / 60)
        const seconds = total % 60
        return minutes + ":" + (seconds < 10 ? "0" : "") + seconds
    }

    function togglePlay() {
        if (root.playing) {
            player.pause()
        } else {
            player.play()
        }
    }

    implicitWidth: 640
    implicitHeight: 400
    activeFocusOnTab: true

    Accessible.role: Accessible.Animation
    Accessible.name: qsTr("Video player")
    Accessible.description: root.playing ? qsTr("Playing") : qsTr("Paused")

    Keys.onSpacePressed: root.togglePlay()
    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_K) {
            root.togglePlay(); event.accepted = true
        } else if (event.key === Qt.Key_Left) {
            player.position = Math.max(0, player.position - 5000); event.accepted = true
        } else if (event.key === Qt.Key_Right) {
            player.position = Math.min(player.duration, player.position + 5000); event.accepted = true
        } else if (event.key === Qt.Key_M) {
            audio.muted = !audio.muted; event.accepted = true
        }
    }

    onVisibleChanged: if (!visible) player.pause()
    Component.onDestruction: player.stop()

    MediaPlayer {
        id: player
        source: root.source
        audioOutput: AudioOutput { id: audio }
        videoOutput: video
        Component.onCompleted: if (root.autoPlay) play()
    }

    Rectangle {
        anchors.fill: parent
        color: "black"
        radius: Theme.radiusMd
    }

    VideoOutput {
        id: video
        anchors.fill: parent
        anchors.bottomMargin: controls.height
        fillMode: VideoOutput.PreserveAspectFit
    }

    Image {
        anchors.fill: video
        source: root.posterSource
        asynchronous: true
        fillMode: Image.PreserveAspectFit
        visible: player.position === 0 && !root.playing && String(root.posterSource).length > 0
        Accessible.ignored: true
    }

    Text {
        anchors.centerIn: video
        visible: root.hasError
        text: qsTr("This video cannot be played")
        color: "white"
        font: Theme.bodyFont
        wrapMode: Text.WordWrap
    }

    MouseArea {
        anchors.fill: video
        onClicked: { root.forceActiveFocus(); root.togglePlay() }
    }

    RowLayout {
        id: controls
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Theme.spacingSm
        height: implicitHeight
        spacing: Theme.spacingSm

        AedstIconButton {
            iconText: root.playing ? "⏸" : "▶"
            tooltipText: root.playing ? qsTr("Pause (Space)") : qsTr("Play (Space)")
            Accessible.name: root.playing ? qsTr("Pause") : qsTr("Play")
            enabled: !root.hasError
            onClicked: root.togglePlay()
        }

        Text {
            text: root.formatTime(player.position)
            color: "white"
            font: Theme.captionFont
        }

        Slider {
            id: seek
            Layout.fillWidth: true
            from: 0
            to: Math.max(1, player.duration)
            value: player.position
            enabled: player.seekable
            Accessible.name: qsTr("Seek")
            onMoved: player.position = value
        }

        Text {
            text: root.formatTime(player.duration)
            color: "white"
            font: Theme.captionFont
        }

        AedstIconButton {
            iconText: audio.muted ? "\u{1F507}" : "\u{1F50A}"
            tooltipText: audio.muted ? qsTr("Unmute (M)") : qsTr("Mute (M)")
            Accessible.name: audio.muted ? qsTr("Unmute") : qsTr("Mute")
            onClicked: audio.muted = !audio.muted
        }
    }
}
