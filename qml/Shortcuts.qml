import QtQuick 2.15
Item {
 property var bindings: backend.shortcuts
 Repeater { model: Object.keys(bindings); Item { Shortcut { sequence: bindings[modelData]; context: Qt.ApplicationShortcut; enabled: backend.page === "ide"; onActivated: backend.command(modelData) } } }
}
