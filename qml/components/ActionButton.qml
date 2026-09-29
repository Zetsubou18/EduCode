import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
Button {
 id: control
 property string glyph: ""
 property bool primary: false
 property bool subtle: false
 property string hint: ""
 property color ink: primary ? "#e9eff7" : Theme.text
 implicitHeight: 36
 implicitWidth: glyph !== "" && text === "" ? 36 : Math.max(90, contentItem.implicitWidth + 28)
 padding: text==="" ? 0 : 8
 hoverEnabled: true
 opacity: enabled ? 1 : 0.4
 background: Rectangle {
  radius: Theme.radius
  color: control.down ? Theme.pressed : control.hovered ? (control.primary ? "#495f7b" : Theme.hover) : control.primary ? Theme.accentSurface : control.subtle ? "transparent" : Theme.surface
  border.width: control.activeFocus ? 1 : 0
  border.color: Theme.accent
  Behavior on color { ColorAnimation { duration: Theme.duration } }
 }
 contentItem: Item {
  implicitWidth: contents.implicitWidth
  implicitHeight: contents.implicitHeight
  Row {
   id: contents; anchors.centerIn: parent; spacing: control.glyph!==""&&control.text!=="" ? 8 : 0
   Icon { visible: control.glyph!==""; name: control.glyph; color: control.ink; width: 18; height: 18; anchors.verticalCenter: parent.verticalCenter }
   Text { visible: control.text!==""; text: control.text; color: control.ink; font.family: Theme.font; font.pixelSize: Theme.fontSize; anchors.verticalCenter: parent.verticalCenter }
  }
 }
 ToolTip.visible: hovered && hint !== ""
 ToolTip.delay: 700
 ToolTip.text: hint
}
