import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../components" as C
Rectangle {
 id: packagesPage
 color: C.Theme.background
 property var manager: backend.packages
 property string selectedVersion: manager.details.version || ""
 function installedVersion(name) { var key=String(name||"").toLowerCase(); for(var i=0;i<manager.items.length;i++){var item=manager.items[i]||{};if(item.installed&&String(item.name||"").toLowerCase()===key)return String(item.version||"");}return ""; }
 RowLayout {
  anchors.fill: parent; anchors.margins: 10; spacing: 10
  Rectangle {
   Layout.preferredWidth: 340; Layout.fillHeight: true; color: C.Theme.sidebar; radius: 6
   ColumnLayout { anchors.fill: parent; anchors.margins: 10; spacing: 8
    RowLayout { Layout.fillWidth: true
     C.Input { id: packageSearch; Layout.fillWidth: true; placeholderText: "Найти пакет на PyPI…"; selectByMouse: true; onTextChanged: manager.search(text) }
     C.ActionButton { visible: packageSearch.text!==""; glyph: "close"; implicitWidth: 34; hint: "Очистить поиск"; onClicked: {packageSearch.clear();packageSearch.forceActiveFocus();} }
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
   ScrollView { id: packageDetailsScroll; anchors.fill: parent; anchors.margins: 18; clip: true; contentWidth: availableWidth
    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
    ColumnLayout { width: packageDetailsScroll.availableWidth; spacing: 12
     Text { text: manager.details.name || "Выберите библиотеку"; color: C.Theme.text; font.pixelSize: 22; font.weight: Font.DemiBold }
     Text { visible: !!manager.details.version; text: "Версия "+(manager.details.version||"")+(manager.details.requiresPython ? " · Python "+manager.details.requiresPython : ""); color: C.Theme.muted; font.pixelSize: 12 }
     Text { text: manager.details.summary || "Слева показаны библиотеки виртуального окружения. Введите точное имя пакета, чтобы найти его на PyPI."; color: C.Theme.muted; font.pixelSize: 13; wrapMode: Text.Wrap; Layout.fillWidth: true }
     RowLayout { id: packageActions; visible: !!manager.details.name; Layout.fillWidth: true
      property string installedVersion: packagesPage.installedVersion(manager.details.name)
      property bool installed: installedVersion!==""
      property bool updateAvailable: installed && !!manager.details.version && installedVersion!==String(manager.details.version)
      C.ActionButton { visible: !parent.installed; text: "Установить"; primary: true; enabled: !manager.busy; onClicked: manager.install(manager.details.name,packagesPage.selectedVersion) }
      C.Select { visible: !parent.installed; Layout.preferredWidth: 170; model: manager.details.versions||[]; currentIndex: Math.max(0,model.indexOf(manager.details.version)); onActivated: packagesPage.selectedVersion=model[index]; Component.onCompleted: packagesPage.selectedVersion=manager.details.version||"" }
      C.ActionButton { visible: parent.updateAvailable; text: "Обновить"; primary: true; enabled: !manager.busy; onClicked: manager.update(manager.details.name) }
      Text { visible: parent.installed&&!parent.updateAvailable; text: "Установлена последняя версия"; color: C.Theme.green; font.pixelSize: 12 }
      C.ActionButton { visible: parent.installed; text: "Удалить"; ink: C.Theme.error; enabled: !manager.busy; onClicked: manager.remove(manager.details.name) }
      C.ActionButton { visible: !!manager.details.home; text: "Сайт"; subtle: true; onClicked: backend.openBrowser(manager.details.home) }
     }
     Rectangle { Layout.fillWidth: true; height: 1; color: C.Theme.border }
     Text {
      text: manager.details.description || ""
      textFormat: String(manager.details.descriptionContentType||"").toLowerCase().indexOf("markdown")>=0 ? Text.MarkdownText : Text.PlainText
      color: C.Theme.muted; linkColor: "#56a8f5"; font.pixelSize: 12; wrapMode: Text.Wrap; Layout.fillWidth: true
      onLinkActivated: backend.openBrowser(link)
     }
    }
   }
   BusyIndicator { anchors.centerIn: parent; running: manager.busy; visible: running }
  }
 }
 Connections { target: manager; function onChanged(){if(manager.details&&manager.details.version)packagesPage.selectedVersion=manager.details.version;} }
 Component.onCompleted: manager.refresh()
}
