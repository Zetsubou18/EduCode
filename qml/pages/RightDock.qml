import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtWebEngine 1.10
import "../components" as C
Item {
 id: dock
 property int activePanel: -1
 property real panelWidth: 0
 property bool browserCreated: false
 property string aiModel: backend.configuration[backend.configuration["ai.provider"]==="groq" ? "ai.groqModel" : "ai.model"] || ""
 property var titles: ["Уведомления","Браузер","Лира"].map(function(s){return backend.translate(s,backend.configuration["general.language"]);})
 property var pluginPanels: backend.plugins.filter(function(p){return p.enabled&&p.sidebar&&p.sidebar.url;})
 onPluginPanelsChanged: if(activePanel>=3 && !pluginPanels[activePanel-3]){activePanel=-1;panelWidth=0;}
 property string pluginOutput: ""
 function toggle(index){if(activePanel===index){panelWidth=0;activePanel=-1;}else{activePanel=index;panelWidth=Math.min(440,width*0.45);if(index===0)backend.markNotificationsRead();if(index===1)browserCreated=true;}}
 Behavior on panelWidth { NumberAnimation { duration: 180; easing.type: Easing.InOutCubic } }
 property real occupiedWidth: panelWidth+44
 Rectangle {
  id: pane; anchors.right: rail.left; anchors.top: parent.top; anchors.bottom: parent.bottom; width: dock.panelWidth; clip: true; color: C.Theme.sidebar
  Rectangle { anchors.left: parent.left; width: 1; height: parent.height; color: C.Theme.border }
  Item {
   anchors.right: parent.right; height: parent.height; width: Math.max(350,dock.panelWidth)
   ColumnLayout {
    anchors.fill: parent; anchors.margins: 12; spacing: 10
    RowLayout {
     Layout.fillWidth: true
     Text { text: dock.activePanel<0 ? "" : dock.activePanel<3 ? dock.titles[dock.activePanel] : dock.pluginPanels[dock.activePanel-3].sidebar.title; color: C.Theme.text; font.pixelSize: 14; Layout.fillWidth: true }
     C.ActionButton { glyph: "close"; subtle: true; implicitWidth: 28; implicitHeight: 28; hint: backend.translate("Свернуть",backend.configuration["general.language"]); onClicked: dock.toggle(dock.activePanel) }
    }
    Item {
     Layout.fillWidth: true; Layout.fillHeight: true
     ColumnLayout {
      anchors.fill: parent; visible: dock.activePanel===0; spacing: 12
      RowLayout { Layout.fillWidth: true
       Text { text: backend.translate("История",backend.configuration["general.language"]); color: C.Theme.faint; font.pixelSize: 11; Layout.fillWidth: true }
       C.ActionButton { text: backend.translate("Очистить",backend.configuration["general.language"]); subtle: true; implicitHeight: 28; onClicked: backend.clearNotifications() }
      }
      ListView {
       id: notificationList; Layout.fillWidth: true; Layout.fillHeight: true; model: backend.notifications; clip: true; spacing: 12
       delegate: Rectangle {
        width: notificationList.width; height: notificationText.implicitHeight+54; radius: 6; color: C.Theme.surface
        Column { anchors.fill: parent; anchors.margins: 12; spacing: 8
         RowLayout { width: parent.width
          C.Icon { name: "bell"; size: 14; color: C.Theme.accent }
          Text { text: modelData.title; color: C.Theme.text; font.pixelSize: 13; font.weight: Font.Medium; Layout.fillWidth: true; elide: Text.ElideRight }
          Text { text: modelData.time; color: C.Theme.faint; font.pixelSize: 10 }
         }
         Text { id: notificationText; width: parent.width; text: modelData.message; color: C.Theme.muted; font.pixelSize: 12; wrapMode: Text.Wrap; textFormat: Text.PlainText }
        }
       }
       Text { anchors.centerIn: parent; visible: notificationList.count===0; text: backend.translate("Новых уведомлений нет",backend.configuration["general.language"]); color: C.Theme.faint; font.pixelSize: 12 }
       ScrollBar.vertical: ScrollBar {}
      }
     }
     Loader { id: browserLoader; anchors.fill: parent; active: dock.browserCreated; visible: dock.activePanel===1; sourceComponent: browserComponent }
     ColumnLayout {
      anchors.fill: parent; visible: dock.activePanel===2; spacing: 8
      RowLayout { Layout.fillWidth: true
       Repeater { model: backend.ai.chats
        C.ActionButton { text: modelData.title.length>14 ? modelData.title.slice(0,14)+"…" : modelData.title; subtle: backend.ai.activeChat!==modelData.id; implicitWidth: 112; onClicked: backend.ai.selectChat(modelData.id) }
       }
       Item { Layout.fillWidth: true }
       C.ActionButton { glyph: "add"; subtle: true; implicitWidth: 30; hint: backend.translate("Новый чат · максимум 3",backend.configuration["general.language"]); enabled: backend.ai.chats.length<3&&!backend.ai.busy; onClicked: backend.ai.createChat() }
       C.ActionButton { glyph: "close"; subtle: true; implicitWidth: 30; hint: backend.translate("Удалить чат",backend.configuration["general.language"]); enabled: !backend.ai.busy; onClicked: backend.ai.deleteChat(backend.ai.activeChat) }
      }
      Rectangle { Layout.fillWidth: true; height: 1; color: C.Theme.border }
      ColumnLayout { visible: backend.configuration["ai.provider"]!=="groq" && !backend.ai.ollamaAvailable; Layout.fillWidth: true; Layout.fillHeight: true; spacing: 12
       Item { Layout.fillHeight: true }
       C.Icon { name: "ai"; size: 36; color: C.Theme.accent; Layout.alignment: Qt.AlignHCenter }
       Text { text: backend.translate("Для Лиры нужна Ollama",backend.configuration["general.language"]); color: C.Theme.text; font.pixelSize: 16; Layout.alignment: Qt.AlignHCenter }
       Text { text: backend.translate("Установите Ollama, загрузите модель и выберите её в настройках ИИ.",backend.configuration["general.language"]); color: C.Theme.muted; font.pixelSize: 12; wrapMode: Text.Wrap; horizontalAlignment: Text.AlignHCenter; Layout.fillWidth: true }
       C.ActionButton { text: backend.translate("Скачать Ollama",backend.configuration["general.language"]); primary: true; Layout.alignment: Qt.AlignHCenter; onClicked: backend.installOllama() }
       C.ActionButton { text: backend.translate("Проверить снова",backend.configuration["general.language"]); subtle: true; Layout.alignment: Qt.AlignHCenter; onClicked: backend.ai.probe() }
       Item { Layout.fillHeight: true }
      }
      ColumnLayout { visible: backend.configuration["ai.provider"]==="groq" || backend.ai.ollamaAvailable; Layout.fillWidth: true; Layout.fillHeight: true; spacing: 8
       ListView {
        id: chatMessages; Layout.fillWidth: true; Layout.fillHeight: true; model: backend.ai.messages; clip: true; spacing: 9
        delegate: Rectangle {
         width: chatMessages.width; height: messageText.implicitHeight+42; radius: 7; color: modelData.role==="user" ? C.Theme.accentSurface : C.Theme.surface
         Column { anchors.fill: parent; anchors.margins: 10; spacing: 5
          Row { width: parent.width
           Text { text: modelData.role==="user" ? "Вы" : "Лира"; color: modelData.role==="user" ? C.Theme.text : C.Theme.accent; font.pixelSize: 11; font.weight: Font.DemiBold; width: parent.width-45 }
           Text { text: modelData.time||""; color: C.Theme.faint; font.pixelSize: 9 }
          }
          Text { id: messageText; width: parent.width; text: modelData.content; textFormat: Text.MarkdownText; color: C.Theme.text; linkColor: C.Theme.accent; font.pixelSize: 12; wrapMode: Text.Wrap; onLinkActivated: {if(link.indexOf("http")===0)backend.openBrowser(link);else backend.openProjectLink(link);} }
         }
        }
        onCountChanged: positionViewAtEnd()
        ScrollBar.vertical: ScrollBar {}
       }
       RowLayout { visible: backend.ai.activity!==""; Layout.fillWidth: true
        BusyIndicator { running: true; implicitWidth: 20; implicitHeight: 20 }
        Text { text: backend.ai.activity; color: C.Theme.accent; font.pixelSize: 11; Layout.fillWidth: true; elide: Text.ElideRight }
        C.ActionButton { text: backend.translate("Стоп",backend.configuration["general.language"]); subtle: true; implicitHeight: 26; onClicked: backend.ai.stop() }
       }
       TextArea { id: aiInput; Layout.fillWidth: true; Layout.preferredHeight: 86; enabled: !backend.ai.busy; placeholderText: dock.aiModel ? "Напишите Лире…" : "Выберите модель в настройках ИИ"; color: C.Theme.text; placeholderTextColor: C.Theme.faint; font.pixelSize: 12; wrapMode: TextEdit.Wrap; selectByMouse: true; background: Rectangle { color: C.Theme.editor; border.color: aiInput.activeFocus?C.Theme.accent:C.Theme.border; radius: 6 }
        Keys.onPressed: {if(event.key===Qt.Key_Return&&(event.modifiers&Qt.ControlModifier)){sendButton.clicked();event.accepted=true;}}
       }
       RowLayout { Layout.fillWidth: true
        Text { text: backend.translate("Ctrl+Enter — отправить",backend.configuration["general.language"]); color: C.Theme.faint; font.pixelSize: 10; Layout.fillWidth: true }
        C.ActionButton { text: backend.translate("Повторить запрос",backend.configuration["general.language"]); subtle: true; enabled: !backend.ai.busy && backend.ai.messages.some(function(m){return m.role==="user";}); onClicked: backend.ai.retry() }
        C.ActionButton { id: sendButton; text: backend.translate("Отправить",backend.configuration["general.language"]); primary: true; enabled: !backend.ai.busy&&aiInput.text.trim()!==""&&dock.aiModel!==""; onClicked: {backend.ai.send(aiInput.text);aiInput.clear();} }
       }
      }
     }
     Repeater { model: dock.pluginPanels
      ColumnLayout { anchors.fill: parent; visible: dock.activePanel===3+index
       WebEngineView { Layout.fillWidth: true; Layout.preferredHeight: 190; url: modelData.sidebar.url
        settings.localContentCanAccessRemoteUrls: false; settings.javascriptCanOpenWindows: false
        onNavigationRequested: {var link=request.url.toString(),prefix=modelData.sidebar.url+"?command=";if(link.indexOf(prefix)===0){request.action=WebEngineNavigationRequest.IgnoreRequest;backend.pluginAction(modelData.id,decodeURIComponent(link.substring(prefix.length)));}else if(link!==modelData.sidebar.url)request.action=WebEngineNavigationRequest.IgnoreRequest;}
       }
       TextArea { Layout.fillWidth: true; Layout.fillHeight: true; readOnly: true; text: dock.pluginOutput; color: C.Theme.text; font.family: "Consolas"; font.pixelSize: 11; wrapMode: TextEdit.NoWrap; background: Rectangle { color: C.Theme.editor } }
      }
     }
    }
   }
  }
 }
 Rectangle {
  id: rail; anchors.right: parent.right; anchors.top: parent.top; anchors.bottom: parent.bottom; width: 44; color: C.Theme.sidebar
  Rectangle { width: 1; height: parent.height; color: C.Theme.border }
  Column { anchors.horizontalCenter: parent.horizontalCenter; y: 8; spacing: 8
   Repeater { model: [{icon:"bell",title: backend.translate("Уведомления",backend.configuration["general.language"])},{icon:"browser",title: backend.translate("Браузер",backend.configuration["general.language"])},{icon:"ai",title: backend.translate("Лира",backend.configuration["general.language"])}]
    C.ActionButton { visible: index!==2||backend.configuration["ai.enabled"]; glyph: modelData.icon; subtle: true; implicitWidth: 36; implicitHeight: 36; hint: modelData.title; ink: dock.activePanel===index ? C.Theme.text : C.Theme.muted; onClicked: dock.toggle(index)
     Rectangle { visible: index===0&&backend.unreadNotifications>0; x: 24; y: 5; width: 6; height: 6; radius: 3; color: C.Theme.accent }
    }
   }
   Repeater { model: dock.pluginPanels
    C.ActionButton { glyph: "file"; subtle: true; implicitWidth: 36; implicitHeight: 36; hint: modelData.sidebar.title; ink: dock.activePanel===3+index ? C.Theme.text : C.Theme.muted; onClicked: dock.toggle(3+index) }
   }
  }
 }
 Component {
  id: browserComponent
  ColumnLayout {
   function open(value){ browser.url=value }
   spacing: 8
   RowLayout {
    Layout.fillWidth: true; spacing: 4
    C.ActionButton { glyph: "back"; subtle: true; implicitWidth: 28; enabled: browser.canGoBack; hint: backend.translate("Назад",backend.configuration["general.language"]); onClicked: browser.goBack() }
    C.ActionButton { glyph: "home"; subtle: true; implicitWidth: 28; hint: backend.translate("Начальная страница",backend.configuration["general.language"]); onClicked: browser.url=backend.configuration["browser.homePage"] }
    C.ActionButton { glyph: "refresh"; subtle: true; implicitWidth: 28; hint: backend.translate("Обновить",backend.configuration["general.language"]); onClicked: browser.reload() }
    C.Input { id: address; Layout.fillWidth: true; implicitHeight: 32; font.pixelSize: 11; text: browser.url.toString(); selectByMouse: true
     onAccepted: {var value=text.trim();if(!/^https?:\/\//i.test(value))value="https://"+value;if(/^https?:\/\//i.test(value))browser.url=value;}
    }
   }
   WebEngineView {
    id: browser; Layout.fillWidth: true; Layout.fillHeight: true; url: backend.configuration["browser.homePage"]
    profile: WebEngineProfile { storageName: "EduCodeMiniBrowser"; offTheRecord: false }
    settings.localContentCanAccessFileUrls: false; settings.javascriptCanOpenWindows: false
    onNewViewRequested: {if(/^https?:\/\//i.test(request.requestedUrl.toString()))browser.url=request.requestedUrl;}
    onNavigationRequested: {if(!/^https?:\/\//i.test(request.url.toString())&&request.url.toString()!=="about:blank")request.action=WebEngineNavigationRequest.IgnoreRequest;}
    onFeaturePermissionRequested: grantFeaturePermission(securityOrigin,feature,false)
   }
   ProgressBar { Layout.fillWidth: true; implicitHeight: 3; visible: browser.loading; value: browser.loadProgress/100 }
  }
 }
 Connections { target: backend; function onSidebarRequested(index){if(dock.activePanel!==index)dock.toggle(index);} function onBrowserRequested(url){browserCreated=true;if(dock.activePanel!==1)dock.toggle(1);browserLoader.item.open(url);} function onNotificationsChanged(){if(dock.activePanel===0&&backend.unreadNotifications>0)backend.markNotificationsRead();} function onPluginResult(id,result){dock.pluginOutput=result;} }
}
