// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#pragma once
#include <QObject>
#include <QPointer>
#include <QString>

namespace qor {
// Device host only: an authenticated unlock may start the unprivileged welcome.
class SessionWelcome : public QObject
{
    Q_OBJECT
public:
    SessionWelcome(QObject *desktop, const QString &program, QObject *appearance = nullptr, QObject *parent = nullptr);
Q_SIGNALS:
    void launchAttempted(bool started);
private Q_SLOTS:
    void check();

private:
    QPointer<QObject> m_desktop;
    QPointer<QObject> m_appearance;
    QString m_program;
    bool m_attempted = false;
};
} // namespace qor
