import QtQuick 2.15
import QtQuick.Controls 2.15
MenuItem {
 id: item
 implicitHeight: 32
 leftPadding: 12; rightPadding: 12
 contentItem: Text { text: item.text; color: item.enabled ? Theme.text : Theme.faint; font.family: Theme.font; font.pixelSize: 13; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
 background: Rectangle { color: item.highlighted && item.enabled ? Theme.hover : "transparent"; radius: 3 }
}
