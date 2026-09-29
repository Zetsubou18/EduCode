import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../components" as C
Item {
 id: page
 signal createRequested()
 signal openRequested()
 Image { anchors.fill: parent; source: appBase+"assets/welcome_background.png"; fillMode: Image.PreserveAspectCrop; opacity: 0.45 }
 Rectangle { anchors.fill: parent; color: "#801b1c1e" }
 ColumnLayout {
  anchors.centerIn: parent
  width: Math.min(580,page.width-80)
  height: Math.min(700,page.height-40)
  spacing: 18
  ColumnLayout {
   Layout.fillWidth: true
   Layout.alignment: Qt.AlignHCenter
   spacing: 10
   Image { source: appBase+"assets/app_logo.png"; Layout.alignment: Qt.AlignHCenter; Layout.preferredWidth: 58; Layout.preferredHeight: 58; fillMode: Image.PreserveAspectFit }
   Text { text: "EduCode"; color: C.Theme.text; font.family: C.Theme.font; Layout.alignment: Qt.AlignHCenter; font.pixelSize: 40; font.weight: Font.DemiBold; font.letterSpacing: -1.5 }
   Text { text: backend.translate("«Большой путь начинается с первой строки.»",backend.configuration["general.language"]); color: "#b1b5bf"; font.family: C.Theme.font; horizontalAlignment: Text.AlignHCenter; font.pixelSize: 14; Layout.fillWidth: true; wrapMode: Text.WordWrap }
   Item { Layout.preferredHeight: 10 }
   RowLayout {
    Layout.alignment: Qt.AlignHCenter; spacing: 12
    C.ActionButton { text: backend.translate("Продолжить",backend.configuration["general.language"]); glyph: "play"; primary: true; implicitHeight: 42; enabled: backend.recentProjects.length>0; onClicked: backend.continueProject() }
    C.ActionButton { text: backend.translate("Создать новый",backend.configuration["general.language"]); glyph: "add"; implicitHeight: 42; onClicked: page.createRequested() }
   }
   C.ActionButton { Layout.alignment: Qt.AlignHCenter; text: backend.translate("Открыть папку проекта",backend.configuration["general.language"]); glyph: "folder"; subtle: true; ink: C.Theme.muted; onClicked: page.openRequested() }
   Item { Layout.preferredHeight: 0 }
   Text { Layout.alignment: Qt.AlignHCenter; text: "PYTHON IDE  /  0.1"; color: C.Theme.faint; font.pixelSize: 10; font.letterSpacing: 2 }
  }
  Rectangle {
   Layout.fillWidth: true
   Layout.fillHeight: true; Layout.minimumHeight: 200
   color: "#e6232427"
   radius: 12
   border.color: "#393b40"
   ColumnLayout {
    anchors.fill: parent; anchors.margins: 24; spacing: 18
    RowLayout {
     Text { text: backend.translate("Ваши проекты",backend.configuration["general.language"]); color: C.Theme.text; font.family: C.Theme.font; font.pixelSize: 18; font.weight: Font.DemiBold; Layout.fillWidth: true }
     C.ActionButton { glyph: "settings"; subtle: true; hint: backend.translate("Настройки",backend.configuration["general.language"]); onClicked: backend.settings() }
    }
    C.Input { id: search; Layout.fillWidth: true; placeholderText: backend.translate("Найти проект…",backend.configuration["general.language"]) }
    ListView {
     Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 4; model: backend.recentProjects
     delegate: Rectangle {
      width: ListView.view.width; height: visible ? 68 : 0
      visible: modelData.name.toLowerCase().indexOf(search.text.toLowerCase())>=0
      radius: 6; color: mouse.containsMouse ? C.Theme.hover : "transparent"
      RowLayout {
       anchors.fill: parent; anchors.margins: 10; spacing: 12
       Image { source: appBase+"assets/icons/folder.svg"; Layout.preferredWidth: 24; Layout.preferredHeight: 24 }
       ColumnLayout { Layout.fillWidth: true; spacing: 5
        Text { text: modelData.name; color: C.Theme.text; font.pixelSize: 14; font.weight: Font.Medium; Layout.fillWidth: true; elide: Text.ElideRight }
        Text { text: modelData.exists ? modelData.path : "Папка недоступна"; color: C.Theme.faint; font.pixelSize: 11; Layout.fillWidth: true; elide: Text.ElideMiddle }
       }
       C.Icon { name: "chevron"; size: 14; color: C.Theme.faint }
      }
      MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: backend.openProject(modelData.path) }
     }
     Text { anchors.centerIn: parent; visible: backend.recentProjects.length===0; text: backend.translate("Здесь появятся ваши проекты.\nСоздайте первый — и начните писать.",backend.configuration["general.language"]); color: C.Theme.faint; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; lineHeight: 1.6 }
     ScrollBar.vertical: ScrollBar {}
    }
    Rectangle { Layout.fillWidth: true; height: 1; color: C.Theme.border }
    TeacherPanel { Layout.fillHeight: false; Layout.preferredHeight: panelHeight; Layout.minimumHeight: panelHeight; Layout.fillWidth: true }
    Text { text: backend.translate("Среда настроится автоматически.",backend.configuration["general.language"]); color: C.Theme.faint; font.pixelSize: 11 }
   }
  }
 }
}
