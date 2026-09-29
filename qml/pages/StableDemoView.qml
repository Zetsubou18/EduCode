import QtQuick 2.15

Item {
    id: view
    property string frame: ""
    property bool streaming: true
    property bool zoomable: false
    property real zoom: 1
    property int nativeWidth: 0
    property int nativeHeight: 0
    property int visibleFrame: -1

    function updateFrame() {
        if (!streaming || frame === "") {
            if (frame === "") visibleFrame = -1
            return
        }
        if (visibleFrame === 0) second.source = frame
        else first.source = frame
    }
    function ready(index, image) {
        if (!streaming || image.status !== Image.Ready || image.source.toString() !== frame)
            return
        nativeWidth = image.implicitWidth
        nativeHeight = image.implicitHeight
        visibleFrame = index
    }
    function fit() { zoom = 1 }
    function actualSize() { zoom = fitScale > 0 ? 1 / fitScale : 1 }

    onFrameChanged: updateFrame()
    onStreamingChanged: updateFrame()
    Component.onCompleted: updateFrame()

    readonly property real fitScale: nativeWidth > 0 && nativeHeight > 0
                                     ? Math.min(viewport.width / nativeWidth, viewport.height / nativeHeight) : 1
    readonly property real renderWidth: Math.max(1, nativeWidth * fitScale * zoom)
    readonly property real renderHeight: Math.max(1, nativeHeight * fitScale * zoom)

    Flickable {
        id: viewport
        anchors.fill: parent
        clip: true
        interactive: view.zoomable && (contentWidth > width || contentHeight > height)
        contentWidth: Math.max(width, view.renderWidth)
        contentHeight: Math.max(height, view.renderHeight)

        Item {
            width: viewport.contentWidth
            height: viewport.contentHeight
            Image {
                id: first
                x: (parent.width - width) / 2
                y: (parent.height - height) / 2
                width: view.renderWidth
                height: view.renderHeight
                visible: view.visibleFrame === 0 && status === Image.Ready
                fillMode: Image.Stretch
                cache: false
                asynchronous: true
                onStatusChanged: view.ready(0, first)
            }
            Image {
                id: second
                x: (parent.width - width) / 2
                y: (parent.height - height) / 2
                width: view.renderWidth
                height: view.renderHeight
                visible: view.visibleFrame === 1 && status === Image.Ready
                fillMode: Image.Stretch
                cache: false
                asynchronous: true
                onStatusChanged: view.ready(1, second)
            }
        }
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.NoButton
            enabled: view.zoomable
            onWheel: {
                view.zoom = Math.max(0.5, Math.min(8, view.zoom * (wheel.angleDelta.y > 0 ? 1.25 : 0.8)))
                wheel.accepted = true
            }
        }
    }
}
