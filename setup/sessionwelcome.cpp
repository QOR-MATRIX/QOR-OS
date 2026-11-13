// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#include "sessionwelcome.h"

#include <QProcess>
#include <QProcessEnvironment>
#include <QTimer>
#include <QVariant>
using namespace Qt::StringLiterals;
namespace qor {
SessionWelcome::SessionWelcome(QObject *desktop, const QString &program, QObject *appearance, QObject *parent)
    : QObject(parent), m_desktop(desktop), m_appearance(appearance), m_program(program)
{
    if (!desktop || !desktop->property("locked").isValid()) {
        m_desktop.clear();
        return;
    }
    if (!connect(desktop, SIGNAL(lockedChanged()), this, SLOT(check()))) {
        m_desktop.clear();
        return;
    }
    // Let the host connect diagnostics before an already-unlocked session
    // attempts its first launch.
    QTimer::singleShot(0, this, &SessionWelcome::check);
}
void SessionWelcome::check()
{
    if (!m_desktop || m_attempted || m_desktop->property("locked").toBool())
        return;
    m_attempted = true;
    QProcess process;
    auto environment = QProcessEnvironment::systemEnvironment();
    const bool motionKnown = m_appearance && m_appearance->property("reduceMotion").isValid();
    environment.insert(u"QOR_REDUCED_MOTION"_s,
                       !motionKnown || m_appearance->property("reduceMotion").toBool() ? u"1"_s : u"0"_s);
    environment.insert(u"QOR_INCREASE_CONTRAST"_s,
                       m_appearance && m_appearance->property("increaseContrast").toBool() ? u"1"_s : u"0"_s);
    environment.remove(u"XDG_ACTIVATION_TOKEN"_s);
    process.setProcessEnvironment(environment);
    process.setProgram(m_program);
    process.setArguments({ u"--first-run"_s });
    const bool started = process.startDetached();
    Q_EMIT launchAttempted(started);
}
} // namespace qor
