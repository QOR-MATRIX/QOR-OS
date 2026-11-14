// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
//
// A game from the Store, played by the real QQ Player (QOR-0021, docs/STORE.md in QOR OS). The demonstration's Ember Climb is
// laid out from its signed package exactly as the Store installs it: every entry of the archive, the manifest and its
// signature beside the files, and nothing else. The Player is started on the scene the game's own game.json names, as the
// Library starts it, behind its walls, and must reach its first frame, play the scene it was given, and quit cleanly.
//
// The package is read from QOR OS's tests/store/fixtures-demo/, and never signed again here.

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <QtEndian>

namespace {

// The entries of a package's archive: stored, never compressed, one after another from the first byte, as the Store's
// builder writes them and the Store reads them. Empty if it is anything else.
QList<std::pair<QString, QByteArray>> storedEntries(const QByteArray &archive)
{
    QList<std::pair<QString, QByteArray>> entries;
    const auto *bytes = reinterpret_cast<const uchar *>(archive.constData());
    qsizetype at = 0;
    while (at + 30 <= archive.size() && qFromLittleEndian<quint32>(bytes + at) == 0x04034b50u) {
        const uchar *header = bytes + at;
        const quint16 method = qFromLittleEndian<quint16>(header + 8);
        const quint32 size = qFromLittleEndian<quint32>(header + 18);
        const quint16 nameLength = qFromLittleEndian<quint16>(header + 26);
        const quint16 extraLength = qFromLittleEndian<quint16>(header + 28);
        const qsizetype data = at + 30 + nameLength + extraLength;
        if (method != 0 || data + qsizetype(size) > archive.size())
            return {};
        entries.append({QString::fromUtf8(archive.mid(at + 30, nameLength)), archive.mid(data, size)});
        at = data + size;
    }
    return entries;
}

QJsonObject readJson(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll()).object() : QJsonObject();
}

} // namespace

class TestStoreGame : public QObject
{
    Q_OBJECT

private slots:
    void theRealPlayerPlaysAStoreInstallFromTheSceneItsGameJsonNames()
    {
        QFile package(QStringLiteral(QOR_STORE_DEMO_GAME));
        QVERIFY2(package.open(QIODevice::ReadOnly), qPrintable(package.fileName()));
        const auto entries = storedEntries(package.readAll());
        QVERIFY(entries.size() > 3);

        // As the Store installs it: items/<id>/, holding the archive's every entry.
        QTemporaryDir store;
        const QString folder = QDir(store.path()).filePath(QStringLiteral("items/org.example.demo.game.ember-climb"));
        QStringList names;
        for (const auto &[name, data] : entries) {
            QVERIFY2(!name.isEmpty() && !name.startsWith(QLatin1Char('/')) && !name.contains(QLatin1String("..")), qPrintable(name));
            const QString path = QDir(folder).filePath(name);
            QVERIFY(QDir().mkpath(QFileInfo(path).path()));
            QFile out(path);
            QVERIFY(out.open(QIODevice::WriteOnly));
            QCOMPARE(out.write(data), data.size());
            names.append(name);
        }
        QVERIFY(names.contains(QStringLiteral("manifest.json")) && names.contains(QStringLiteral("manifest.sig")));

        // The scene game.json names, and nothing guessed: the Library's rule.
        const QJsonObject game = readJson(QDir(folder).filePath(QStringLiteral("game.json")));
        QCOMPARE(game.value(QStringLiteral("format")).toInt(), 1);
        QCOMPARE(game.value(QStringLiteral("kind")).toString(), QStringLiteral("game"));
        const QString relative = game.value(QStringLiteral("scene")).toString();
        QVERIFY2(relative.startsWith(QStringLiteral("scenes/")) && relative.count(QLatin1Char('/')) == 1, qPrintable(relative));
        const QString scene = QDir(folder).filePath(relative);
        QVERIFY(QFileInfo(scene).isFile());

        // Behind its walls, as the Player always is: the environment says nothing to lower them.
        QTemporaryDir out;
        const QString result = QDir(out.path()).filePath(QStringLiteral("measure.json"));
        QProcess player;
        player.setProgram(QStringLiteral(QQ_PLAYER));
        // As Prism starts it, the scene after "--", and asked for one measured result.
        player.setArguments({QStringLiteral("--measure"), result, QStringLiteral("--"), scene});
        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        environment.remove(QStringLiteral("QQ_PLAYER_SANDBOX"));
        player.setProcessEnvironment(environment);
        player.setProcessChannelMode(QProcess::MergedChannels);
        player.start();
        const bool finished = player.waitForFinished(60000);
        const QByteArray said = player.readAll();
        if (!finished) {
            player.kill();
            player.waitForFinished();
        }
        QVERIFY2(finished, said.constData());
        QVERIFY2(player.exitStatus() == QProcess::NormalExit && player.exitCode() == 0, said.constData());
#ifdef Q_OS_LINUX
        QVERIFY2(said.contains("QQ Player walled in"), said.constData());
#endif

        const QJsonObject measured = readJson(result);
        QCOMPARE(measured.value(QStringLiteral("problem")).toString(), QString());
        QCOMPARE(measured.value(QStringLiteral("logicErrors")).toArray(), QJsonArray());
        // The scene it was given, from the folder the Store installed it in, and no other.
        QCOMPARE(QUrl(measured.value(QStringLiteral("scene")).toString()).toLocalFile(), QFileInfo(scene).absoluteFilePath());
        QVERIFY(measured.value(QStringLiteral("firstFrameMs")).toDouble() > 0);
    }
};

QTEST_GUILESS_MAIN(TestStoreGame)
#include "tst_storegame.moc"
