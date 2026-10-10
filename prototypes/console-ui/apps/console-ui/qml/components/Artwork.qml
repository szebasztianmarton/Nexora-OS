import QtQuick

// Game artwork with asynchronous loading and a placeholder for missing or
// unreadable images. Portrait art fills (cropped proportionally); landscape
// art is fitted inside the frame over the placeholder instead of being
// stretched or heavily cropped.
Item {
    id: artwork
    property string source: ""
    property string title: ""
    property bool showPlaceholderTitle: true

    readonly property bool loaded: image.status === Image.Ready
    // Measured once per loaded source. Not bound to the image's live state:
    // changing fillMode reloads a sourceSize'd image, which would otherwise
    // form a binding loop (landscape -> fillMode -> reload -> status).
    readonly property bool landscape: _landscape
    property bool _landscape: false
    onSourceChanged: _landscape = false

    ArtworkPlaceholder {
        anchors.fill: parent
        title: artwork.title
        showTitle: artwork.showPlaceholderTitle && !artwork.loaded
        visible: !artwork.loaded || artwork.landscape
        opacity: artwork.landscape ? 0.6 : 1
    }
    Image {
        id: image
        anchors.fill: parent
        source: artwork.source
        visible: status === Image.Ready
        asynchronous: true
        cache: true
        smooth: true
        mipmap: true
        fillMode: artwork.landscape ? Image.PreserveAspectFit : Image.PreserveAspectCrop
        // Decode at display resolution only (memory friendly on large libraries).
        sourceSize.width: Math.ceil(artwork.width * 1.5)
        onStatusChanged: if (status === Image.Ready) artwork._landscape = implicitWidth > implicitHeight
    }
}
