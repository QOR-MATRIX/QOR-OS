// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
//
// prism-test-store-install: the real StoreEngine installs one item into a folder, as the shell's Store would, with
// creator features on and QQ at this tree's version. For the suites that join the Store to QQ Studio's asset library
// across the two builds (products/qq/tests/tst_assetlibrary.cpp), which find it where QOR_PRISM_BUILD names a build of
// Prism.
//
//   prism-test-store-install <store folder> <store.pub> <catalog.json> <item id>
//   prism-test-store-install <store folder> <store.pub> --remove <item id>
//
// Exit status: 0 installed or removed, 1 refused or failed, with why on standard error, 2 used wrongly.

#include "storeengine.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QFile>
#include <QtCore/QTimer>
#include <QtCore/QUrl>

#include <cstdio>

using namespace Qt::StringLiterals;
using qor::store::StoreEngine;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QStringList arguments = app.arguments();
    if (arguments.size() != 5) {
        std::fputs("usage: prism-test-store-install <store folder> <store.pub> <catalog.json> <item id>\n", stderr);
        return 2;
    }
    QFile key(arguments.at(2));
    if (!key.open(QIODevice::ReadOnly)) {
        std::fputs("the store's key cannot be read\n", stderr);
        return 2;
    }
    StoreEngine engine(arguments.at(1), QByteArray::fromBase64(key.readAll().trimmed()));
    if (arguments.at(3) == u"--remove"_s) {
        // Removing what is installed needs nothing of the store it came from.
        if (!engine.remove(arguments.at(4))) {
            std::fputs("not removed\n", stderr);
            return 1;
        }
        std::printf("removed %s\n", qPrintable(arguments.at(4)));
        return 0;
    }
    engine.setCreatorFeatures(true);
    engine.setEngines({ { u"qq"_s, QStringLiteral(QOR_STORE_QQ_VERSION) } });
    engine.setCatalogUrl(QUrl::fromLocalFile(arguments.at(3)));
    const QString id = arguments.at(4);
    int code = 1;
    const auto fail = [&code](const QString &why) {
        std::fprintf(stderr, "%s\n", qPrintable(why));
        code = 1;
        QCoreApplication::quit();
    };
    QObject::connect(&engine, &StoreEngine::stateChanged, &app, [&] {
        if (engine.state() == StoreEngine::State::Failed)
            fail(u"The catalogue was refused: "_s + engine.error());
        else if (engine.state() == StoreEngine::State::Ready && !engine.isBusy(id) && !engine.install(id))
            fail(u"The install did not start."_s);
    });
    QObject::connect(&engine, &StoreEngine::finished, &app, [&](const QString &done, bool ok, const QString &error) {
        if (done != id)
            return;
        if (!ok)
            return fail(u"Refused: "_s + error);
        std::printf("installed %s %s\n", qPrintable(id), qPrintable(engine.installed().value(id).version));
        code = 0;
        QCoreApplication::quit();
    });
    QTimer::singleShot(60000, &app, [&] { fail(u"Nothing happened in a minute."_s); });
    // Inside the event loop, so a quit always has one to end.
    QTimer::singleShot(0, &engine, &StoreEngine::refresh);
    app.exec();
    return code;
}
