// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "controlleractions.h"
#include "controllerinput_windows.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QHash>
#include <QtCore/QPointer>
#include <QtGui/QGuiApplication>
#include <QtQuick/QQuickWindow>

#include <cstddef>
#include <utility>
#include <vector>

namespace qor::qonsole {

struct WindowsControllerInput::State
{
    QPointer<QQuickWindow> window;
    QPointer<QObject> launcher;
    HWND hwnd = nullptr;
    bool registered = false;
    QHash<HANDLE, quint32> buttons;
    ControllerActions actions;

    bool active() const
    {
        return window && window->isActive() && window->isVisible()
               && QGuiApplication::applicationState() == Qt::ApplicationActive;
    }
};

WindowsControllerInput::WindowsControllerInput(QQuickWindow *window, QObject *launcher)
    : QObject(window), m_state(std::make_unique<State>())
{
    m_state->window = window;
    m_state->launcher = launcher;
    m_state->hwnd = reinterpret_cast<HWND>(window->winId());
    // No INPUTSINK or NOLEGACY: foreground only; keyboard delivery is unchanged.
    RAWINPUTDEVICE device { 0x01, 0x05, RIDEV_DEVNOTIFY, m_state->hwnd };
    if (!RegisterRawInputDevices(&device, 1, sizeof(device))) {
        qWarning("Qonsole could not register foreground controller input");
        return;
    }
    m_state->registered = true;
    const auto reset = [this] {
        m_state->buttons.clear();
        m_state->actions.reset();
    };
    connect(window, &QWindow::activeChanged, this, reset);
    connect(window, &QWindow::visibleChanged, this, reset);
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, reset);
    QCoreApplication::instance()->installNativeEventFilter(this);
}

WindowsControllerInput::~WindowsControllerInput()
{
    if (QCoreApplication::instance())
        QCoreApplication::instance()->removeNativeEventFilter(this);
    if (m_state->registered) {
        RAWINPUTDEVICE device { 0x01, 0x05, RIDEV_REMOVE, nullptr };
        RegisterRawInputDevices(&device, 1, sizeof(device));
    }
}

bool WindowsControllerInput::nativeEventFilter(const QByteArray &type, void *message, qintptr *result)
{
    Q_UNUSED(type);
    Q_UNUSED(result);
    const auto *msg = static_cast<const MSG *>(message);
    if (!msg || msg->hwnd != m_state->hwnd)
        return false;
    if (msg->message == WM_INPUT_DEVICE_CHANGE && msg->wParam == GIDC_REMOVAL) {
        if (m_state->buttons.remove(reinterpret_cast<HANDLE>(msg->lParam)))
            m_state->actions.reset();
        return false;
    }
    if (msg->message != WM_INPUT)
        return false;
    if (!m_state->active() || GET_RAWINPUT_CODE_WPARAM(msg->wParam) != RIM_INPUT) {
        m_state->buttons.clear();
        m_state->actions.reset();
        return false;
    }

    const auto rawHandle = reinterpret_cast<HRAWINPUT>(msg->lParam);
    UINT size = 0;
    constexpr auto offset = offsetof(RAWINPUT, data.hid.bRawData);
    if (GetRawInputData(rawHandle, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER)) != 0 || size < offset
        || size > 65536)
        return false;
    // quint64 storage supplies RAWINPUT's native alignment.
    std::vector<quint64> storage((size + 7) / 8);
    const UINT read = GetRawInputData(rawHandle, RID_INPUT, storage.data(), &size, sizeof(RAWINPUTHEADER));
    if (read == UINT(-1) || read < offset)
        return false;
    const auto *raw = reinterpret_cast<const RAWINPUT *>(storage.data());
    if (raw->header.dwType != RIM_TYPEHID || raw->header.dwSize < offset || raw->header.dwSize > read)
        return false;
    const HANDLE id = raw->header.hDevice;
    if (!m_state->buttons.contains(id)) {
        RID_DEVICE_INFO info {};
        info.cbSize = sizeof(info);
        UINT infoSize = sizeof(info);
        if (GetRawInputDeviceInfoW(id, RIDI_DEVICEINFO, &info, &infoSize) == UINT(-1) || info.dwType != RIM_TYPEHID
            || info.hid.usUsagePage != 0x01 || info.hid.usUsage != 0x05 || info.hid.dwVendorId != 0x054c
            || (info.hid.dwProductId != 0x05c4 && info.hid.dwProductId != 0x09cc) || m_state->buttons.size() >= 16)
            return false;
        m_state->buttons.insert(id, 0);
    }
    const QByteArrayView reports(reinterpret_cast<const char *>(raw) + offset, qsizetype(raw->header.dwSize - offset));
    visitWiredDs4Reports(
        reports, raw->data.hid.dwSizeHid, raw->data.hid.dwCount, [this, id](const input::GamepadState &state) {
            m_state->buttons[id] = state.buttons;
            quint32 merged = 0;
            for (quint32 buttons : std::as_const(m_state->buttons))
                merged |= buttons;
            const auto action = m_state->actions.update(merged, m_state->active());
            if (action != ControllerAction::None && !dispatchControllerAction(m_state->launcher, action))
                qWarning("Qonsole controller action could not reach its launcher");
        });
    // Let Qt/DefWindowProc perform foreground Raw Input message cleanup.
    return false;
}

} // namespace qor::qonsole
