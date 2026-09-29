import QtQuick 2.15
import QtQuick.Controls 2.15
ComboBox {
 id: control
 implicitHeight: 36
 font.family: Theme.font
 font.pixelSize: Theme.fontSize
 leftPadding: 12
 rightPadding: 28
 background: Rectangle { radius: Theme.radius; color: control.hovered ? Theme.hover : Theme.surface; border.color: control.activeFocus ? Theme.accent : Theme.border }
 contentItem: Text { text: control.displayText; color: Theme.text; font: control.font; verticalAlignment: Text.AlignVCenter; elide: Text.ElideMiddle }
 indicator: Icon { x: parent.width-24; y: (parent.height-height)/2; name: "down"; size: 14; color: Theme.muted }
 delegate: ItemDelegate {
  width: control.width
  text: control.textRole ? modelData[control.textRole] : modelData
  font: control.font
  contentItem: Text { text: parent.text; color: Theme.text; font: control.font; elide: Text.ElideMiddle; verticalAlignment: Text.AlignVCenter }
  background: Rectangle { color: highlighted ? Theme.hover : Theme.surface }
 }
 popup: Popup {
  y: control.height+4
  width: control.width
  padding: 4
  implicitHeight: Math.min(contentItem.implicitHeight+8,280)
  background: Rectangle { color: Theme.surface; radius: Theme.radius; border.color: Theme.border }
  contentItem: ListView { clip: true; implicitHeight: contentHeight; model: control.popup.visible ? control.delegateModel : null; currentIndex: control.highlightedIndex; ScrollIndicator.vertical: ScrollIndicator {} }
 }
}
