import QtQuick 2.15
import QtWebEngine 1.10
import QtWebChannel 1.0
WebEngineView {
 id: view
 property string resource: "editor.html"
 url: appBase + "web/" + resource
 backgroundColor: Theme.editor
 webChannel: WebChannel { Component.onCompleted: registerObject("backend", backend) }
 settings.localContentCanAccessRemoteUrls: false
 settings.localContentCanAccessFileUrls: true
 settings.javascriptCanOpenWindows: false
 settings.errorPageEnabled: false
 onJavaScriptConsoleMessage: console.log("Web:", message, sourceID, lineNumber)
 onNavigationRequested: { if (!request.url.toString().startsWith(appBase + "web/")) request.action = WebEngineNavigationRequest.IgnoreRequest }
}
