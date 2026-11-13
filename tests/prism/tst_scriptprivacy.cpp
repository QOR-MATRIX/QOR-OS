// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
// An actual failing script keeps supplied arguments private, without turning its failure into a successful command.

#include <QtCore/QDir>
#include <QtCore/QProcess>
#include <QtCore/QProcessEnvironment>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

class TstScriptPrivacy : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void aFailedStepKeepsItsPayloadPrivate()
    {
        // The driver nests a runtime directory here: use a short private root for Unix socket path limits.
        QTemporaryDir home(QStringLiteral("/tmp/qor-script-XXXXXX"));
        QVERIFY(home.isValid());
        QVERIFY(QDir(home.path()).mkdir(QStringLiteral("cache")));
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("HOME"), home.path());
        environment.insert(QStringLiteral("TMPDIR"), home.path());
        environment.insert(QStringLiteral("XDG_CACHE_HOME"), home.path() + QStringLiteral("/cache"));

        QProcess process;
        process.setProcessEnvironment(environment);
        process.setProcessChannelMode(QProcess::MergedChannels);
        process.start(QStringLiteral(QOR_PYTHON_PROGRAM),
                      { QStringLiteral(QOR_PRISM_DRIVER), QStringLiteral("--prism"), QStringLiteral(QOR_PRISM_PROGRAM),
                        QStringLiteral("--arqade"), QStringLiteral(QOR_TEST_ARQADE), QStringLiteral("--media"),
                        QStringLiteral(QOR_TEST_MEDIA), QStringLiteral("--qq-studio"), QStringLiteral(QOR_TEST_STUDIO),
                        QStringLiteral("--qq-player"), QStringLiteral(QOR_TEST_PLAYER), QStringLiteral("--script"),
                        QStringLiteral(QOR_PRIVACY_SCRIPT) });
        QVERIFY(process.waitForStarted(10000));
        QVERIFY(process.waitForFinished(110000));
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QCOMPARE(process.exitCode(), 4);

        // Keep captured diagnostics in memory. A failed check must never print their potentially private contents.
        const QByteArray output = process.readAll();
        const QByteArray marker = QByteArrayLiteral("QOR_TEST_PRIVATE_SCRIPT_PAYLOAD");
        QVERIFY2(!output.contains(marker), "The failed script echoed its supplied payload.");
        QVERIFY2(output.contains("script: step 1 failed:"), "The failed step lost its number or failure marker.");
        QVERIFY2(output.contains("Desktop has no method noSuchScriptAction"), "The failure lost its reason.");
    }
};

QTEST_GUILESS_MAIN(TstScriptPrivacy)
#include "tst_scriptprivacy.moc"
