// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#pragma once

#include <QtCore/QAbstractNativeEventFilter>
#include <QtCore/QObject>
#include <memory>

class QQuickWindow;

namespace qor::qonsole {

// Reads foreground gamepad reports; never opens a device or sends output.
class WindowsControllerInput final : public QObject, public QAbstractNativeEventFilter
{
public:
    WindowsControllerInput(QQuickWindow *window, QObject *launcher);
    ~WindowsControllerInput() override;
    bool nativeEventFilter(const QByteArray &type, void *message, qintptr *result) override;

private:
    struct State;
    std::unique_ptr<State> m_state;
};

} // namespace qor::qonsole
