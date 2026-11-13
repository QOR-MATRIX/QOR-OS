// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#include "setupstate.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QVariant>

using namespace Qt::StringLiterals;
namespace qor {
namespace {
const QByteArray record = QByteArrayLiteral("{\"schema\":1,\"completed\":true}\n");
}
void applyWelcomeAppearance(QObject *theme, const QByteArray &reducedMotion, const QByteArray &increaseContrast)
{
    if (!theme)
        return;
    // Unknown/manual launches stay still. These two public scalar hints never
    // change a system preference or enter the completion record.
    theme->setProperty("reduceMotion", reducedMotion != "0");
    theme->setProperty("increaseContrast", increaseContrast == "1");
}
SetupState::SetupState(QObject *parent)
    : SetupState(QStandardPaths::writableLocation(QStandardPaths::StateLocation), parent)
{ }
SetupState::SetupState(const QString &directory, QObject *parent) : QObject(parent), m_directory(directory)
{
    reload();
}
void SetupState::setError(const QString &message)
{
    if (m_error == message)
        return;
    m_error = message;
    Q_EMIT errorChanged();
}
bool SetupState::safeDirectory() const
{
    if (m_directory.isEmpty() || !QDir::isAbsolutePath(m_directory))
        return false;
    QString path = QDir::cleanPath(m_directory);
    while (true) {
        const QFileInfo info(path);
        if (info.isSymLink() || info.isJunction() || (info.exists() && !info.isDir()))
            return false;
        const QString parent = QFileInfo(path).dir().absolutePath();
        if (parent == path)
            break;
        path = parent;
    }
    return true;
}
void SetupState::reload()
{
    setError({});
    if (!safeDirectory()) {
        setError(tr("The setup record folder is unavailable or linked. No files were changed."));
        return;
    }
    const QFileInfo info(QDir(m_directory).filePath(u"completed.json"_s));
    if (info.isSymLink() || info.isJunction() || (info.exists() && (!info.isFile() || info.size() != record.size()))) {
        setError(tr("An existing setup record is not recognised. It has been preserved."));
        return;
    }
    if (!info.exists())
        return;
    QFile file(info.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly) || file.read(record.size() + 1) != record) {
        setError(tr("An existing setup record cannot be read safely. It has been preserved."));
        return;
    }
    if (!m_completed) {
        m_completed = true;
        Q_EMIT completedChanged();
    }
}
bool SetupState::finish()
{
    if (m_completed)
        return true;
    // Recheck a file that may have appeared while the welcome window was open.
    reload();
    if (!m_error.isEmpty())
        return false;
    if (m_completed)
        return true;
    if (!QDir().mkpath(m_directory) || !safeDirectory()) {
        setError(tr("Setup could not create its record folder. You can close this window and try next time."));
        return false;
    }
    if (!QFile::setPermissions(m_directory, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner)) {
        setError(tr("Setup could not protect its record folder. No completion was recorded."));
        return false;
    }
    // Publish without replacing a record created by another welcome instance.
    // QTemporaryFile owns the handle through publication, including on Windows.
    // Its atomic rename refuses an existing destination without copy fallback.
    QTemporaryFile pending(QDir(m_directory).filePath(u".setup-XXXXXX"_s));
    if (!pending.open()) {
        setError(tr("Setup could not prepare its completion record. It will appear again next time."));
        return false;
    }
    if (pending.write(record) != record.size() || !pending.flush()) {
        setError(tr("Setup could not save its completion record. It will appear again next time."));
        return false;
    }
    if (!pending.rename(QDir(m_directory).filePath(u"completed.json"_s))) {
        reload();
        if (m_completed)
            return true;
        if (m_error.isEmpty())
            setError(tr("Setup could not publish its completion record. It will appear again next time."));
        return false;
    }
    pending.setAutoRemove(false);
    m_completed = true;
    Q_EMIT completedChanged();
    return true;
}
} // namespace qor
