// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#pragma once
#include <QObject>
#include <QProcess>
#include <QtQml/qqmlregistration.h>

namespace qor {
class InstallerState : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString phase READ phase NOTIFY changed FINAL)
    Q_PROPERTY(QString message READ message NOTIFY changed FINAL)
    Q_PROPERTY(QString target READ target NOTIFY changed FINAL)
    Q_PROPERTY(QString serial READ serial NOTIFY changed FINAL)
    Q_PROPERTY(QString provenance READ provenance NOTIFY changed FINAL)
    Q_PROPERTY(bool writing READ writing NOTIFY changed FINAL)
    Q_PROPERTY(bool retryAllowed READ retryAllowed NOTIFY changed FINAL)
public:
    explicit InstallerState(QObject *parent = nullptr);
    InstallerState(const QString &program, const QStringList &prefix, QObject *parent = nullptr);
    QString phase() const { return m_phase; }
    QString message() const { return m_message; }
    QString target() const { return m_target; }
    QString serial() const { return m_serial; }
    QString provenance() const { return m_provenance; }
    bool writing() const { return m_transactionActive || m_phase == u"rebooting"; }
    bool retryAllowed() const { return !m_failedTransaction && !writing(); }
    Q_INVOKABLE void inspect();
    Q_INVOKABLE void review();
    Q_INVOKABLE void install(const QString &confirmation, bool eraseAcknowledged);
    Q_INVOKABLE void reboot();
Q_SIGNALS:
    void changed();

private:
    void start(const QStringList &arguments);
    void receive();
    void fail(const QString &message);
    QProcess m_process;
    QString m_program;
    QStringList m_prefix;
    QString m_phase = QStringLiteral("idle");
    QString m_message;
    QString m_target;
    QString m_serial;
    QString m_provenance;
    QByteArray m_pending;
    bool m_transactionActive = false;
    bool m_failedTransaction = false;
    bool m_completionSeen = false;
    bool m_inventorySeen = false;
};
} // namespace qor
