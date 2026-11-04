// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
// A hidden window receives synthetic frame signals. This exercises the real benchmark collector, not graphics hardware.
#include "framestats.h"
#include "shellbenchmark.h"

#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QProcess>
#include <QtCore/QStandardPaths>
#include <QtCore/QTemporaryDir>
#include <QtCore/QTimer>
#include <QtGui/QGuiApplication>
#include <QtQuick/QQuickWindow>
#include <QtTest/QTest>

namespace {
int capture(const QString &path)
{
    QQuickWindow window;
    window.resize(320, 180);
    qor::ShellBenchmarkOptions options;
    options.tool = QStringLiteral("synthetic-frame-test");
    options.seconds = 2;
    options.outputPath = path;
    qor::runShellBenchmark({ &window }, options);
    auto *probe = window.findChild<qor::FrameStats *>();
    if (!probe)
        return 3;
    QTimer frames;
    bool gapScheduled = false;
    int previousFrames = 0;
    QObject::connect(&frames, &QTimer::timeout, &window, [&] {
        const int recorded = probe->framesRecorded();
        // The driver's warm-up ends by resetting the probe. Observe that real boundary, not a copied warm-up duration.
        if (!gapScheduled && previousFrames > 0 && recorded < previousFrames) {
            gapScheduled = true;
            QTimer::singleShot(200, &frames, [&] { frames.stop(); });
            QTimer::singleShot(700, &frames, [&] { frames.start(10); });
        }
        window.frameSwapped();
        previousFrames = probe->framesRecorded();
    });
    frames.start(10);
    return QCoreApplication::exec();
}
} // namespace

class FrameCaptureTest : public QObject
{
    Q_OBJECT
private slots:
    void continuousBenchmarkRetainsAStall()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        const QString path = folder.filePath(QStringLiteral("capture.json"));
        QProcess child;
        child.start(QCoreApplication::applicationFilePath(), { QStringLiteral("--capture"), path });
        QVERIFY2(child.waitForFinished(15000), qPrintable(child.errorString()));
        QCOMPARE(child.exitStatus(), QProcess::NormalExit);
        QCOMPARE(child.exitCode(), 0);
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QJsonParseError error;
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
        QCOMPARE(error.error, QJsonParseError::NoError);
        const QJsonArray outputs = doc.object().value(QStringLiteral("outputs")).toArray();
        QCOMPARE(outputs.size(), 1);
        const QJsonObject output = outputs.first().toObject();
        QVERIFY(output.value(QStringLiteral("frames")).toInt() > 0);
        const double maximum = output.value(QStringLiteral("maxMs")).toDouble();
        QVERIFY2(
            maximum >= 200,
            qPrintable(QStringLiteral("The real benchmark omitted its injected 500 ms stall: max %1 ms").arg(maximum)));
    }

    void theInteractiveProbeStillTreatsALongGapAsIdle()
    {
        QQuickWindow window;
        qor::FrameStats probe;
        probe.setWindow(&window);
        window.frameSwapped();
        QTest::qWait(250);
        window.frameSwapped();
        QVERIFY(probe.intervals().isEmpty());
    }

    void probeCost()
    {
        QQuickWindow window;
        qor::FrameStats probe;
        probe.setWindow(&window);
        QBENCHMARK
        {
            probe.reset();
            window.frameSwapped();
            window.frameSwapped();
        }
        QCOMPARE(probe.framesRecorded(), 1);
    }
};

int main(int argc, char **argv)
{
    QStandardPaths::setTestModeEnabled(true);
    QGuiApplication app(argc, argv);
    QGuiApplication::setQuitOnLastWindowClosed(false);
    const QStringList args = app.arguments();
    if (args.size() == 3 && args.at(1) == QStringLiteral("--capture"))
        return capture(args.at(2));
    FrameCaptureTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_framecapture.moc"
