// A picture on a game's screen (P3.8): a logo, an icon, a portrait. An image file in the project (`assets/`), drawn
// smooth, or `crisp` for pixel art: every pixel kept square, never blurred.
// Changed for QOR OS: the drawing stays bound to this screen part; readiness uses its actual Image type.

pragma ComponentBehavior: Bound

import QtQuick
import QQ

ScreenPart {
    id: picture

    readonly property string kind: "ScreenImage"
    readonly property var fields: ["name", "screen", "anchor", "insetX", "insetY", "width", "height", "source", "crisp"]
    readonly property var ranges: ({ insetX: [-2000, 2000], insetY: [-2000, 2000], width: [1, 1280], height: [1, 720] })
    readonly property var choices: ({ anchor: places })
    readonly property var fileTypes: ({ source: ["Images (*.png *.jpg *.jpeg *.webp)"] })

    name: "Image"
    /// Points: the image is fitted inside, keeping its shape.
    property real width: 128
    property real height: 128
    /// The image file, kept relative to the scene file.
    property url source
    /// Pixel art: nearest pixels, no smoothing.
    property bool crisp: false

    /// Ready once the file is read; the error if it could not be.
    readonly property bool ready: {
        const image = content as Image
        return image !== null && image.status === Image.Ready
    }

    drawing: Component {
        Image {
            width: picture.width * picture.unit
            height: picture.height * picture.unit
            source: SceneIO.resolvedUrl(picture, picture.source)
            fillMode: Image.PreserveAspectFit
            smooth: !picture.crisp
            mipmap: !picture.crisp
            asynchronous: false
        }
    }
}
