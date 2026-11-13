// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
// Inert protocol fixture: never opens a device or invokes an install command.
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const auto args = app.arguments();
    if (args.size() < 3)
        return 2;
    const QString mode = args[1];
    const QString action = args[2];
    QFile output;
    if (!output.open(stdout, QIODevice::WriteOnly))
        return 3;
    const auto emitObject = [&](const QJsonObject &object) {
        output.write(QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n');
        output.flush();
    };
    if (action == u"inspect") {
        if (mode == u"oversize") {
            output.write(QByteArray(40000, 'x'));
        } else if (mode == u"invalid") {
            output.write("<img src='https://invalid.test/not-fetched'>\n");
        } else if (mode == u"incomplete") {
            output.write("{");
        } else {
            emitObject({ { QStringLiteral("type"), QStringLiteral("inventory") },
                         { QStringLiteral("disk"),
                           QJsonObject { { QStringLiteral("path"), QStringLiteral("/dev/vda") },
                                         { QStringLiteral("serial"), mode == u"bad-inventory"
                                                                         ? QStringLiteral("/dev/sda")
                                                                         : QStringLiteral("qor-0123456789abcdef") },
                                         { QStringLiteral("bytes"), 16LL * 1024 * 1024 * 1024 } } },
                         { QStringLiteral("source_revision"), QString(40, u'a') },
                         { QStringLiteral("image_sha256"), QString(64, u'b') },
                         { QStringLiteral("encryption"), false } });
        }
        return mode == u"inventory-fail" ? 1 : 0;
    }
    if (action == u"install") {
        if (mode != u"early-complete")
            emitObject({ { QStringLiteral("type"), QStringLiteral("writing") } });
        if (mode != u"no-complete")
            emitObject({ { QStringLiteral("type"), QStringLiteral("complete") } });
        return mode == u"complete-fail" ? 1 : 0;
    }
    return action == u"reboot" ? 0 : 2;
}
