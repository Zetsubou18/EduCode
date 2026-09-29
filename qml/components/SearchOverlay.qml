import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
Popup {
 id: popup
 parent: Overlay.overlay
 width: Math.min(660,parent.width-80); height: Math.min(440,parent.height-120)
 x: (parent.width-width)/2; y: 72
 modal: true; dim: false; padding: 16
 closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
 background: Rectangle { color: Theme.surface; radius: 10; border.color: Theme.border }
 property var combined: backend.searchResults.concat(backend.pluginSearchResults(query.text))
 function choose(index){if(index<0||index>=combined.length)return;var result=combined[index];if(result.pluginId)backend.pluginAction(result.pluginId,result.pluginCommand);else if(result.value)backend.copyText(result.value);else if(result.revealPath)backend.fileOperation("reveal",result.revealPath);else if(result.path)backend.openFile(result.path,result.line||0,result.column||0);close();}
 onOpened: { query.text="";backend.searchQuery("");query.forceActiveFocus(); }
 contentItem: ColumnLayout {
  spacing: 10
  RowLayout {
   Layout.fillWidth: true
   Icon { name: "search"; color: Theme.muted }
   Input { id: query; Layout.fillWidth: true; placeholderText: backend.translate("Файл, фрагмент кода или выражение: (12 + 8) / 4",backend.configuration["general.language"]); onTextChanged: {list.currentIndex=0;backend.searchQuery(text);} onAccepted: popup.choose(list.currentIndex)
    Keys.onDownPressed: list.currentIndex=Math.min(list.count-1,list.currentIndex+1)
    Keys.onUpPressed: list.currentIndex=Math.max(0,list.currentIndex-1)
   }
   ActionButton { glyph: "close"; subtle: true; onClicked: popup.close() }
  }
  ListView {
   id: list; Layout.fillWidth: true; Layout.fillHeight: true; clip: true; model: popup.combined; currentIndex: 0
   delegate: Item {
    property bool firstInGroup: index===0||popup.combined[index-1].group!==modelData.group
    width: list.width; height: firstInGroup ? 76 : 48
    Text { visible: firstInGroup; text: modelData.group; height: 28; color: Theme.muted; font.pixelSize: 11; verticalAlignment: Text.AlignVCenter }
    Rectangle {
     anchors.bottom: parent.bottom; width: parent.width; height: 48; radius: 5
     color: parent.ListView.isCurrentItem||mouse.containsMouse ? Theme.hover : "transparent"
     Column { anchors.fill: parent; anchors.margins: 7; spacing: 3
      Text { width: parent.width; text: modelData.title; color: Theme.text; font.pixelSize: 13; elide: Text.ElideRight }
      Text { width: parent.width; text: modelData.detail; color: Theme.faint; font.pixelSize: 10; elide: Text.ElideMiddle }
     }
     MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: true; onClicked: popup.choose(index) }
    }
   }
   Text { anchors.centerIn: parent; visible: list.count===0; text: backend.translate("Ничего не найдено",backend.configuration["general.language"]); color: Theme.faint; font.pixelSize: 13 }
   ScrollBar.vertical: ScrollBar {}
  }
  Text { text: backend.translate("↑ ↓ выбрать    Enter открыть / скопировать    Esc закрыть",backend.configuration["general.language"]); color: Theme.faint; font.pixelSize: 10 }
 }
}
