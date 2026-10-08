import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import "../components" as C

ColumnLayout {
    id: panel
    objectName: "teacherPanel"
    property var session: backend.teacher
    property bool detached: false
    property int previewTab: 0
    readonly property int rosterHeight: Math.min(240, Math.max(156, session.students.length * 54))
    readonly property int panelHeight: session.role === "teacher" ? 112 + rosterHeight
                                      : session.role === "student" ? 390
                                      : session.role === "connecting" ? 80
                                      : session.status === "" ? 96 : session.canReconnect ? 154 : 116
    spacing: 6

    Text { text: "TEACHER MODE"; color: C.Theme.muted; font.pixelSize: 10; font.letterSpacing: 1.4 }
    C.ActionButton { Layout.fillWidth: true; visible: session.role === "idle"; text: "Создать конференцию"; onClicked: session.create() }
    C.ActionButton { Layout.fillWidth: true; visible: session.role === "idle"; text: "Подключиться"; onClicked: connectDialog.open() }
    C.ActionButton { Layout.fillWidth: true; visible: session.canReconnect; text: "Переподключиться"; glyph: "refresh"; primary: true; onClicked: session.reconnect() }
    Text { Layout.fillWidth: true; visible: session.status !== ""; text: session.status; color: C.Theme.muted; font.pixelSize: 11; wrapMode: Text.Wrap }
    Text { visible: session.role === "teacher"; text: "Порт: " + session.port; color: C.Theme.text; font.pixelSize: 14 }
    C.ActionButton { Layout.fillWidth: true; visible: session.role === "teacher"; text: "Завершить"; glyph: "stop"; onClicked: session.stop() }

    ListView {
        id: studentList
        visible: session.role === "teacher"
        Layout.fillWidth: true
        Layout.preferredHeight: panel.rosterHeight
        Layout.minimumHeight: panel.rosterHeight
        clip: true
        spacing: 3
        model: session.roster
        delegate: Rectangle {
            width: studentList.width
            height: 51
            radius: 6
            color: C.Theme.surface
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 4
                spacing: 4
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    spacing: 2
                    Text { Layout.fillWidth: true; text: studentName; color: C.Theme.text; font.pixelSize: 12; font.weight: Font.Medium; elide: Text.ElideRight }
                    Text { text: "ping " + studentPing + " мс"; color: C.Theme.muted; font.pixelSize: 10 }
                }
                C.ActionButton { glyph: "file"; subtle: true; implicitWidth: 32; hint: "Посмотреть и редактировать код"; onClicked: { session.watch(studentId); studentWindow.show(); studentWindow.raise(); } }
                C.ActionButton { glyph: "close"; subtle: true; implicitWidth: 32; hint: "Отключить ученика"; onClicked: session.kick(studentId) }
            }
        }
        ScrollBar.vertical: ScrollBar {}
    }

    RowLayout {
        visible: session.role === "student"
        Layout.fillWidth: true
        spacing: 4
        C.ActionButton { text: "Код"; subtle: panel.previewTab !== 0; implicitWidth: 72; onClicked: panel.previewTab = 0 }
        C.ActionButton { text: "Экран"; subtle: panel.previewTab !== 1; implicitWidth: 72; onClicked: panel.previewTab = 1 }
        Item { Layout.fillWidth: true }
        Text { text: session.remotePath.split(/[\\/]/).pop(); color: C.Theme.muted; font.pixelSize: 10; elide: Text.ElideMiddle; Layout.maximumWidth: 95 }
    }
    ScrollView {
        visible: session.role === "student" && panel.previewTab === 0 && !panel.detached
        Layout.fillWidth: true
        Layout.preferredHeight: 176
        clip: true
        TextArea {
            text: session.remoteText
            readOnly: true
            selectByMouse: true
            wrapMode: TextEdit.NoWrap
            color: C.Theme.text
            font.family: Qt.platform.os === "windows" ? "Consolas" : "DejaVu Sans Mono"
            font.pixelSize: 12
            background: Rectangle { color: C.Theme.editor }
        }
    }
    StableDemoView {
        visible: session.role === "student" && panel.previewTab === 1 && !panel.detached
        streaming: visible
        frame: session.frame
        Layout.fillWidth: true
        Layout.preferredHeight: 176
    }
    ColumnLayout {
        visible: session.role === "student"
        Layout.fillWidth: true
        spacing: 4
        C.ActionButton { Layout.fillWidth: true; text: "Вынести отдельным окном"; glyph: "open"; onClicked: { panel.detached = true; demo.show(); demo.raise(); } }
        C.ActionButton { Layout.fillWidth: true; text: "Скопировать код"; glyph: "copy"; enabled: session.remotePath !== ""; onClicked: backend.copyText(session.remoteText) }
        C.ActionButton { Layout.fillWidth: true; text: "Отключиться"; glyph: "close"; onClicked: session.stop() }
    }
    C.ActionButton { visible: session.role === "connecting"; text: "Отмена"; onClicked: session.stop() }

    C.Modal {
        id: connectDialog
        title: "Подключиться к учителю"
        contentItem: ColumnLayout {
            spacing: 10
            Text { text: "IP учителя"; color: C.Theme.text }
            C.Input { id: address; Layout.fillWidth: true; placeholderText: "192.168.1.12"; onAccepted: portInput.forceActiveFocus() }
            Text { text: "Порт"; color: C.Theme.text }
            C.Input { id: portInput; Layout.fillWidth: true; placeholderText: "Порт конференции"; validator: IntValidator { bottom: 1; top: 65535 } onAccepted: connectButton.clicked() }
            C.ActionButton { id: connectButton; text: "Подключиться"; enabled: address.text.trim() !== "" && portInput.acceptableInput; onClicked: { session.join(address.text, Number(portInput.text)); connectDialog.close(); } }
        }
    }

    Window {
        id: demo
        objectName: "teacherDemo"
        property bool demoViewer: true
        width: 1250; height: 800; minimumWidth: 520; minimumHeight: 330
        title: "Teacher Mode — демонстрация учителя"
        color: C.Theme.background
        onClosing: { panel.detached = false; visible = false; }
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 8
            spacing: 6
            RowLayout {
                Layout.fillWidth: true
                Text { text: session.remotePath.split(/[\\/]/).pop(); color: C.Theme.text; Layout.fillWidth: true; elide: Text.ElideMiddle }
                C.ActionButton { text: "−"; implicitWidth: 32; onClicked: demoView.zoom = Math.max(0.5, demoView.zoom / 1.25) }
                C.ActionButton { text: "По размеру"; onClicked: demoView.fit() }
                C.ActionButton { text: "100%"; onClicked: demoView.actualSize() }
                C.ActionButton { text: "+"; implicitWidth: 32; onClicked: demoView.zoom = Math.min(8, demoView.zoom * 1.25) }
                C.ActionButton { text: demo.visibility === Window.FullScreen ? "Окно" : "На весь экран"; onClicked: demo.visibility = demo.visibility === Window.FullScreen ? Window.Windowed : Window.FullScreen }
                C.ActionButton { text: "Скопировать код"; enabled: session.remotePath !== ""; onClicked: backend.copyText(session.remoteText) }
                C.ActionButton { text: "Вернуть в IDE"; onClicked: demo.close() }
                C.ActionButton { text: "Отключиться"; onClicked: session.stop() }
            }
            StableDemoView { id: demoView; Layout.fillWidth: true; Layout.fillHeight: true; streaming: demo.visible; zoomable: true; frame: session.frame }
        }
    }
    Window {
        id: studentWindow
        objectName: "teacherStudentCode"
        property bool demoViewer: true
        property bool syncing: false
        width: 950; height: 650; minimumWidth: 480; minimumHeight: 300
        title: "Teacher Mode — код ученика"
        color: C.Theme.background
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            Text { Layout.fillWidth: true; text: session.remotePath || "Ученик пока не открыл файл"; color: C.Theme.text; elide: Text.ElideMiddle }
            Text { Layout.fillWidth: true; text: session.status; color: C.Theme.muted; font.pixelSize: 11; wrapMode: Text.Wrap }
            ScrollView {
                Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                TextArea {
                    id: studentCode; color: C.Theme.text; selectionColor: C.Theme.accentSurface
                    font.family: Qt.platform.os === "windows" ? "Consolas" : "DejaVu Sans Mono"
                    font.pixelSize: 14; wrapMode: TextEdit.NoWrap; selectByMouse: true
                    readOnly: session.remotePath === "" || session.role !== "teacher"
                    background: Rectangle { color: C.Theme.editor }
                    onTextChanged: if (!studentWindow.syncing && studentWindow.visible && !readOnly) editDelay.restart()
                }
            }
        }
        Timer { id: editDelay; interval: 140; onTriggered: session.editRemote(studentCode.text) }
    }
    Connections {
        target: session
        function onUiCommand(name) {
            if (!panel.visible) return
            if (name === "detach") { panel.detached = true; demo.show() }
            if (name === "return") demo.close()
            if (name === "copy") backend.copyText(session.remoteText)
            if (name === "watchWindow") { studentWindow.show(); studentWindow.raise() }
        }
        function onChanged() {
            if (session.role !== "student") { demo.close(); panel.detached = false }
            if (session.role !== "teacher") studentWindow.close()
        }
        function onRemoteChanged() {
            if (session.role === "teacher") {
                editDelay.stop(); studentWindow.syncing = true
                var cursor = studentCode.cursorPosition
                studentCode.text = session.remoteText
                studentCode.cursorPosition = Math.min(cursor, studentCode.length)
                studentWindow.syncing = false
                if (session.selected === "") studentWindow.close()
            }
        }
    }
}

