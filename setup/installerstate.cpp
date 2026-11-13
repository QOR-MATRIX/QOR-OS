// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#include "installerstate.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QRegularExpression>

using namespace Qt::StringLiterals;
namespace qor {
InstallerState::InstallerState(QObject *parent)
    : InstallerState(u"/usr/bin/python3"_s, { u"/usr/libexec/qor-os/vm_install_backend.py"_s }, parent)
{ }
InstallerState::InstallerState(const QString &program, const QStringList &prefix, QObject *parent)
    : QObject(parent), m_program(program), m_prefix(prefix)
{
    connect(&m_process, &QProcess::readyReadStandardOutput, this, &InstallerState::receive);
    connect(&m_process, &QProcess::readyReadStandardError, this, [this] {
        // Raw backend errors can contain input. They belong to the guest's
        // dedicated diagnostics, never an automatic rich-text label or host log.
        m_process.readAllStandardError();
    });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            m_failedTransaction = m_failedTransaction || m_transactionActive;
            m_transactionActive = false;
        }
        fail(tr("The installation service is unavailable. No automatic retry will occur."));
    });
    connect(&m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        receive();
        if (!m_pending.isEmpty()) {
            m_pending.clear();
            fail(tr("The installation service ended with an incomplete response."));
        }
        const bool completed = m_completionSeen && m_phase == u"writing" && !m_failedTransaction && code == 0
                               && status == QProcess::NormalExit;
        const bool checked = m_inventorySeen && m_phase == u"checking" && code == 0 && status == QProcess::NormalExit;
        const bool incomplete =
            (m_phase == u"checking" && !checked) || (m_phase == u"writing" && !completed) || m_phase == u"starting";
        const bool failed = code != 0 || status != QProcess::NormalExit || incomplete;
        m_failedTransaction = m_failedTransaction || (m_transactionActive && failed);
        m_transactionActive = false;
        if (failed)
            fail(tr("The operation did not finish successfully. Preserve the selected disk and inspect diagnostics."));
        else if (checked) {
            m_phase = u"choose"_s;
            m_message = tr("The disposable target is available. Nothing has been selected or erased.");
        } else if (completed) {
            m_phase = u"complete"_s;
            m_message = tr("Installation finished. Boot from the installed virtual disk to verify first run.");
        }
        Q_EMIT changed();
    });
}
void InstallerState::fail(const QString &message)
{
    if (m_phase == u"checking") {
        m_serial.clear();
        m_target.clear();
        m_provenance.clear();
    }
    m_phase = u"error"_s;
    m_message = message;
    m_failedTransaction = m_failedTransaction || m_transactionActive;
    if (m_process.state() != QProcess::NotRunning && !m_transactionActive)
        m_process.kill();
    Q_EMIT changed();
}
void InstallerState::start(const QStringList &arguments)
{
    if (m_process.state() != QProcess::NotRunning)
        return;
    m_pending.clear();
    m_completionSeen = false;
    m_inventorySeen = false;
    QProcessEnvironment environment;
#ifdef Q_OS_WIN
    // Only the inert protocol fixture runs here; the installer executable is Linux-only.
    const auto system = QProcessEnvironment::systemEnvironment();
    for (const QString &name : { u"PATH"_s, u"SystemRoot"_s, u"WINDIR"_s, u"TEMP"_s, u"TMP"_s })
        if (system.contains(name))
            environment.insert(name, system.value(name));
#else
    environment.insert(u"PATH"_s, u"/usr/sbin:/usr/bin"_s);
#endif
    environment.insert(u"LANG"_s, u"C.UTF-8"_s);
    environment.insert(u"HOME"_s, u"/root"_s);
    m_process.setProcessEnvironment(environment);
    m_process.setProgram(m_program);
    m_process.setArguments(m_prefix + arguments);
    m_process.start();
}
void InstallerState::inspect()
{
    if (m_process.state() != QProcess::NotRunning || writing() || m_failedTransaction)
        return;
    m_serial.clear();
    m_target.clear();
    m_provenance.clear();
    m_message = tr("Checking the assigned virtual disk and firmware identity…");
    m_phase = u"checking"_s;
    Q_EMIT changed();
    start({ u"inspect"_s });
}
void InstallerState::review()
{
    if (m_phase != u"choose" || m_serial.isEmpty() || m_process.state() != QProcess::NotRunning)
        return;
    m_phase = u"review"_s;
    Q_EMIT changed();
}
void InstallerState::install(const QString &confirmation, bool eraseAcknowledged)
{
    if (m_phase != u"review" || !eraseAcknowledged || m_serial.isEmpty() || confirmation != m_serial
        || m_process.state() != QProcess::NotRunning)
        return;
    m_phase = u"starting"_s;
    m_transactionActive = true;
    m_message = tr("Rechecking the exact target and offline payload before writing…");
    Q_EMIT changed();
    start({ u"install"_s, u"--confirm-serial"_s, m_serial });
}
void InstallerState::reboot()
{
    if (m_phase != u"complete" || m_process.state() != QProcess::NotRunning)
        return;
    m_phase = u"rebooting"_s;
    m_message = tr("Rebooting this disposable VM…");
    Q_EMIT changed();
    start({ u"reboot"_s, u"--confirm-serial"_s, m_serial });
}
void InstallerState::receive()
{
    m_pending += m_process.readAllStandardOutput();
    if (m_pending.size() > 32768) {
        m_pending.clear();
        fail(tr("The installation service sent an oversized response."));
        return;
    }
    while (m_pending.contains('\n')) {
        const qsizetype end = m_pending.indexOf('\n');
        const QByteArray line = m_pending.first(end);
        m_pending.remove(0, end + 1);
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(line, &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) {
            fail(tr("The installation service sent an invalid response."));
            return;
        }
        const auto object = document.object();
        const QString type = object.value(u"type"_s).toString();
        if (type == u"inventory" && m_phase == u"checking" && !m_inventorySeen) {
            const auto disk = object.value(u"disk"_s).toObject();
            const QString serial = disk.value(u"serial"_s).toString();
            const QString path = disk.value(u"path"_s).toString();
            const qint64 bytes = disk.value(u"bytes"_s).toInteger(-1);
            const QString source = object.value(u"source_revision"_s).toString();
            const QString image = object.value(u"image_sha256"_s).toString();
            if (!QRegularExpression(u"^qor-[0-9a-f]{16}$"_s).match(serial).hasMatch()
                || !QRegularExpression(u"^/dev/[a-z][a-z0-9]*$"_s).match(path).hasMatch()
                || bytes < 8LL * 1024 * 1024 * 1024 || bytes > 64LL * 1024 * 1024 * 1024
                || !QRegularExpression(u"^[0-9a-f]{40}$"_s).match(source).hasMatch()
                || !QRegularExpression(u"^[0-9a-f]{64}$"_s).match(image).hasMatch()
                || !object.value(u"encryption"_s).isBool() || object.value(u"encryption"_s).toBool()) {
                fail(tr("The assigned disk or source identity is incomplete. Installation is disabled."));
                return;
            }
            m_serial = serial;
            m_target = tr("%1\nSerial: %2\nCapacity: %3 bytes\nVirtIO PCI slot: 00:04.0")
                           .arg(path, serial, QString::number(bytes));
            m_provenance = tr("Source: %1\nImage SHA-256: %2").arg(source, image);
            m_inventorySeen = true;
            m_message = tr("Waiting for the disk check's final result…");
        } else if (type == u"writing" && m_phase == u"starting") {
            m_phase = u"writing"_s;
            m_message = tr("The stock bootc backend is partitioning and deploying the selected disk. Diagnostics "
                           "remain available.");
        } else if (type == u"complete" && m_phase == u"writing" && !m_completionSeen) {
            m_completionSeen = true;
            m_message = tr("Waiting for the installation backend's final result…");
        } else if (type == u"error") {
            fail(tr("The target or operation was refused or failed. Preserve this disk and inspect the guest "
                    "diagnostics."));
            return;
        } else {
            fail(tr("The installation service sent an unexpected state transition."));
            return;
        }
        Q_EMIT changed();
    }
}
} // namespace qor
