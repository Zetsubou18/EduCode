import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Dialogs 1.3 as Native
import "../components" as C
Rectangle {
 id: settingsPage
 color: C.Theme.background
 property string selected: "general"
 property var cfg: backend.configuration
 property var sections: [
  {id:"general",name:backend.translate("Основное",cfg["general.language"]),keywords:"проект сохранение папка"},
  {id:"editor",name:backend.translate("Редактор",cfg["general.language"]),keywords:"шрифт отступ размер миникарта перенос строк"},
  {id:"terminal",name:backend.translate("Терминал",cfg["general.language"]),keywords:"шрифт история размер вывод система запуск"},
  {id:"browser",name:backend.translate("Браузер",cfg["general.language"]),keywords:"google сайт начальная страница"},
  {id:"python",name:"Python",keywords:"интерпретатор окружение пути"},
  {id:"notifications",name:backend.translate("Уведомления",cfg["general.language"]),keywords:"сообщение система тест"},
  {id:"ai",name:backend.translate("ИИ · Лира",cfg["general.language"]),keywords:"ollama groq api модель агент память контекст"},
  {id:"plugins",name:backend.translate("Плагины",cfg["general.language"]),keywords:"python расширения установка"},
  {id:"hotkeys",name:backend.translate("Горячие клавиши",cfg["general.language"]),keywords:"комбинации палитра команды f1"},
  {id:"about",name:backend.translate("О программе",cfg["general.language"]),keywords:"educode zetsubou версия"}
 ].concat(backend.plugins.filter(function(p){return p.enabled&&(p.settings||[]).length>0;}).map(function(p){return {id:"plugin:"+p.id,name:p.name,keywords:p.id};}))
 property var names: ({save:"Сохранить",saveAll:"Сохранить все файлы",find:"Поиск в редакторе",replace:"Замена в редакторе",close:"Закрыть вкладку",quickOpen:"Поиск проекта и калькулятор",run:"Запустить",back:"Назад к позиции",palette:"Палитра команд",split:"Разделить редактор",terminal:"Терминал",toggleExplorer:"Проводник",notifications:"Уведомления",browser:"Браузер"})
 ColumnLayout {
  anchors.fill: parent; spacing: 0
  RowLayout {
   Layout.fillWidth: true; Layout.preferredHeight: 64; Layout.leftMargin: 24; Layout.rightMargin: 24
   C.ActionButton { glyph: "back"; text: backend.translate("Назад",backend.configuration["general.language"]); subtle: true; onClicked: backend.back() }
   Text { text: backend.translate("Настройки",backend.configuration["general.language"]); color: C.Theme.text; font.pixelSize: 20; Layout.fillWidth: true }
   Text { text: backend.translate("Сохраняются автоматически",backend.configuration["general.language"]); color: C.Theme.faint; font.pixelSize: 11 }
  }
  Rectangle { Layout.fillWidth: true; height: 1; color: C.Theme.border }
  RowLayout {
   Layout.fillWidth: true; Layout.fillHeight: true; spacing: 0
   Rectangle {
    Layout.preferredWidth: 245; Layout.fillHeight: true; color: C.Theme.sidebar
    ColumnLayout {
     anchors.fill: parent; anchors.margins: 16; spacing: 12
     C.Input { id: query; Layout.fillWidth: true; placeholderText: backend.translate("Найти настройку…",backend.configuration["general.language"]) }
     ListView {
      Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 4
      model: settingsPage.sections.filter(function(s){return (s.name+" "+s.keywords).toLowerCase().indexOf(query.text.toLowerCase())>=0;})
      delegate: Rectangle { width: ListView.view.width; height: 38; radius: 5; color: settingsPage.selected===modelData.id ? C.Theme.accentSurface : sectionMouse.containsMouse ? C.Theme.hover : "transparent"
       Text { anchors.fill: parent; anchors.margins: 10; text: modelData.name; color: C.Theme.text; font.pixelSize: 13; verticalAlignment: Text.AlignVCenter }
       MouseArea { id: sectionMouse; anchors.fill: parent; hoverEnabled: true; onClicked: settingsPage.selected=modelData.id }
      }
     }
     C.ActionButton { text: backend.translate("Сбросить настройки",backend.configuration["general.language"]); subtle: true; ink: C.Theme.muted; onClicked: resetDialog.open() }
    }
   }
   ScrollView {
    id: settingsScroll; contentWidth: availableWidth; contentHeight: settingsColumn.implicitHeight+60
    Layout.fillWidth: true; Layout.fillHeight: true; clip: true
    ColumnLayout {
     id: settingsColumn
     width: Math.max(400,settingsScroll.availableWidth-64); x: 32; y: 28; spacing: 18
     Text { text: {for(var i=0;i<settingsPage.sections.length;i++)if(settingsPage.sections[i].id===settingsPage.selected)return settingsPage.sections[i].name;return "";} color: C.Theme.text; font.pixelSize: 24; font.weight: Font.DemiBold }
     ColumnLayout {
      visible: settingsPage.selected==="general"; Layout.fillWidth: true; spacing: 16
      Text { text: backend.translate("Имя в Teacher Mode",backend.configuration["general.language"]); color: C.Theme.muted; font.pixelSize: 13 }
      Text { text: backend.translate("Язык интерфейса / Interface language",backend.configuration["general.language"]); color: C.Theme.muted; font.pixelSize: 13 }
      C.Select { Layout.preferredWidth: 250; model: ["Русский", "English"].concat(backend.plugins.reduce(function(a,p){return a.concat(Object.keys(p.translations||{}));},[])); currentIndex: Math.max(0,["ru","en"].concat(backend.plugins.reduce(function(a,p){return a.concat(Object.keys(p.translations||{}));},[])).indexOf(cfg["general.language"])); onActivated: backend.setSetting("general.language",["ru","en"].concat(backend.plugins.reduce(function(a,p){return a.concat(Object.keys(p.translations||{}));},[]))[index]) }
      C.Input { Layout.fillWidth: true; text: cfg["general.displayName"]; placeholderText: backend.translate("Ваше имя",backend.configuration["general.language"]); maximumLength: 80; onEditingFinished: backend.setSetting("general.displayName",text.trim()) }
      C.Toggle { text: backend.translate("Сохранять файлы перед запуском",backend.configuration["general.language"]); checked: cfg["general.saveBeforeRun"]; onClicked: backend.setSetting("general.saveBeforeRun",checked) }
      Text { text: backend.translate("Папка для новых проектов",backend.configuration["general.language"]); color: C.Theme.muted; font.pixelSize: 13 }
      C.Input { Layout.fillWidth: true; text: cfg["general.projectsDirectory"]; onEditingFinished: {if(text.trim()!=="")backend.setSetting("general.projectsDirectory",text.trim());} }
      Text { text: backend.translate("Конфигурация JSON",backend.configuration["general.language"]); color: C.Theme.muted; font.pixelSize: 13 }
      Text { text: backend.configPath; color: C.Theme.faint; font.pixelSize: 11; Layout.fillWidth: true; wrapMode: Text.Wrap }
      C.ActionButton { text: backend.translate("Открыть конфигурацию",backend.configuration["general.language"]); glyph: "file"; enabled: backend.projectPath!==""; onClicked: {backend.back();backend.openFile(backend.configPath);} }
      Text { visible: Qt.platform.os!=="linux"; text: backend.translate("Прокси для ИИ и браузера",backend.configuration["general.language"]); color: C.Theme.muted; font.pixelSize: 13 }
      C.Input { visible: Qt.platform.os!=="linux"; Layout.fillWidth: true; text: cfg["network.proxy"]; placeholderText: backend.translate("socks5://host:port или http://host:port",backend.configuration["general.language"]); onEditingFinished: backend.setSetting("network.proxy",text.trim()) }
      Text { visible: Qt.platform.os!=="linux"; text: backend.translate("Пустое поле — без явного прокси. Браузер применяет изменение после перезапуска IDE.",backend.configuration["general.language"]); color: C.Theme.faint; font.pixelSize: 11; Layout.fillWidth: true; wrapMode: Text.Wrap }
      Text { text: backend.translate("Журнал IDE",backend.configuration["general.language"]); color: C.Theme.muted; font.pixelSize: 13 }
      Text { text: backend.logPath; color: C.Theme.faint; font.pixelSize: 11; Layout.fillWidth: true; wrapMode: Text.Wrap }
      C.ActionButton { text: backend.translate("Показать папку журнала",backend.configuration["general.language"]); glyph: "folder"; onClicked: backend.fileOperation("reveal",backend.logPath) }
     }
     ColumnLayout {
      visible: settingsPage.selected==="editor"; spacing: 14
      C.Toggle { text: backend.translate("Показывать миникарту кода",backend.configuration["general.language"]); checked: cfg["editor.minimap"]; onClicked: backend.setSetting("editor.minimap",checked) }
      C.Toggle { text: backend.translate("Переносить длинные строки",backend.configuration["general.language"]); checked: cfg["editor.wordWrap"]; onClicked: backend.setSetting("editor.wordWrap",checked) }
     }
     ColumnLayout {
      visible: settingsPage.selected==="editor"; spacing: 14
      Text { text: backend.translate("Размер шрифта редактора",backend.configuration["general.language"]); color: C.Theme.muted; font.pixelSize: 13 }
      C.NumberInput { from: 9; to: 32; value: cfg["editor.fontSize"]; onValueModified: backend.setSetting("editor.fontSize",value) }
      Text { text: backend.translate("Количество пробелов в отступе",backend.configuration["general.language"]); color: C.Theme.muted; font.pixelSize: 13 }
      C.NumberInput { from: 1; to: 8; value: cfg["editor.tabSize"]; onValueModified: backend.setSetting("editor.tabSize",value) }
     }
     ColumnLayout {
      visible: settingsPage.selected==="terminal"; spacing: 14
      Text { text: backend.translate("Размер шрифта терминала и консоли",backend.configuration["general.language"]); color: C.Theme.muted; font.pixelSize: 13 }
      C.NumberInput { from: 9; to: 32; value: cfg["terminal.fontSize"]; onValueModified: backend.setSetting("terminal.fontSize",value) }
      Text { text: backend.translate("Сколько строк хранить в истории",backend.configuration["general.language"]); color: C.Theme.muted; font.pixelSize: 13 }
      C.NumberInput { from: 100; to: 50000; stepSize: 100; value: cfg["terminal.scrollback"]; onValueModified: backend.setSetting("terminal.scrollback",value) }
     }
     ColumnLayout {
      visible: settingsPage.selected==="terminal"; spacing: 14; Layout.fillWidth: true
      Text { text: backend.translate("Где запускать Python",backend.configuration["general.language"]); color: C.Theme.muted; font.pixelSize: 13 }
      C.Select { Layout.preferredWidth: 300; model: ["Консоль IDE","Системная консоль"]; currentIndex: cfg["console.output"]==="system" ? 1 : 0; onActivated: backend.setSetting("console.output",index===1 ? "system" : "ide") }
      Text { text: backend.translate("В системной консоли ввод и вывод находятся в отдельном окне.\nПосле завершения Enter закрывает окно. Изменение действует при следующем запуске.",backend.configuration["general.language"]); color: C.Theme.faint; font.pixelSize: 12; Layout.fillWidth: true; wrapMode: Text.Wrap }
     }
     ColumnLayout {
      visible: settingsPage.selected==="browser"; spacing: 14; Layout.fillWidth: true
      Text { text: backend.translate("Начальная страница",backend.configuration["general.language"]); color: C.Theme.muted; font.pixelSize: 13 }
      C.Input { Layout.fillWidth: true; text: cfg["browser.homePage"]; placeholderText: "https://www.google.com"; onEditingFinished: backend.setSetting("browser.homePage",text.trim()) }
      Text { text: backend.translate("Кнопка домика в боковом браузере открывает этот адрес.",backend.configuration["general.language"]); color: C.Theme.faint; font.pixelSize: 12; Layout.fillWidth: true; wrapMode: Text.Wrap }
     }
     ColumnLayout {
      visible: settingsPage.selected==="python"; spacing: 14; Layout.fillWidth: true
      Text { text: backend.translate("Интерпретатор текущего проекта",backend.configuration["general.language"]); color: C.Theme.muted; font.pixelSize: 13 }
      Text { text: backend.projectPath!=="" ? backend.pythonPath : "Откройте проект, чтобы увидеть его окружение."; color: C.Theme.faint; font.pixelSize: 12; Layout.fillWidth: true; wrapMode: Text.Wrap }
      Text { text: backend.translate("Обнаруженные версии Python",backend.configuration["general.language"]); color: C.Theme.muted; font.pixelSize: 13 }
      Repeater { model: backend.pythonVersions; Text { text: modelData.label+" · "+modelData.path; color: C.Theme.faint; font.pixelSize: 12; Layout.fillWidth: true; wrapMode: Text.Wrap } }
      Text { text: backend.translate("Дополнительные пути для анализа кода (по одному в строке)",backend.configuration["general.language"]); color: C.Theme.muted; font.pixelSize: 13 }
      TextArea { Layout.fillWidth: true; Layout.preferredHeight: 110; text: cfg["python.extraPaths"].join("\n"); color: C.Theme.text; font.pixelSize: 12; wrapMode: TextEdit.Wrap; selectByMouse: true; background: Rectangle { color: C.Theme.editor; border.color: C.Theme.border; radius: 5 }
       onActiveFocusChanged: if(!activeFocus)backend.setSetting("python.extraPaths",text.split("\n").filter(function(v){return v.trim()!=="";}))
      }
     }
     ColumnLayout {
      visible: settingsPage.selected==="notifications"; spacing: 14; Layout.fillWidth: true
      Text { text: backend.translate("Куда доставлять уведомления",backend.configuration["general.language"]); color: C.Theme.muted; font.pixelSize: 13 }
      C.Select { Layout.preferredWidth: 300; model: ["В IDE","В систему","В IDE и систему"]; currentIndex: ["ide","system","both"].indexOf(cfg["notifications.delivery"]); onActivated: backend.setSetting("notifications.delivery",["ide","system","both"][index]) }
      C.ActionButton { text: backend.translate("Тестовое уведомление",backend.configuration["general.language"]); glyph: "bell"; onClicked: backend.notify("EduCode","Уведомления работают. Это тестовое сообщение.") }
      Text { text: 'Из Python: import educode; educode.notify("Готово", "Задача завершена")\nИз терминала: python -m educode notify "Готово" "Задача завершена"'; color: C.Theme.faint; font.pixelSize: 12; Layout.fillWidth: true; wrapMode: Text.Wrap }
     }
     ColumnLayout {
      visible: settingsPage.selected==="ai"; spacing: 14; Layout.fillWidth: true
      C.Toggle { text: backend.translate("Включить Лиру",backend.configuration["general.language"]); checked: cfg["ai.enabled"]; onClicked: backend.setSetting("ai.enabled",checked) }
      C.Toggle { text: backend.translate("Показывать «Спросить Лиру» при ошибках",backend.configuration["general.language"]); checked: cfg["ai.askButtons"]; onClicked: backend.setSetting("ai.askButtons",checked) }
      C.Select { Layout.preferredWidth: 300; model: ["Ollama", "Groq"]; currentIndex: cfg["ai.provider"]==="groq" ? 1 : 0; onActivated: backend.setSetting("ai.provider",index===1 ? "groq" : "ollama") }
      ColumnLayout { visible: cfg["ai.provider"]==="groq"; Layout.fillWidth: true; spacing: 12
       Text { text: backend.translate("API-ключ Groq",backend.configuration["general.language"]); color: C.Theme.muted }
       C.Input { Layout.fillWidth: true; echoMode: TextInput.Password; text: cfg["ai.groqApiKey"]; onEditingFinished: backend.setSetting("ai.groqApiKey",text.trim()) }
       Text { text: backend.translate("Модель Groq · ID из консоли",backend.configuration["general.language"]); color: C.Theme.muted }
       C.Input { Layout.fillWidth: true; text: cfg["ai.groqModel"]; placeholderText: "openai/gpt-oss-20b"; onEditingFinished: backend.setSetting("ai.groqModel",text.trim()) }
       Text { text: backend.translate("Ключ хранится в локальной конфигурации. Код и контекст отправляются в Groq.",backend.configuration["general.language"]); color: C.Theme.faint; Layout.fillWidth: true; wrapMode: Text.Wrap }
       RowLayout {
        Text { text: backend.translate("Запросов/мин",backend.configuration["general.language"]); color: C.Theme.muted }
        C.NumberInput { from:1;to:1000;value:cfg["ai.groqRPM"];onValueModified:backend.setSetting("ai.groqRPM",value) }
        Text { text: backend.translate("Запросов/сутки",backend.configuration["general.language"]); color: C.Theme.muted }
        C.NumberInput { from:1;to:100000;value:cfg["ai.groqRPD"];onValueModified:backend.setSetting("ai.groqRPD",value) }
       }
       RowLayout {
        Text { text: backend.translate("Токенов/мин",backend.configuration["general.language"]); color: C.Theme.muted }
        C.NumberInput { from:1024;to:1000000;stepSize:1024;value:cfg["ai.groqTPM"];onValueModified:backend.setSetting("ai.groqTPM",value) }
        Text { text: backend.translate("Токенов/сутки",backend.configuration["general.language"]); color: C.Theme.muted }
        C.NumberInput { from:1024;to:10000000;stepSize:1000;value:cfg["ai.groqTPD"];onValueModified:backend.setSetting("ai.groqTPD",value) }
       }
       Text { text: backend.translate("Паузы и счётчик сохраняются между чатами и запусками. Укажите лимиты своей модели; расход других клиентов учитывается по ответам Groq.",backend.configuration["general.language"]); color:C.Theme.faint; Layout.fillWidth:true; wrapMode:Text.Wrap }
       C.ActionButton { text: backend.translate("Модели и лимиты Groq",backend.configuration["general.language"]); onClicked: Qt.openUrlExternally("https://console.groq.com/docs/rate-limits") }
      }
      Text { visible: cfg["ai.provider"]!=="groq"; text: backend.translate("Сервер Ollama",backend.configuration["general.language"]); color: C.Theme.muted; font.pixelSize: 13 }
      RowLayout { visible: cfg["ai.provider"]!=="groq"; Layout.fillWidth: true
       C.Input { Layout.fillWidth: true; text: cfg["ai.url"]; onEditingFinished: backend.setSetting("ai.url",text.trim()) }
       C.ActionButton { text: backend.translate("Проверить",backend.configuration["general.language"]); onClicked: backend.ai.probe() }
      }
      RowLayout { visible: cfg["ai.provider"]!=="groq"; Layout.fillWidth: true
       Text { text: backend.ai.ollamaAvailable ? "Ollama подключена" : "Ollama не найдена"; color: backend.ai.ollamaAvailable ? C.Theme.green : C.Theme.error; font.pixelSize: 12; Layout.fillWidth: true }
       C.ActionButton { visible: !backend.ai.ollamaAvailable; text: backend.translate("Скачать Ollama",backend.configuration["general.language"]); onClicked: backend.installOllama() }
      }
      Text { text: backend.translate("Модель",backend.configuration["general.language"]); color: C.Theme.muted; font.pixelSize: 13 }
      C.Select { visible: cfg["ai.provider"]!=="groq"; Layout.preferredWidth: 360; model: [{name:"Не выбрана"}].concat(backend.ai.models); textRole: "name"; currentIndex: {for(var i=1;i<model.length;i++)if(model[i].name===cfg["ai.model"])return i;return 0;} onActivated: backend.setSetting("ai.model",index===0 ? "" : model[index].name) }
      Text { visible: cfg["ai.provider"]!=="groq"; text: backend.translate("Модель не выбирается автоматически. Сначала загрузите её командой ollama pull <имя>.",backend.configuration["general.language"]); color: C.Theme.faint; font.pixelSize: 11; Layout.fillWidth: true; wrapMode: Text.Wrap }
      Text { text: backend.translate("Что Лире важно знать о вас",backend.configuration["general.language"]); color: C.Theme.muted; font.pixelSize: 13; Layout.topMargin: 8 }
      TextArea { Layout.fillWidth: true; Layout.preferredHeight: 90; text: cfg["ai.userPrompt"]; placeholderText: backend.translate("Например: я только начинаю изучать Python; объясняй простыми словами",backend.configuration["general.language"]); color: C.Theme.text; placeholderTextColor: C.Theme.faint; wrapMode: TextEdit.Wrap; selectByMouse: true; background: Rectangle { color:C.Theme.editor;border.color:C.Theme.border;radius:5 } onActiveFocusChanged: if(!activeFocus)backend.setSetting("ai.userPrompt",text) }
      Text { text: backend.translate("Глобальная память о пользователе · JSON",backend.configuration["general.language"]); color: C.Theme.muted; font.pixelSize: 13 }
      TextArea { id: userMemory; Layout.fillWidth: true; Layout.preferredHeight: 100; text: JSON.stringify(cfg["ai.userMemory"],null,2); color:C.Theme.text; font.family:"Consolas"; font.pixelSize:11; selectByMouse:true; background:Rectangle{color:C.Theme.editor;border.color:C.Theme.border;radius:5} onActiveFocusChanged: if(!activeFocus){try{backend.setSetting("ai.userMemory",JSON.parse(text));}catch(e){text=JSON.stringify(cfg["ai.userMemory"],null,2);}} }
      Text { text: backend.translate("Рабочие факты Лиры · JSON",backend.configuration["general.language"]); color: C.Theme.muted; font.pixelSize: 13 }
      TextArea { id: factMemory; Layout.fillWidth: true; Layout.preferredHeight: 100; text: JSON.stringify(cfg["ai.factMemory"],null,2); color:C.Theme.text; font.family:"Consolas"; font.pixelSize:11; selectByMouse:true; background:Rectangle{color:C.Theme.editor;border.color:C.Theme.border;radius:5} onActiveFocusChanged: if(!activeFocus){try{backend.setSetting("ai.factMemory",JSON.parse(text));}catch(e){text=JSON.stringify(cfg["ai.factMemory"],null,2);}} }
      RowLayout {
       ColumnLayout { Text{text: backend.translate("Лимит памяти чата · символы",backend.configuration["general.language"]);color:C.Theme.muted;font.pixelSize:12} C.NumberInput{from:4000;to:100000;stepSize:1000;value:cfg["ai.chatMemoryLimit"];onValueModified:backend.setSetting("ai.chatMemoryLimit",value)} }
       ColumnLayout { Text{text: backend.translate("Контекст модели · токены",backend.configuration["general.language"]);color:C.Theme.muted;font.pixelSize:12} C.NumberInput{from:2048;to:131072;stepSize:1024;value:cfg["ai.contextSize"];onValueModified:backend.setSetting("ai.contextSize",value)} }
      }
      Text { text: backend.translate("После лимита старая часть конкретного чата сжимается. Память пользователя и рабочие факты хранятся отдельно и видны здесь.",backend.configuration["general.language"]); color:C.Theme.faint;font.pixelSize:11;Layout.fillWidth:true;wrapMode:Text.Wrap }
     }
     ColumnLayout {
      visible: settingsPage.selected==="hotkeys"; spacing: 8; Layout.fillWidth: true
      Text { text: backend.translate("Основные команды",backend.configuration["general.language"]); color: C.Theme.muted; font.pixelSize: 13 }
      Repeater { model: Object.keys(backend.shortcuts)
       RowLayout { Layout.fillWidth: true
        Text { text: backend.translate(settingsPage.names[modelData]||modelData,cfg["general.language"]); color: C.Theme.text; font.pixelSize: 12; Layout.fillWidth: true }
        C.Input { Layout.preferredWidth: 190; text: backend.shortcuts[modelData]; placeholderText: backend.translate("Без комбинации",backend.configuration["general.language"]); onEditingFinished: {if(text!==backend.shortcuts[modelData])backend.setHotkey(modelData,text);} }
       }
      }
      Text { text: backend.translate("Команды редактора",backend.configuration["general.language"]); color: C.Theme.muted; font.pixelSize: 13; Layout.topMargin: 18 }
      Repeater { model: backend.editorActions
       RowLayout { Layout.fillWidth: true
        Text { text: modelData.label; color: C.Theme.text; font.pixelSize: 12; Layout.fillWidth: true; elide: Text.ElideRight }
        C.Input { Layout.preferredWidth: 190; text: cfg["editor.hotkeys"][modelData.id]!==undefined ? cfg["editor.hotkeys"][modelData.id] : modelData.key||""; placeholderText: backend.translate("Без комбинации",backend.configuration["general.language"]); onEditingFinished: {var previous=cfg["editor.hotkeys"][modelData.id]!==undefined ? cfg["editor.hotkeys"][modelData.id] : modelData.key||"";if(text!==previous)backend.setEditorHotkey(modelData.id,text);} }
       }
      }
     }
     ColumnLayout {
      visible: settingsPage.selected==="plugins"; spacing: 14; Layout.fillWidth: true
      Text { text: backend.translate("Установить плагин из папки или ZIP-ссылки",backend.configuration["general.language"]); color: C.Theme.text; font.pixelSize: 13 }
      RowLayout { Layout.fillWidth: true
       C.Input { id: pluginSource; Layout.fillWidth: true; placeholderText: backend.translate("Путь к папке или https://…/plugin.zip",backend.configuration["general.language"]) }
       C.ActionButton { text: backend.translate("Выбрать",backend.configuration["general.language"]); onClicked: pluginPicker.open() }
       C.ActionButton { text: backend.translate("Добавить",backend.configuration["general.language"]); onClicked: backend.installPlugin(pluginSource.text.trim()) }
      }
      Native.FileDialog { id: pluginPicker; title: backend.translate("Выбрать plugin.json",backend.configuration["general.language"]); nameFilters: ["Plugin manifest (plugin.json)"]; onAccepted: pluginSource.text=decodeURIComponent(fileUrl.toString().replace(Qt.platform.os==="windows" ? "file:///" : "file://","")) }
      Repeater { model: backend.plugins
       ColumnLayout { Layout.fillWidth: true; spacing: 5
        RowLayout { Layout.fillWidth: true
         Text { text: modelData.name+" · "+(modelData.version||"")+" · "+modelData.id.slice(0,8); color: C.Theme.text; Layout.fillWidth: true }
         C.Toggle { text: backend.translate("Включён",backend.configuration["general.language"]); checked: modelData.enabled; onClicked: backend.togglePlugin(modelData.id,checked) }
         C.ActionButton { text: backend.translate("Удалить",backend.configuration["general.language"]); onClicked: backend.removePlugin(modelData.id) }
        }
        Text { visible: !!modelData.error; text: modelData.error||""; color: C.Theme.error; Layout.fillWidth: true; wrapMode: Text.Wrap }
        Repeater { model: modelData.settings||[]
         RowLayout { Layout.fillWidth: true
          Text { text: modelData.label; color: C.Theme.muted; Layout.fillWidth: true }
          C.Input { Layout.preferredWidth: 220; text: backend.configuration.extensions[modelData.key]||modelData.default||""; onEditingFinished: {var values=backend.configuration.extensions;values[modelData.key]=text;backend.setSetting("extensions",values);} }
         }
        }
       }
      }
     }
     Repeater { model: backend.plugins.filter(function(p){return p.enabled&&(p.settings||[]).length>0;})
      ColumnLayout { visible: settingsPage.selected==="plugin:"+modelData.id; Layout.fillWidth: true; spacing: 12
       Text { text: modelData.name; color: C.Theme.text; font.pixelSize: 18 }
       Repeater { model: modelData.settings
        RowLayout { Layout.fillWidth: true
         Text { text: modelData.label; color: C.Theme.muted; Layout.fillWidth: true }
         C.Input { Layout.preferredWidth: 230; text: backend.configuration.extensions[modelData.key]||modelData.default||""; onEditingFinished: {var values=backend.configuration.extensions;values[modelData.key]=text;backend.setSetting("extensions",values);} }
        }
       }
      }
     }
     ColumnLayout {
      visible: settingsPage.selected==="about"; spacing: 14
      Image { source: appBase+"assets/app_logo.png"; Layout.preferredWidth: 64; Layout.preferredHeight: 64 }
      Text { text: "EduCode"; color: C.Theme.text; font.pixelSize: 28; font.weight: Font.DemiBold }
      Text { text: backend.translate("Версия 0.2 · 2026\nСоздал Zetsubou",backend.configuration["general.language"]); color: C.Theme.muted; font.pixelSize: 15; lineHeight: 1.6 }
      Text { text: backend.translate("Простая IDE для изучения Python.",backend.configuration["general.language"]); color: C.Theme.faint; font.pixelSize: 12 }
     }
     Item { Layout.preferredHeight: 32 }
    }
   }
  }
 }
 C.Modal { id: resetDialog; title: backend.translate("Сбросить настройки?",backend.configuration["general.language"]); contentItem: ColumnLayout { spacing: 16
  Text { text: backend.translate("Вернутся стандартные значения. Проекты и файлы останутся.",backend.configuration["general.language"]); color: C.Theme.muted; Layout.fillWidth: true; wrapMode: Text.Wrap }
  RowLayout { Layout.alignment: Qt.AlignRight
   C.ActionButton { text: backend.translate("Отмена",backend.configuration["general.language"]); subtle: true; onClicked: resetDialog.close() }
   C.ActionButton { text: backend.translate("Сбросить",backend.configuration["general.language"]); onClicked: {backend.resetSettings();resetDialog.close();} }
  }
 } }
}
