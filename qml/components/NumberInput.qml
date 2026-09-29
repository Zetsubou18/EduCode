import QtQuick 2.15
import QtQuick.Controls 2.15
SpinBox {
 id: control
 implicitWidth: 130; implicitHeight: 36; editable: true
 contentItem: TextInput { text: control.textFromValue(control.value,control.locale); color: Theme.text; font.pixelSize: 13; horizontalAlignment: Qt.AlignHCenter; verticalAlignment: Qt.AlignVCenter; validator: control.validator; selectByMouse: true; inputMethodHints: Qt.ImhDigitsOnly; readOnly: !control.editable }
 background: Rectangle { color: Theme.editor; radius: 5; border.color: control.activeFocus ? Theme.accent : Theme.border }
 up.indicator: Text { width: 28; height: control.height; x: control.width-width; text: "+"; color: Theme.muted; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: 18 }
 down.indicator: Text { width: 28; height: control.height; text: "−"; color: Theme.muted; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: 18 }
}
