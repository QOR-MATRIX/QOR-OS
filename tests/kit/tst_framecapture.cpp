// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
// A hidden window receives synthetic frame signals. This exercises the real benchmark collector, not graphics hardware.
#include "framestats.h"
#include "shellbenchmark.h"

#include <QtCore/QEvent>
#include <QtCore/QHash>
#include <QtCore/QPointer>
#include <QtCore/qscopeguard.h>
#include <chrono>
#include <thread>

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
class TimerEvents : public QObject
{
public:
    QObject *lastReceiver = nullptr;
    QEvent::Type lastType = QEvent::None;
    QHash<QObject *, int> counts;

    bool eventFilter(QObject *receiver, QEvent *event) override
    {
        lastReceiver = receiver;
        lastType = event->type();
        if (lastType == QEvent::Timer)
            ++counts[receiver];
        return false;
    }
};

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
    void reportWriteFailureFailsTheCommand_data()
    {
        QTest::addColumn<bool>("fullSink");
        QTest::newRow("directory") << false;
#if defined(Q_OS_LINUX)
        // A kernel-provided sink that always refuses writes, with no physical disk or user data involved.
        QTest::newRow("full-sink") << true;
#endif
    }

    void reportWriteFailureFailsTheCommand()
    {
        QFETCH(bool, fullSink);
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        const QString sentinel = folder.filePath(QStringLiteral("keep"));
        QFile keep(sentinel);
        QVERIFY(keep.open(QIODevice::WriteOnly));
        QCOMPARE(keep.write("unchanged"), 9);
        keep.close();

        QProcess child;
        child.start(QCoreApplication::applicationFilePath(),
                    { QStringLiteral("--capture"), fullSink ? QStringLiteral("/dev/full") : folder.path() });
        QVERIFY2(child.waitForFinished(15000), qPrintable(child.errorString()));
        QCOMPARE(child.exitStatus(), QProcess::NormalExit);
        QCOMPARE(child.exitCode(), 1);
        QVERIFY(child.readAllStandardError().contains("Could not write benchmark report."));
        QVERIFY(keep.open(QIODevice::ReadOnly));
        QCOMPARE(keep.readAll(), QByteArrayLiteral("unchanged"));
    }

    void aReportReplacesStaleOutputWithCompleteJson()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        const QString path = folder.filePath(QStringLiteral("report.json"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray stale(16384, 'x');
        QCOMPARE(file.write(stale), stale.size());
        file.close();

        QProcess child;
        child.start(QCoreApplication::applicationFilePath(), { QStringLiteral("--capture"), path });
        QVERIFY2(child.waitForFinished(15000), qPrintable(child.errorString()));
        QCOMPARE(child.exitStatus(), QProcess::NormalExit);
        QCOMPARE(child.exitCode(), 0);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QJsonParseError error;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
        QCOMPARE(error.error, QJsonParseError::NoError);
        QVERIFY(document.isObject());
        QCOMPARE(document.object().value(QStringLiteral("tool")).toString(), QStringLiteral("synthetic-frame-test"));
        QVERIFY(document.object().value(QStringLiteral("frames")).toInt() > 0);
    }

#if defined(Q_OS_LINUX)
    void stdoutWriteFailureFailsTheCommand()
    {
        QTemporaryDir folder;
        QVERIFY(folder.isValid());
        const QString path = folder.filePath(QStringLiteral("report.json"));
        QProcess child;
        child.setStandardOutputFile(QStringLiteral("/dev/full"));
        child.start(QCoreApplication::applicationFilePath(), { QStringLiteral("--capture"), path });
        QVERIFY2(child.waitForFinished(15000), qPrintable(child.errorString()));
        QCOMPARE(child.exitStatus(), QProcess::NormalExit);
        QCOMPARE(child.exitCode(), 1);
        QVERIFY(child.readAllStandardError().contains("Could not write benchmark report."));
        QVERIFY(QFile::exists(path));
    }
#endif

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

    void idleProbeDoesNotWakeRepeatedly()
    {
        QQuickWindow window;
        qor::FrameStats probe;
        TimerEvents events;
        QTimer frames;
        QPointer<QObject> publisher;
        auto *application = QCoreApplication::instance();
        application->installEventFilter(&events);
        const auto removeFilter = qScopeGuard([&] { application->removeEventFilter(&events); });
        // Identify the actual timer from an update emitted during its Timer event, without private-member access.
        QObject::connect(&probe, &qor::FrameStats::updated, &events, [&] {
            if (events.lastType == QEvent::Timer && events.lastReceiver != &frames)
                publisher = events.lastReceiver;
        });
        probe.setWindow(&window);
        QObject::connect(&frames, &QTimer::timeout, &window, &QQuickWindow::frameSwapped);
        frames.start(10);
        QTRY_VERIFY(!probe.isIdle());
        frames.stop();
        QTRY_VERIFY(probe.isIdle());
        QVERIFY(publisher);
        const int atIdle = events.counts.value(publisher.data());
        QTest::qWait(750);
        QCOMPARE(events.counts.value(publisher.data()), atIdle);

        // A real render thread emits frameSwapped. Resuming from another thread must restart GUI publishing safely.
        std::thread rendering([&] {
            for (int i = 0; i < 80; ++i) {
                window.frameSwapped();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
        const auto join = qScopeGuard([&] {
            if (rendering.joinable())
                rendering.join();
        });
        QTRY_VERIFY(!probe.isIdle());
        rendering.join();
        QTRY_VERIFY(probe.isIdle());

        // Detaching before a queued restart runs must leave the old timer stopped.
        std::thread lastFrame([&] { window.frameSwapped(); });
        lastFrame.join();
        probe.setWindow(nullptr);
        const int detached = events.counts.value(publisher.data());
        QTest::qWait(300);
        QCOMPARE(events.counts.value(publisher.data()), detached);
    }

    void anIdleStatusRedrawDoesNotKeepPublishing()
    {
        QQuickWindow window;
        qor::FrameStats probe;
        TimerEvents events;
        QTimer frames;
        QPointer<QObject> publisher;
        auto *application = QCoreApplication::instance();
        application->installEventFilter(&events);
        const auto removeFilter = qScopeGuard([&] { application->removeEventFilter(&events); });
        QObject::connect(&probe, &qor::FrameStats::updated, &events, [&] {
            if (events.lastType == QEvent::Timer && events.lastReceiver != &frames)
                publisher = events.lastReceiver;
            // A Monitor label changing to idle can itself cause one more presented frame.
            if (probe.isIdle())
                QTimer::singleShot(0, &window, [&] { window.frameSwapped(); });
        });
        probe.setWindow(&window);
        QObject::connect(&frames, &QTimer::timeout, &window, &QQuickWindow::frameSwapped);
        frames.start(10);
        QTRY_VERIFY(!probe.isIdle());
        frames.stop();
        QTRY_VERIFY(probe.isIdle());
        QVERIFY(publisher);
        const int atIdle = events.counts.value(publisher.data());
        QTest::qWait(1000);
        // The one status redraw gets one final sample, with no idle/active feedback loop.
        QCOMPARE(events.counts.value(publisher.data()) - atIdle, 1);
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
