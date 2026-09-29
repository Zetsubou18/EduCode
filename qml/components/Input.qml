import QtQuick 2.15
import QtQuick.Controls 2.15
TextField {
 id: field
 implicitHeight: 38
 color: Theme.text
 placeholderTextColor: Theme.faint
 font.family: Theme.font
 font.pixelSize: Theme.fontSize
 selectionColor: Theme.accentSurface
 selectedTextColor: Theme.text
 leftPadding: 12
 rightPadding: 12
 background: Rectangle { radius: Theme.radius; color: "#1f2022"; border.color: field.activeFocus ? Theme.accent : Theme.border; border.width: 1 }
}
