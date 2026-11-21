// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#pragma once

#include "playstation.h"

#include <QtCore/QByteArrayView>
#include <QtCore/QObject>

namespace qor::qonsole {

enum class ControllerAction { None, Back, Confirm, Up, Down, Left, Right };

// A new foreground interval must first observe neutral navigation controls.
// Held/repeated reports and releases never activate a second page action.
class ControllerActions
{
public:
    void reset()
    {
        m_previous = 0;
        m_armed = false;
    }

    ControllerAction update(quint32 buttons, bool active)
    {
        using input::Button;
        const quint32 mapped = input::bit(Button::South) | input::bit(Button::East) | input::bit(Button::DPadUp)
                               | input::bit(Button::DPadDown) | input::bit(Button::DPadLeft)
                               | input::bit(Button::DPadRight);
        buttons &= mapped;
        const quint32 pressed = buttons & ~m_previous;
        m_previous = buttons;
        if (!active) {
            m_armed = false;
            return ControllerAction::None;
        }
        if (!m_armed) {
            m_armed = buttons == 0;
            return ControllerAction::None;
        }
        // At most one action per report. Cancel wins over simultaneous confirm.
        if (pressed & input::bit(Button::East))
            return ControllerAction::Back;
        if (pressed & input::bit(Button::South))
            return ControllerAction::Confirm;
        if (pressed & input::bit(Button::DPadUp))
            return ControllerAction::Up;
        if (pressed & input::bit(Button::DPadDown))
            return ControllerAction::Down;
        if (pressed & input::bit(Button::DPadLeft))
            return ControllerAction::Left;
        if (pressed & input::bit(Button::DPadRight))
            return ControllerAction::Right;
        return ControllerAction::None;
    }

private:
    quint32 m_previous = 0;
    bool m_armed = false;
};

inline QString controllerActionName(ControllerAction action)
{
    switch (action) {
    case ControllerAction::Back: return QStringLiteral("back");
    case ControllerAction::Confirm: return QStringLiteral("confirm");
    case ControllerAction::Up: return QStringLiteral("up");
    case ControllerAction::Down: return QStringLiteral("down");
    case ControllerAction::Left: return QStringLiteral("left");
    case ControllerAction::Right: return QStringLiteral("right");
    case ControllerAction::None: return {};
    }
    return {};
}

inline bool dispatchControllerAction(QObject *launcher, ControllerAction action)
{
    if (!launcher || action == ControllerAction::None)
        return false;
    return QMetaObject::invokeMethod(launcher, "dispatch", Qt::DirectConnection,
                                     Q_ARG(QString, controllerActionName(action)));
}

// Raw Input can batch reports. Bound the entire block before any callback, and
// accept only the existing parser's full DS4 USB-shaped 0x01/64-byte layout.
template<typename Visitor>
bool visitWiredDs4Reports(QByteArrayView data, quint32 reportSize, quint32 reportCount, Visitor visit)
{
    if (reportSize != 64 || reportCount == 0 || reportCount > 64 || qsizetype(reportCount) > data.size() / 64)
        return false;
    for (quint32 i = 0; i < reportCount; ++i) {
        if (quint8(data[qsizetype(i) * 64]) != 0x01)
            return false;
    }
    for (quint32 i = 0; i < reportCount; ++i) {
        const auto *bytes = reinterpret_cast<const quint8 *>(data.data() + qsizetype(i) * 64);
        const auto state = input::playstation::parseInput(input::playstation::Model::DualShock4, bytes, 64);
        if (!state)
            return false;
        visit(*state);
    }
    return true;
}

} // namespace qor::qonsole
