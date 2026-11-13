// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#pragma once
#include <QByteArray>
#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

namespace qor {
void applyWelcomeAppearance(QObject *theme, const QByteArray &reducedMotion, const QByteArray &increaseContrast);

// First-run completion only. No account, key, disk or installation operations.
class SetupState : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool completed READ completed NOTIFY completedChanged FINAL)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged FINAL)
public:
    explicit SetupState(QObject *parent = nullptr);
    explicit SetupState(const QString &directory, QObject *parent = nullptr);
    bool completed() const { return m_completed; }
    QString error() const { return m_error; }
    Q_INVOKABLE bool finish();
    Q_INVOKABLE void reload();
Q_SIGNALS:
    void completedChanged();
    void errorChanged();

private:
    bool safeDirectory() const;
    void setError(const QString &message);
    QString m_directory;
    QString m_error;
    bool m_completed = false;
};
} // namespace qor
