import QtQuick 2.15
import QtQuick.Controls 2.15
Dialog {
 id: popup
 parent: Overlay.overlay
 modal: true
 anchors.centerIn: parent
 width: 460
 padding: 24
 closePolicy: Popup.CloseOnEscape
 background: Rectangle { radius: 12; color: Theme.sidebar; border.color: "#45484d" }
 header: Text { text: popup.title; color: Theme.text; font.family: Theme.font; font.pixelSize: 19; font.weight: Font.DemiBold; padding: 24; bottomPadding: 4 }
 Overlay.modal: Rectangle { color: "#99000000" }
}
