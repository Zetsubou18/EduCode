import QtQuick 2.15
import QtQuick.Controls 2.15
CheckBox {
 id: control
 contentItem: Text { text: control.text; color: Theme.text; font.pixelSize: 13; leftPadding: 30; verticalAlignment: Text.AlignVCenter }
 indicator: Rectangle { width: 18; height: 18; y: (control.height-height)/2; radius: 4; color: control.checked ? Theme.accentSurface : Theme.editor; border.color: control.activeFocus ? Theme.accent : Theme.border
  Icon { anchors.centerIn: parent; name: "check"; size: 13; visible: control.checked }
 }
}
