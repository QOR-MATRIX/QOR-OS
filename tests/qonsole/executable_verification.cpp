// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#include "executable_verification.h"

#include <QtCore/QDir>
#include <QtCore/QElapsedTimer>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtGui/QGuiApplication>
#include <QtGui/QImage>
#include <QtGui/QKeyEvent>
#include <QtQml/QQmlApplicationEngine>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>

#include <utility>

using namespace Qt::StringLiterals;

namespace {
class Verification final : public QObject
{
public:
    Verification(QGuiApplication &app, QQuickWindow *window, QQuickItem *ui, QString output)
        : QObject(&app), m_app(app), m_window(window), m_ui(ui), m_output(std::move(output))
    {
        connect(window, &QQuickWindow::frameSwapped, this, [this] { ++m_frames; });
        connect(&m_timer, &QTimer::timeout, this, [this] { tick(); });
        m_elapsed.start();
        m_timer.start(50);
        window->requestActivate();
    }

private:
    void fail(const QString &reason)
    {
        m_timer.stop();
        qCritical().noquote() << u"QONSOLE_EXECUTABLE_FAILED "_s + reason;
        m_app.exit(1);
    }

    bool capture(const QString &name)
    {
        const QString file = QDir(m_output).filePath(name + u".png"_s);
        if (QFile::exists(file) || !m_window->grabWindow().save(file)) {
            fail(u"capture missing or would overwrite evidence"_s);
            return false;
        }
        return true;
    }

    void key(int code)
    {
        QKeyEvent press(QEvent::KeyPress, code, Qt::NoModifier);
        QKeyEvent release(QEvent::KeyRelease, code, Qt::NoModifier);
        QCoreApplication::sendEvent(m_window, &press);
        QCoreApplication::sendEvent(m_window, &release);
    }

    void tick()
    {
        if (m_elapsed.elapsed() > 5000) {
            fail(u"five-second flow watchdog"_s);
            return;
        }
        QQuickItem *focus = m_window->activeFocusItem();
        if (!m_window->isVisible() || !m_window->isExposed() || !m_window->isActive() || !focus)
            return;
        const QString page = m_ui->property("page").toString();
        if (m_stage == 0 && m_frames > 0 && page == u"home"_s && focus->objectName() == u"openSample"_s) {
            if (m_window->title() != u"QOR OS · Qonsole"_s || QCoreApplication::applicationName() != u"qor-qonsole"_s) {
                fail(u"wrong visible/application identity"_s);
                return;
            }
            m_home = focus;
            m_homeFrames = m_frames;
            if (!capture(u"home"_s))
                return;
            m_stage = 1;
            key(Qt::Key_Return);
        } else if (m_stage == 1 && m_frames > m_homeFrames && page == u"sample"_s
                   && focus->objectName() == u"samplePage"_s) {
            m_sampleFrames = m_frames;
            if (!capture(u"sample"_s))
                return;
            m_stage = 2;
            key(Qt::Key_Escape);
        } else if (m_stage == 2 && m_frames > m_sampleFrames && page == u"home"_s && m_home && focus == m_home.data()
                   && m_ui->property("homeIndex").toInt() == 0) {
            if (!capture(u"returned-home"_s))
                return;
            m_timer.stop();
            const QJsonObject result { { u"application"_s, QCoreApplication::applicationName() },
                                       { u"title"_s, m_window->title() },
                                       { u"input"_s, u"injected-Qt-key-events"_s },
                                       { u"restoredFocus"_s, focus->objectName() },
                                       { u"sameItem"_s, true },
                                       { u"selectedIndex"_s, 0 },
                                       { u"homeFrames"_s, m_homeFrames },
                                       { u"sampleFrames"_s, m_sampleFrames },
                                       { u"returnedFrames"_s, m_frames } };
            qInfo().noquote() << "QONSOLE_EXECUTABLE_FLOW" << QJsonDocument(result).toJson(QJsonDocument::Compact);
            m_app.exit(0);
        }
    }

    QGuiApplication &m_app;
    QQuickWindow *m_window;
    QQuickItem *m_ui;
    QString m_output;
    QPointer<QQuickItem> m_home;
    QTimer m_timer;
    QElapsedTimer m_elapsed;
    int m_frames = 0;
    int m_homeFrames = 0;
    int m_sampleFrames = 0;
    int m_stage = 0;
};
} // namespace

bool startQonsoleVerification(QGuiApplication &app, QQmlApplicationEngine &engine, const QString &outputDirectory)
{
    const QFileInfo output(outputDirectory);
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
    auto *ui = window ? window->findChild<QQuickItem *>(u"qonsoleLauncher"_s) : nullptr;
    if (!output.isAbsolute() || !output.isDir() || output.isSymLink() || !window || !ui) {
        qCritical("Local verification requires its actual window and an existing absolute output directory");
        return false;
    }
    new Verification(app, window, ui, outputDirectory);
    return true;
}
