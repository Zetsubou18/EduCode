import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../components" as C
Rectangle {
 color: C.Theme.background
 property var manager: backend.packages
 RowLayout {
  anchors.fill: parent; anchors.margins: 10; spacing: 10
  Rectangle {
   Layout.preferredWidth: 340; Layout.fillHeight: true; color: C.Theme.sidebar; radius: 6
   ColumnLayout { anchors.fill: parent; anchors.margins: 10; spacing: 8
    RowLayout { Layout.fillWidth: true
     C.Input { id: packageSearch; Layout.fillWidth: true; placeholderText: "Найти пакет на PyPI…"; onTextChanged: manager.search(text) }
     C.ActionButton { glyph: "refresh"; implicitWidth: 34; hint: "Обновить список"; enabled: !manager.busy; onClicked: manager.refresh() }
    }
    Text { text: manager.status; color: C.Theme.faint; font.pixelSize: 11 }
    ListView {
     id: packageList; Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 3; model: manager.items
     delegate: Rectangle {
      width: packageList.width; height: 48; radius: 5; color: packageMouse.containsMouse ? C.Theme.hover : "transparent"
      Column { anchors.left: parent.left; anchors.leftMargin: 10; anchors.verticalCenter: parent.verticalCenter; width: parent.width-20; spacing: 3
       Text { text: modelData.name; color: C.Theme.text; font.pixelSize: 13; elide: Text.ElideRight; width: parent.width }
       Text { text: modelData.installed ? modelData.version+(modelData.latest ? "  →  "+modelData.latest : "") : (modelData.version||"")+" · PyPI"; color: modelData.latest ? "#d2b080" : C.Theme.faint; font.pixelSize: 10 }
      }
      MouseArea { id: packageMouse; anchors.fill: parent; hoverEnabled: true; onClicked: manager.select(modelData.name) }
     }
     ScrollBar.vertical: ScrollBar {}
    }
   }
  }
  Rectangle {
   Layout.fillWidth: true; Layout.fillHeight: true; color: C.Theme.editor; radius: 6
   ScrollView { anchors.fill: parent; anchors.margins: 18; clip: true
    ColumnLayout { width: Math.max(300,parent.width); spacing: 12
     Text { text: manager.details.name || "Выберите библиотеку"; color: C.Theme.text; font.pixelSize: 22; font.weight: Font.DemiBold }
     Text { visible: !!manager.details.version; text: "Версия "+(manager.details.version||"")+(manager.details.requiresPython ? " · Python "+manager.details.requiresPython : ""); color: C.Theme.muted; font.pixelSize: 12 }
     Text { text: manager.details.summary || "Слева показаны библиотеки виртуального окружения. Введите точное имя пакета, чтобы найти его на PyPI."; color: C.Theme.muted; font.pixelSize: 13; wrapMode: Text.Wrap; Layout.fillWidth: true }
     RowLayout { visible: !!manager.details.name
      property bool installed: {for(var i=0;i<manager.items.length;i++)if(manager.items[i].installed&&manager.items[i].name.toLowerCase()===manager.details.name.toLowerCase())return true;return false;}
      C.ActionButton { text: parent.installed ? "Обновить" : "Установить"; primary: true; enabled: !manager.busy; onClicked: parent.installed ? manager.update(manager.details.name) : manager.install(manager.details.name) }
      C.ActionButton { visible: parent.installed; text: "Удалить"; ink: C.Theme.error; enabled: !manager.busy; onClicked: manager.remove(manager.details.name) }
      C.ActionButton { visible: !!manager.details.home; text: "Сайт"; subtle: true; onClicked: backend.openBrowser(manager.details.home) }
     }
     Rectangle { Layout.fillWidth: true; height: 1; color: C.Theme.border }
     Text { text: manager.details.description || ""; textFormat: Text.MarkdownText; color: C.Theme.muted; linkColor: C.Theme.accent; font.pixelSize: 12; wrapMode: Text.Wrap; Layout.fillWidth: true; onLinkActivated: backend.openBrowser(link) }
    }
   }
   BusyIndicator { anchors.centerIn: parent; running: manager.busy; visible: running }
  }
 }
 Component.onCompleted: manager.refresh()
}
