// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#pragma once

#include <QtCore/QObject>
#include <QtQml/qqmlregistration.h>
#include <QtQuick/QQuickItem>
#include <QtWaylandCompositor/QWaylandCompositor>

namespace qor::prism {

// QML singleton `WindowManagement`: makes Prism the only window manager for
// a client surface item.
//
// Qt's xdg-shell integration for a ShellSurfaceItem reacts to the client's
// move, resize, maximise and fullscreen requests itself (it moves the item,
// sends its own resize and maximise configures). Prism handles all of these
// with its own springs, snapping, work area and size limits, and two
// handlers fighting over one window give inconsistent sizes. takeOver()
// disconnects Qt's handlers for those requests; popups and activation stay
// with Qt.
class WindowManagement : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // How many windows one client may keep open.
    Q_PROPERTY(int mostWindowsPerClient READ mostWindowsPerClient CONSTANT)

public:
    explicit WindowManagement(QObject *parent = nullptr);

    int mostWindowsPerClient() const { return 32; }

    Q_INVOKABLE void takeOver(QQuickItem *shellSurfaceItem);
    // No client holds the keyboard: it is sent wl_keyboard.leave, and can mint no
    // activation tokens on the strength of having had it.
    Q_INVOKABLE void releaseKeyboard(QWaylandCompositor *compositor);
    // Whether the client has acknowledged every configure sent to it, so
    // that what it commits answers the latest one. During a resize a client
    // still draws at sizes configured earlier.
    Q_INVOKABLE bool configureAcked(QObject *toplevel) const;
};

} // namespace qor::prism
