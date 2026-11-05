// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#include "kit.h"

#include <QCommandLineParser>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQuickItem>
#include <QQuickView>
#include <QSaveFile>
#include <QTimer>
#include <QtQml/QQmlExtensionPlugin>

#ifdef Q_OS_LINUX
#include <sys/stat.h>
#endif

Q_IMPORT_QML_PLUGIN(QOR_KitPlugin)
using namespace Qt::StringLiterals;

namespace {
QByteArray digest(const QByteArray &data) { return QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex(); }

bool save(const QString &path, const QByteArray &data)
{
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size() && file.commit();
}
} // namespace

int main(int argc, char **argv)
{
#ifdef Q_OS_LINUX
    struct stat gpu = {}, nullDevice = {};
    if (::stat("/dev/dxg", &gpu) == 0 && (::stat("/dev/null", &nullDevice) != 0 || gpu.st_rdev != nullDevice.st_rdev)) {
        qCritical("Under WSL, generate assets inside the documented GPU fence.");
        return 2;
    }
#endif
    // A build tool never opens a window on the developer's desktop.
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(u"qor-brand-assets"_s);
    QCommandLineParser parser;
    parser.addHelpOption();
    const QCommandLineOption output(u"output"_s, u"Empty output directory for the mark and provenance."_s,
                                    u"directory"_s);
    parser.addOption(output);
    parser.process(app);
    const QString path = parser.value(output);
    QDir directory(QFileInfo(path).absoluteFilePath());
    if (path.isEmpty() || directory.isRoot() || QFileInfo(path).isSymLink()
        || (directory.exists()
            && !directory.entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot).isEmpty())
        || (!directory.exists() && !QDir().mkpath(directory.path()))) {
        qCritical("Use a new, empty output directory.");
        return 2;
    }

    qor::Kit::initialize();
    QQuickView view;
    view.setColor(Qt::transparent);
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.resize(1024, 1024);
    view.setSource(QUrl(u"qrc:/qor/branding/Mark.qml"_s));
    if (view.status() == QQuickView::Error || !view.rootObject())
        return 2;

    bool captured = false;
    QObject::connect(
        &view, &QQuickWindow::frameSwapped, &app,
        [&] {
            if (captured)
                return;
            captured = true;
            QTimer::singleShot(0, &app, [&] {
                QImage image = view.grabWindow();
                if (image.isNull() || image.width() != image.height()) {
                    qCritical("The mark renderer did not produce a square image.");
                    app.exit(1);
                    return;
                }
                image = image.scaled(1024, 1024, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                int covered = 0;
                int coloured = 0;
                for (int y = 16; y < 1024; y += 32) {
                    for (int x = 16; x < 1024; x += 32) {
                        const QColor pixel = image.pixelColor(x, y);
                        covered += pixel.alpha() > 200 ? 1 : 0;
                        coloured += pixel.alpha() > 200
                                            && qMax(pixel.red(), qMax(pixel.green(), pixel.blue()))
                                                       - qMin(pixel.red(), qMin(pixel.green(), pixel.blue()))
                                                   > 20
                                        ? 1
                                        : 0;
                    }
                }
                if (covered < 400 || coloured < 300 || image.pixelColor(0, 0).alpha() > 10) {
                    qCritical("The mark's shaders did not render; use a fenced shader-capable backend.");
                    app.exit(1);
                    return;
                }
                image.setText(u"SPDX-License-Identifier"_s, u"LicenseRef-QOR-OS-Proprietary"_s);
                const QString logo = directory.filePath(u"qor-mark.png"_s);
                QSaveFile png(logo);
                if (!png.open(QIODevice::WriteOnly) || !image.save(&png, "PNG") || !png.commit()) {
                    qCritical("The mark could not be written completely.");
                    app.exit(1);
                    return;
                }
                QFile saved(logo);
                if (!saved.open(QIODevice::ReadOnly)) {
                    app.exit(1);
                    return;
                }
                QJsonObject inputs;
                for (const QString &name :
                     { u"kit/QorMark.qml"_s, u"kit/Surface.qml"_s, u"kit/Tokens.qml"_s, u"kit/shaders/surface.frag"_s,
                       u"kit/src/oklchmath.cpp"_s, u"kit/tokens/lumen.tokens.json"_s, u"tools/branding/Mark.qml"_s }) {
                    QFile source(QStringLiteral(QOR_BRAND_SOURCE_DIR) + u'/' + name);
                    if (!source.open(QIODevice::ReadOnly)) {
                        qCritical("A brand source is unavailable; provenance was not written.");
                        app.exit(1);
                        return;
                    }
                    inputs.insert(name, QString::fromLatin1(digest(source.readAll())));
                }
                const QColor background = view.rootObject()->property("backgroundColor").value<QColor>();
                QJsonObject manifest {
                    { u"SPDX-License-Identifier"_s, u"LicenseRef-QOR-OS-Proprietary"_s },
                    { u"schema"_s, 1 },
                    { u"source"_s, u"kit/QorMark.qml"_s },
                    { u"inputs"_s, inputs },
                    { u"logo"_s, QJsonObject { { u"file"_s, u"qor-mark.png"_s },
                                               { u"width"_s, 1024 },
                                               { u"height"_s, 1024 },
                                               { u"sha256"_s, QString::fromLatin1(digest(saved.readAll())) } } },
                    { u"background"_s, background.name(QColor::HexRgb) },
                    { u"qtVersion"_s, QString::fromLatin1(qVersion()) },
                };
                if (!background.isValid()
                    || !save(directory.filePath(u"manifest.json"_s), QJsonDocument(manifest).toJson())) {
                    qCritical("The brand provenance could not be written completely.");
                    app.exit(1);
                    return;
                }
                app.exit(0);
            });
        },
        Qt::QueuedConnection);
    QTimer::singleShot(15000, &app, [&] {
        qCritical("The mark renderer timed out; no usable asset is proven.");
        app.exit(1);
    });
    view.show();
    return app.exec();
}
