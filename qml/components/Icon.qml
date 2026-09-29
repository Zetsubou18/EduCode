import QtQuick 2.15
Text {
 property string name: "file"
 property int size: 18
 property var glyphs: ({copy:"\uebcc",bell:"\ueaa2",browser:"\ueb01",ai:"\ueb08",refresh:"\ueb37",home:"\ueb06",settings:"\ueb51",play:"\ueb2c",stop:"\uead7",add:"\uea60",folder:"\uea83",file:"\uea7b",search:"\uea6d",close:"\uea76",back:"\uea9b",chevron:"\ueab6",down:"\ueab4",terminal:"\uea85",console:"\ueb9b",problems:"\uea87",packages:"\ueb29",save:"\ueb4b",more:"\uea7c",open:"\uea94",check:"\ueab2"})
 text: glyphs[name] || glyphs.file
 font.family: iconFont.name
 font.pixelSize: size
 color: Theme.text
 verticalAlignment: Text.AlignVCenter
 horizontalAlignment: Text.AlignHCenter
}

