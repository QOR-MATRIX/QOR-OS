// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
//
// Prism's command line (prism/compositor/commandline.h), built as it is in a build with the simulator
// (tst_prismoptions) and as a device's image builds it, without (tst_prismoptions_device): what only tests and a
// developer's runs need must not exist on a device, so asking for it there is an error before anything is read.
#include "commandline.h"

#include <QtCore/QCommandLineParser>
#include <QtTest/QTest>

using namespace Qt::StringLiterals;

namespace {

struct Parsed
{
    bool ok = false;
    QString error;
    qor::prism::CommandLine line;
};

Parsed parse(const QStringList &arguments)
{
    QCommandLineParser parser;
    qor::prism::addOptions(parser);
    Parsed parsed;
    parsed.ok = parser.parse(QStringList { u"prism"_s } + arguments);
    parsed.error = parser.errorText();
    if (parsed.ok)
        parsed.line = qor::prism::read(parser);
    return parsed;
}

} // namespace

class TestPrismOptions : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void whatEveryBuildTakes()
    {
        const Parsed parsed = parse({ u"--socket"_s, u"wayland-9"_s, u"--size"_s, u"1280x800"_s, u"--outputs"_s, u"2"_s,
                                      u"--spread-clients"_s, u"--surface"_s, u"home"_s });
        QVERIFY2(parsed.ok, qPrintable(parsed.error));
        QCOMPARE(parsed.line.socket, u"wayland-9"_s);
        QCOMPARE(parsed.line.size, u"1280x800"_s);
        QCOMPARE(parsed.line.outputs, 2);
        QVERIFY(parsed.line.spreadClients);
        QCOMPARE(parsed.line.surface, u"home"_s);
#ifdef QOR_HAS_SIMULATOR
        QCOMPARE(parse({}).line.services, u"embedded"_s);
#else
        QCOMPARE(parse({}).line.services, u"external"_s);
#endif
    }

    void whatOnlyTestsNeed_data()
    {
        QTest::addColumn<QStringList>("arguments");
        QTest::newRow("a script") << QStringList { u"--script"_s, u"/tmp/steps.json"_s };
        QTest::newRow("a script, joined") << QStringList { u"--script=/tmp/steps.json"_s };
        QTest::newRow("starting unlocked") << QStringList { u"--unlocked"_s };
        QTest::newRow("a screenshot") << QStringList { u"--screenshot"_s, u"/tmp/shot.png"_s };
        QTest::newRow("its delay") << QStringList { u"--delay"_s, u"100"_s };
        QTest::newRow("the benchmark") << QStringList { u"--benchmark"_s, u"4"_s };
        QTest::newRow("its report") << QStringList { u"--benchmark-out"_s, u"/tmp/report.json"_s };
    }

    void whatOnlyTestsNeed()
    {
        QFETCH(QStringList, arguments);
        const Parsed parsed = parse(arguments);
#ifdef QOR_HAS_SIMULATOR
        QVERIFY2(parsed.ok, qPrintable(parsed.error));
        const qor::prism::CommandLine &line = parsed.line;
        QVERIFY(line.unlocked || !line.script.isEmpty() || !line.screenshot.isEmpty() || line.delayMs == 100
                || line.benchmarkSeconds == 4 || !line.benchmarkOut.isEmpty());
#else
        // A device: no such option, refused as it is parsed.
        QVERIFY(!parsed.ok);
        QVERIFY2(parsed.error.startsWith(u"Unknown option"_s), qPrintable(parsed.error));
#endif
    }

#ifndef QOR_HAS_SIMULATOR
    // Nothing the device's Prism reads says it is a test's run, or holds a file a test would read or write.
    void aDeviceReadsNothingOfThem()
    {
        const qor::prism::CommandLine line = parse({ u"--surface"_s, u"library"_s }).line;
        QVERIFY(!line.unlocked);
        QVERIFY(line.script.isEmpty());
        QVERIFY(line.screenshot.isEmpty());
        QCOMPARE(line.benchmarkSeconds, 0);
        QVERIFY(line.benchmarkOut.isEmpty());
        QVERIFY(!line.automated());
    }
#endif
};

QTEST_GUILESS_MAIN(TestPrismOptions)
#include "tst_prismoptions.moc"
