import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../components" as C
Rectangle {
 id: ide; objectName: "idePage"
 color: C.Theme.background
 property bool explorerOpen: true
 property real explorerExtent: 246
 property real savedExplorerWidth: 246
 property real widthBeforeTeacher: 246
 property bool teacherExpandedExplorer: false
 Behavior on explorerExtent { NumberAnimation { duration: 180; easing.type: Easing.InOutCubic } }
 function toggleExplorer(){if(explorerOpen){savedExplorerWidth=explorer.width;explorerExtent=0;}else explorerExtent=Math.max(175,savedExplorerWidth);explorerOpen=!explorerOpen;}
 property int bottomTab: 0
 property bool terminalLoaded: false
 property string contextPath: ""
 property var pluginMenuItems: {var rows=[];backend.plugins.forEach(function(p){if(p.enabled)(p.contextMenu||[]).forEach(function(item){rows.push({label:item.label,command:item.command,id:p.id});});});return rows;}
 signal fileActionRequested(string operation,string path)
 ColumnLayout {
  anchors.fill: parent; spacing: 0
  Rectangle {
   Layout.fillWidth: true; Layout.preferredHeight: 56; color: C.Theme.sidebar
   RowLayout {
    anchors.fill: parent; anchors.leftMargin: 16; anchors.rightMargin: 16; spacing: 8
    C.ActionButton { glyph: "home"; subtle: true; hint: backend.translate("Главная",backend.configuration["general.language"]); onClicked: backend.home() }
    C.ActionButton { glyph: "folder"; subtle: true; hint: backend.translate("Скрыть / показать проводник",backend.configuration["general.language"]); onClicked: ide.toggleExplorer() }
    Rectangle { width: 1; height: 22; color: C.Theme.border }
    C.ActionButton { glyph: "back"; subtle: true; hint: backend.translate("Назад к позиции · Alt+←",backend.configuration["general.language"]); onClicked: backend.navigateBack() }
    Item { Layout.fillWidth: true }
    Text { text: backend.projectName; color: C.Theme.text; font.pixelSize: 14; font.weight: Font.Medium }
    Item { Layout.fillWidth: true }
    C.Select {
     id: targetSelect; Layout.preferredWidth: 190; textRole: "name"
     model: [{name:backend.translate("Текущий файл",backend.configuration["general.language"]),path:""}].concat(backend.runTargets)
     currentIndex: {for(var i=0;i<model.length;i++)if(model[i].path===backend.runTarget)return i;return 0;}
     onActivated: backend.setRunTarget(model[index].path)
    }
    C.ActionButton { glyph: backend.running ? "stop" : "play"; ink: backend.running ? C.Theme.error : C.Theme.green; subtle: true; hint: backend.running ? "Остановить" : "Запустить · F5"; onClicked: { if(backend.running)backend.stop();else {ide.bottomTab=0;backend.run();} } }
    C.ActionButton { glyph: "search"; subtle: true; hint: backend.translate("Поиск в проекте и библиотеках · Ctrl+P",backend.configuration["general.language"]); onClicked: backend.command("quickOpen") }
    C.ActionButton { glyph: "file"; subtle: true; hint: backend.translate("Разделить редактор · Ctrl+Alt+S",backend.configuration["general.language"]); onClicked: backend.command("split") }
    C.ActionButton { glyph: "settings"; subtle: true; hint: backend.translate("Настройки",backend.configuration["general.language"]); onClicked: backend.settings() }
   }
  }
  Rectangle { Layout.fillWidth: true; height: 1; color: C.Theme.border }
  Item {
   Layout.fillWidth: true; Layout.fillHeight: true
   RightDock { id: rightDock; anchors.fill: parent; z: 2 }
  SplitView {
   anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom; width: parent.width-rightDock.occupiedWidth; orientation: Qt.Horizontal
   handle: Rectangle { implicitWidth: ide.explorerOpen ? 6 : 0; color: SplitHandle.hovered ? C.Theme.hover : C.Theme.sidebar; Rectangle { anchors.centerIn: parent; width: 1; height: parent.height; color: C.Theme.border } }
   Rectangle {
    id: explorer; clip: true
    SplitView.preferredWidth: ide.explorerExtent; SplitView.minimumWidth: 0; SplitView.maximumWidth: 440; color: C.Theme.sidebar
    Connections { target: ide; function onExplorerExtentChanged(){explorer.SplitView.preferredWidth=ide.explorerExtent;} }
    ColumnLayout {
     anchors.fill: parent; anchors.margins: 12; spacing: 10
     RowLayout {
      Text { text: backend.translate("ПРОЕКТ",backend.configuration["general.language"]); color: C.Theme.muted; font.pixelSize: 10; font.letterSpacing: 1.4; Layout.fillWidth: true }
      C.ActionButton { glyph: "add"; subtle: true; implicitHeight: 28; implicitWidth: 28; hint: backend.translate("Создать файл",backend.configuration["general.language"]); onClicked: ide.fileActionRequested("newFile",backend.projectPath) }
      C.ActionButton { glyph: "more"; subtle: true; implicitHeight: 28; implicitWidth: 28; onClicked: {ide.contextPath=backend.projectPath;fileMenu.popup(parent,0,parent.height);} }
     }
     C.Input { id: search; Layout.fillWidth: true; placeholderText: backend.translate("Поиск файлов",backend.configuration["general.language"]); implicitHeight: 32; font.pixelSize: 12; onTextChanged: backend.filterFiles(text) }
     Item {
      Layout.fillWidth: true; Layout.fillHeight: true
      MouseArea { anchors.fill: parent; acceptedButtons: Qt.RightButton; onClicked: {ide.contextPath=backend.projectPath;fileMenu.popup(this,mouse.x,mouse.y);} }
     ListView {
      id: tree; anchors.fill: parent; clip: true; model: backend.files
      delegate: Rectangle {
       width: tree.width; height: 31; radius: 5
       color: backend.activePath===modelData.path ? C.Theme.accentSurface : rowMouse.containsMouse ? C.Theme.hover : "transparent"
       RowLayout {
        anchors.fill: parent; anchors.leftMargin: modelData.depth*14+4; anchors.rightMargin: 6; spacing: 6
        C.Icon { name: modelData.expanded ? "down" : "chevron"; visible: modelData.directory; size: 11; color: C.Theme.muted; Layout.preferredWidth: 10 }
        Image { source: appBase+"assets/icons/"+(modelData.directory ? "folder" : ["py","json","md"].indexOf(modelData.extension)>=0 ? (modelData.extension==="py" ? "python" : modelData.extension==="md" ? "markdown" : "json") : "file")+".svg"; visible: modelData.directory||["py","json","md"].indexOf(modelData.extension)>=0; Layout.preferredWidth: 17; Layout.preferredHeight: 17 }
        C.Icon { visible: !modelData.directory&&["py","json","md"].indexOf(modelData.extension)<0; name: "file"; size: 16; color: C.Theme.muted }
        Text { text: modelData.name; color: C.Theme.text; font.pixelSize: 12; Layout.fillWidth: true; elide: Text.ElideRight }
       }
       MouseArea { id: rowMouse; anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.LeftButton|Qt.RightButton
        onClicked: { if(mouse.button===Qt.RightButton){ide.contextPath=modelData.path;fileMenu.popup(rowMouse,mouse.x,mouse.y);}else if(modelData.directory)backend.toggleFolder(modelData.path);else backend.openFile(modelData.path); }
       }
      }
      ScrollBar.vertical: ScrollBar {}
     }
     }
     Rectangle { Layout.fillWidth: true; height: 1; color: C.Theme.border }
     TeacherPanel { Layout.fillHeight: false; Layout.preferredHeight: panelHeight; Layout.minimumHeight: panelHeight; Layout.fillWidth: true; Layout.maximumHeight: ide.height*0.65 }
    }
   }
   SplitView {
    SplitView.fillWidth: true; orientation: Qt.Vertical
    handle: Rectangle { implicitHeight: 8; color: SplitHandle.hovered || SplitHandle.pressed ? C.Theme.hover : C.Theme.background; Rectangle { anchors.centerIn: parent; width: parent.width; height: 1; color: SplitHandle.hovered ? C.Theme.accent : C.Theme.border } }
    Item {
     SplitView.fillHeight: true; SplitView.minimumHeight: 180
     ColumnLayout {
      anchors.fill: parent; spacing: 0
      Rectangle {
       Layout.fillWidth: true; Layout.preferredHeight: 40; color: C.Theme.background
       ListView {
        anchors.fill: parent; orientation: ListView.Horizontal; model: backend.tabs; clip: true
        delegate: Rectangle {
         width: Math.max(130,tabName.implicitWidth+70); height: 40
         color: modelData.path===backend.activePath ? C.Theme.editor : tabMouse.containsMouse ? C.Theme.surface : "transparent"
         Rectangle { height: 2; width: parent.width; color: C.Theme.accent; visible: modelData.path===backend.activePath }
         RowLayout {
          anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 7; spacing: 8
          Image { visible: /\.(py|json|md)$/i.test(modelData.name); source: appBase+"assets/icons/"+(modelData.name.endsWith(".py") ? "python" : modelData.name.endsWith(".md") ? "markdown" : "json")+".svg"; Layout.preferredWidth: 15; Layout.preferredHeight: 15 }
          Text { id: tabName; text: modelData.name; color: modelData.path===backend.activePath ? C.Theme.text : C.Theme.muted; font.pixelSize: 12; Layout.fillWidth: true }
          Text { visible: modelData.modified; text: "●"; color: C.Theme.accent; font.pixelSize: 8 }
          C.ActionButton { glyph: "close"; implicitWidth: 24; implicitHeight: 24; subtle: true; hint: backend.translate("Закрыть",backend.configuration["general.language"]); onClicked: backend.closeTab(modelData.path); z: 2 }
         }
         MouseArea { id: tabMouse; anchors.fill: parent; hoverEnabled: true; onClicked: backend.openFile(modelData.path); z: -1 }
        }
       }
      }
      C.EmbeddedView { id: code; Layout.fillWidth: true; Layout.fillHeight: true; resource: "editor.html?lang="+backend.configuration["general.language"]; visible: backend.tabs.length>0 }
      Item { Layout.fillWidth: true; Layout.fillHeight: true; visible: backend.tabs.length===0
       Column { anchors.centerIn: parent; spacing: 16
        Text { text: backend.translate("Начните с файла",backend.configuration["general.language"]); color: C.Theme.muted; font.pixelSize: 23; anchors.horizontalCenter: parent.horizontalCenter }
        Text { text: backend.translate("Откройте файл в дереве проекта\nили используйте Ctrl+P",backend.configuration["general.language"]); color: C.Theme.faint; font.pixelSize: 13; horizontalAlignment: Text.AlignHCenter; anchors.horizontalCenter: parent.horizontalCenter }
       }
      }
     }
    }
    Rectangle {
     SplitView.preferredHeight: 235; SplitView.minimumHeight: 70; color: C.Theme.background
     ColumnLayout {
      anchors.fill: parent; spacing: 0
      Rectangle {
       Layout.fillWidth: true; Layout.preferredHeight: 40; color: C.Theme.sidebar
       RowLayout {
        anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 12; spacing: 4
        Repeater { model: [{label:"Консоль Python",icon:"console"},{label:"Терминал",icon:"terminal"},{label:"Проблемы",icon:"problems"},{label:"Библиотеки",icon:"packages"}].map(function(item){item.label=backend.translate(item.label,backend.configuration["general.language"]);return item;})
         C.ActionButton { text: modelData.label+(index===2&&backend.problems.length>0 ? "  "+backend.problems.length : ""); glyph: modelData.icon; subtle: true; ink: ide.bottomTab===index ? C.Theme.text : C.Theme.faint; implicitHeight: 30; onClicked: {ide.bottomTab=index;if(index===1)ide.terminalLoaded=true;} }
        }
        Item { Layout.fillWidth: true }
        C.ActionButton { visible: backend.configuration["ai.askButtons"]&&(ide.bottomTab===2&&backend.problems.length>0||ide.bottomTab===0&&backend.consoleHasError); text: backend.translate("Спросить Лиру",backend.configuration["general.language"]); glyph: "ai"; implicitHeight: 30; onClicked: backend.askAi(ide.bottomTab===2 ? "Разбери текущие проблемы редактора, найди причину и предложи или внеси исправление." : "В консоли Python появилась ошибка. Прочитай консоль и связанный код, найди причину и исправь её.") }
        C.ActionButton { glyph: "save"; implicitHeight: 30; implicitWidth: 30; subtle: true; hint: backend.translate("Сохранить всё · Ctrl+Shift+S",backend.configuration["general.language"]); onClicked: backend.saveAll() }
       }
      }
      Item {
       Layout.fillWidth: true; Layout.fillHeight: true
       C.EmbeddedView { anchors.fill: parent; resource: "terminal.html?console"; visible: ide.bottomTab===0 }
       Loader { anchors.fill: parent; active: ide.terminalLoaded; visible: ide.bottomTab===1; sourceComponent: C.EmbeddedView { resource: "terminal.html" } }
       ListView {
        anchors.fill: parent; anchors.margins: 12; visible: ide.bottomTab===2; model: backend.problems; clip: true
        delegate: Rectangle {
         width: ListView.view.width; height: 36; radius: 4; color: problemMouse.containsMouse ? C.Theme.hover : "transparent"
         RowLayout { anchors.fill: parent; anchors.margins: 8; spacing: 12
          Text { text: modelData.severity===1 ? "●" : "▲"; color: modelData.severity===1 ? C.Theme.error : "#d2b080"; font.pixelSize: 12 }
          Text { text: modelData.message; color: C.Theme.text; font.pixelSize: 12; Layout.fillWidth: true; elide: Text.ElideRight }
          Text { text: modelData.path.split("/").pop()+":"+modelData.line+":"+modelData.column; color: C.Theme.faint; font.pixelSize: 11 }
         }
         MouseArea { id: problemMouse; anchors.fill: parent; hoverEnabled: true; onDoubleClicked: backend.openFile(modelData.path,modelData.line,modelData.column) }
        }
        Text { anchors.centerIn: parent; text: backend.translate("Проблем в текущем файле не найдено",backend.configuration["general.language"]); color: C.Theme.faint; visible: backend.problems.length===0; font.pixelSize: 12 }
        ScrollBar.vertical: ScrollBar {}
       }
       Loader { anchors.fill: parent; active: ide.bottomTab===3; visible: ide.bottomTab===3; sourceComponent: Packages {} }
      }
     }
    }
   }
  }
  }
  Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 26; color: "#232427"
   RowLayout { anchors.fill: parent; anchors.leftMargin: 14; anchors.rightMargin: 14
    Text { text: backend.status; color: C.Theme.muted; font.pixelSize: 10 }
    Item { Layout.fillWidth: true }
    Text { text: "UTF-8     Python · .venv"; color: C.Theme.faint; font.pixelSize: 10 }
   }
  }
 }
 Menu {
  id: fileMenu
  z: 1000
  width: 240
  background: Rectangle { color: C.Theme.surface; radius: 7; border.color: C.Theme.border }
  padding: 4
  C.ContextMenuItem { text: backend.translate("Новый файл…",backend.configuration["general.language"]); onTriggered: ide.fileActionRequested("newFile",ide.contextPath) }
  C.ContextMenuItem { text: backend.translate("Новая папка…",backend.configuration["general.language"]); onTriggered: ide.fileActionRequested("newFolder",ide.contextPath) }
  C.ContextMenuSeparator {}
  C.ContextMenuItem { enabled: ide.contextPath!==backend.projectPath; text: backend.translate("Переименовать…",backend.configuration["general.language"]); onTriggered: ide.fileActionRequested("rename",ide.contextPath) }
  C.ContextMenuItem { enabled: ide.contextPath!==backend.projectPath; text: backend.translate("Переместить в корзину…",backend.configuration["general.language"]); onTriggered: ide.fileActionRequested("delete",ide.contextPath) }
  C.ContextMenuSeparator {}
  C.ContextMenuItem { text: backend.translate("Копировать",backend.configuration["general.language"]); onTriggered: backend.fileOperation("copy",ide.contextPath) }
  C.ContextMenuItem { enabled: ide.contextPath!==backend.projectPath; text: backend.translate("Вырезать",backend.configuration["general.language"]); onTriggered: backend.fileOperation("cut",ide.contextPath) }
  C.ContextMenuItem { text: backend.translate("Вставить",backend.configuration["general.language"]); onTriggered: backend.fileOperation("paste",ide.contextPath) }
  C.ContextMenuSeparator {}
  C.ContextMenuItem { text: backend.translate("Показать в проводнике",backend.configuration["general.language"]); onTriggered: backend.fileOperation("reveal",ide.contextPath) }
  C.ContextMenuSeparator {}
  Repeater { model: ide.pluginMenuItems
   C.ContextMenuItem { text: modelData.label; onTriggered: backend.pluginAction(modelData.id,modelData.command) }
  }
 }
 Connections { target: backend.teacher; function onChanged(){
   if(backend.teacher.role==="teacher" && !ide.teacherExpandedExplorer && ide.explorerOpen && explorer.width<300){
    ide.widthBeforeTeacher=explorer.width; ide.teacherExpandedExplorer=true; ide.explorerExtent=320;
   }else if(backend.teacher.role==="idle" && ide.teacherExpandedExplorer){
    if(Math.abs(explorer.width-320)<15)ide.explorerExtent=ide.widthBeforeTeacher;
    ide.teacherExpandedExplorer=false;
   }
 } }
 Connections { target: backend; function onStateChanged(){if(backend.running)ide.bottomTab=0;} function onEditorCommand(name){if(name==="toggleExplorer")ide.toggleExplorer();} function onPanelRequested(index){ide.bottomTab=index;if(index===1)ide.terminalLoaded=true;} }
}
