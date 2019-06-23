import QtQuick 2.9
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.2
import org.kde.mauikit 1.0 as Maui
import "../../view_models"
import "../../utils/Help.js" as H

Maui.Dialog
{
    property string pathToRemove : ""

    maxWidth: unit * 600
    maxHeight: unit * 500
    page.margins: 0
    defaultButtons: true
    acceptButton.text: qsTr("Add")
    rejectButton.text: qsTr("Remove")

    onRejected:
    {
        var index = sources.currentIndex
        var url = sources.model.get(index).url
        pathToRemove = url
        confirmationDialog.title = "Remove source"
        confirmationDialog.message = "Are you sure you want to remove the source: \n "+url
        confirmationDialog.open()
    }

    onAccepted:
    {
        fmDialog.onlyDirs = true
        fmDialog.show(function(paths)
        {

            console.log("SCAN DIR <<", paths)
            for(var i in paths)
                listModel.append({url: paths[i]})
            vvave.scanDir([paths])
        })

        getSources()
    }

    Maui.Dialog
    {
        id: confirmationDialog
        onAccepted:
        {
            if(pathToRemove.length>0)
                if(vvave.removeSource(pathToRemove))
                    H.refreshCollection()
            getSources()
            confirmationDialog.close()
        }
        onRejected: confirmationDialog.close()
    }

    Maui.Holder
    {
        anchors.fill: parent
        visible: !sources.count
        emoji: "qrc:/assets/MusicCloud.png"
        isMask: false
        title : "No Sources!"
        body: "Add new sources to organize and play your music collection"
        emojiSize: iconSizes.huge
    }

    BabeList
    {
        id: sources
        anchors.fill: parent
        headBar.visible: false
        headBarExit: false
        headBarTitle: qsTr("Sources")
        Layout.fillWidth: true
        Layout.fillHeight: true
        width: parent.width

        onExit: close()

        ListModel { id: listModel }

        model: listModel

        delegate: Maui.LabelDelegate
        {
            id: delegate
            label: url

            Connections
            {
                target: delegate
                onClicked: sources.currentIndex = index
            }
        }
    }

    onOpened: getSources()

    function getSources()
    {
        sources.model.clear()
        var folders = vvave.getSourceFolders()
        for(var i in folders)
            sources.model.append({url : folders[i]})
    }
}
