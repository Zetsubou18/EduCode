import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Dialogs 1.3 as Native
import "components" as C
import "pages" as Pages
ApplicationWindow {
 id: window
 width: 1280; height: 820; minimumWidth: 960; minimumHeight: 620
 visible: true
 title: backend.projectName ? backend.projectName+" — EduCode" : "EduCode"
 color: C.Theme.background
 font.family: C.Theme.font
 property bool approved: false
 onClosing: { if(!approved){close.accepted=false;backend.requestQuit();} }
 FontLoader { id: iconFont; source: appBase+"node_modules/@vscode/codicons/dist/codicon.ttf" }
 Shortcuts { anchors.fill: parent }
 Pages.Welcome { anchors.fill: parent; visible: backend.page==="welcome"; onCreateRequested: {createDialog.existingPath="";createDialog.open();} onOpenRequested: projectPicker.open() }
 Loader { anchors.fill: parent; active: backend.projectPath!==""; visible: backend.page==="ide"; sourceComponent: Pages.Ide { onFileActionRequested: {fileDialog.operation=operation;fileDialog.path=path;fileDialog.open();} } }
 Pages.Settings { anchors.fill: parent; visible: backend.page==="settings" }
 Native.FileDialog { id: projectPicker; title: backend.translate("Открыть проект",backend.configuration["general.language"]); selectFolder: true; onAccepted: backend.openProject(fileUrl.toString()) }
 Native.FileDialog { id: locationPicker; title: backend.translate("Где создать проект?",backend.configuration["general.language"]); selectFolder: true; onAccepted: locationInput.text=decodeURIComponent(fileUrl.toString().replace(Qt.platform.os==="windows" ? "file:///" : "file://","")) }
 Native.FileDialog { id: pythonPicker; title: backend.translate("Выберите Python executable",backend.configuration["general.language"]); nameFilters: Qt.platform.os==="windows" ? ["Python executable (*.exe)"] : ["Все файлы (*)"]; onAccepted: backend.addPython(fileUrl.toString()) }
 C.Modal {
  id: createDialog
  property string existingPath: ""
  title: existingPath==="" ? "Новый проект" : "Подготовить среду проекта"
  closePolicy: backend.busy ? Popup.NoAutoClose : Popup.CloseOnEscape
  onOpened: nameInput.forceActiveFocus()
  contentItem: ColumnLayout {
   spacing: 14
   Text { text: createDialog.existingPath==="" ? "Название проекта" : createDialog.existingPath; color: C.Theme.muted; font.pixelSize: 12; Layout.fillWidth: true; elide: Text.ElideMiddle }
   C.Input { id: nameInput; Layout.fillWidth: true; placeholderText: backend.translate("Мой первый проект",backend.configuration["general.language"]); visible: createDialog.existingPath===""; enabled: !backend.busy }
   Text { visible: createDialog.existingPath===""; text: backend.translate("Расположение проекта",backend.configuration["general.language"]); color: C.Theme.muted; font.pixelSize: 12 }
   RowLayout { visible: createDialog.existingPath===""; Layout.fillWidth: true
    C.Input { id: locationInput; text: backend.projectsDirectory; Layout.fillWidth: true; enabled: !backend.busy }
    C.ActionButton { glyph: "folder"; hint: backend.translate("Выбрать папку",backend.configuration["general.language"]); enabled: !backend.busy; onClicked: locationPicker.open() }
   }
   Text { text: backend.translate("Версия Python",backend.configuration["general.language"]); color: C.Theme.muted; font.pixelSize: 12 }
   C.Select { id: pythonSelect; model: backend.pythonVersions; textRole: "label"; Layout.fillWidth: true; enabled: !backend.busy }
   C.ActionButton { text: backend.translate("Указать путь к Python…",backend.configuration["general.language"]); glyph: "folder"; subtle: true; ink: C.Theme.muted; enabled: !backend.busy; onClicked: pythonPicker.open() }
   Text { text: backend.busy ? "Создаём .venv и подготавливаем проект…" : backend.pythonVersions.length===0 ? "Python не найден. Укажите установленный интерпретатор." : "Виртуальное окружение будет создано автоматически."; color: C.Theme.faint; font.pixelSize: 12; Layout.fillWidth: true; wrapMode: Text.WordWrap }
   Text { text: createDialog.existingPath==="" ? backend.projectsDirectory : ""; color: C.Theme.faint; font.pixelSize: 10; Layout.fillWidth: true; elide: Text.ElideMiddle }
   RowLayout {
    Layout.topMargin: 8; spacing: 10
    Item { Layout.fillWidth: true }
    C.ActionButton { text: backend.translate("Отмена",backend.configuration["general.language"]); subtle: true; enabled: !backend.busy; onClicked: createDialog.close() }
    C.ActionButton { text: backend.busy ? "Подготовка…" : "Создать"; primary: true; enabled: !backend.busy&&backend.pythonVersions.length>0&&(nameInput.text.trim()!==""||createDialog.existingPath!==""); onClicked: {var py=backend.pythonVersions[pythonSelect.currentIndex].path;if(createDialog.existingPath!=="")backend.prepareEnvironment(createDialog.existingPath,py);else backend.createProject(nameInput.text.trim(),py,locationInput.text.trim());} }
   }
  }
 }
 C.Modal {
  id: unsavedDialog; title: backend.translate("Сохранить изменения?",backend.configuration["general.language"]); closePolicy: Popup.NoAutoClose
  contentItem: ColumnLayout {
   spacing: 22
   Text { text: backend.translate("Есть несохранённые файлы. Выберите,\nчто сделать перед продолжением.",backend.configuration["general.language"]); color: C.Theme.muted; font.pixelSize: 13; lineHeight: 1.5 }
   RowLayout {
    C.ActionButton { text: backend.translate("Отмена",backend.configuration["general.language"]); subtle: true; onClicked: {backend.resolveUnsaved("cancel");unsavedDialog.close();} }
    Item { Layout.fillWidth: true }
    C.ActionButton { text: backend.translate("Не сохранять",backend.configuration["general.language"]); onClicked: {backend.resolveUnsaved("discard");unsavedDialog.close();} }
    C.ActionButton { text: backend.translate("Сохранить",backend.configuration["general.language"]); primary: true; onClicked: {backend.resolveUnsaved("save");unsavedDialog.close();} }
   }
  }
 }
 C.Modal {
  id: fileDialog
  property string operation: ""
  property string path: ""
  title: operation==="newFile" ? "Новый файл" : operation==="newFolder" ? "Новая папка" : operation==="delete" ? "Переместить в корзину?" : "Переименовать"
  onOpened: {fileName.text=operation==="rename" ? path.split("/").pop() : "";fileName.forceActiveFocus();}
  contentItem: ColumnLayout {
   spacing: 14
   Text { text: fileDialog.path; color: C.Theme.faint; font.pixelSize: 11; Layout.fillWidth: true; elide: Text.ElideMiddle }
   C.Select { id: fileType; visible: fileDialog.operation==="newFile"; Layout.fillWidth: true; model: ["Python (.py)","JSON (.json)","Markdown (.md)","Текст (.txt)","Окружение (.env)","Git ignore (.gitignore)","Зависимости (requirements.txt)","Любое расширение"]; currentIndex: 0 }
   C.Input { id: fileName; visible: fileDialog.operation!=="delete"; Layout.fillWidth: true; placeholderText: backend.translate("Название",backend.configuration["general.language"]) }
   RowLayout {
    Item { Layout.fillWidth: true }
    C.ActionButton { text: backend.translate("Отмена",backend.configuration["general.language"]); subtle: true; onClicked: fileDialog.close() }
    C.ActionButton { text: fileDialog.operation==="delete" ? "В корзину" : "Готово"; primary: true; onClicked: {var n=fileName.text;if(fileDialog.operation==="newFile"){var ext=[".py",".json",".md",".txt",".env",".gitignore","requirements.txt",""][fileType.currentIndex];if(fileType.currentIndex===6)n="requirements.txt";else if(fileType.currentIndex===5)n=".gitignore";else if(fileType.currentIndex===4)n=".env";else if(ext!==""&&!n.endsWith(ext))n+=ext;}backend.fileOperation(fileDialog.operation,fileDialog.path,n);fileDialog.close();} }
   }
  }
 }
 C.SearchOverlay { id: quickOpen }
 C.Modal { id: linkDialog; title: backend.translate("Открыть ссылку",backend.configuration["general.language"]); property string address: "";
  contentItem: ColumnLayout { spacing: 14
   Text { text: linkDialog.address; color: C.Theme.muted; Layout.fillWidth: true; wrapMode: Text.Wrap }
   RowLayout { Layout.alignment: Qt.AlignRight
    C.ActionButton { text: backend.translate("Отмена",backend.configuration["general.language"]); subtle: true; onClicked: linkDialog.close() }
    C.ActionButton { text: backend.translate("Встроенный браузер",backend.configuration["general.language"]); onClicked: {backend.openBrowser(linkDialog.address);linkDialog.close();} }
    C.ActionButton { text: backend.translate("Браузер по умолчанию",backend.configuration["general.language"]); primary: true; onClicked: {backend.openExternalLink(linkDialog.address);linkDialog.close();} }
   }
  }
 }
 C.Modal {
  id: errorDialog; title: backend.translate("Не удалось выполнить действие",backend.configuration["general.language"])
  property string message: ""
  contentItem: ColumnLayout { spacing: 20
   Text { text: errorDialog.message; color: C.Theme.muted; font.pixelSize: 13; Layout.fillWidth: true; wrapMode: Text.Wrap; maximumLineCount: 12; elide: Text.ElideRight }
   C.ActionButton { text: backend.translate("Понятно",backend.configuration["general.language"]); Layout.alignment: Qt.AlignRight; onClicked: errorDialog.close() }
  }
 }
 Connections {
  target: backend
  function onError(message){errorDialog.message=message;errorDialog.open();}
  function onConfirmUnsaved(message){unsavedDialog.open();}
  function onUnsavedResolved(){unsavedDialog.close();}
  function onEnvironmentNeeded(path){createDialog.existingPath=path;createDialog.open();}
  function onStateChanged(){if(backend.page==="ide"&&!backend.busy)createDialog.close();}
  function onQuitApproved(){window.approved=true;}
  function onShowQuickOpen(){quickOpen.open();}
  function onExternalLinkRequested(url){linkDialog.address=url;linkDialog.open();}
 }
}
