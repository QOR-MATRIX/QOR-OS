// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#include "windowmanagement.h"

#include <QtWaylandCompositor/QWaylandQuickShellIntegration>
#include <QtWaylandCompositor/QWaylandQuickShellSurfaceItem>
#include <QtWaylandCompositor/QWaylandSeat>
#include <QtWaylandCompositor/QWaylandXdgShell>
#include <QtWaylandCompositor/private/qwaylandxdgshell_p.h>

namespace qor::prism {

WindowManagement::WindowManagement(QObject *parent) : QObject(parent) { }

void WindowManagement::takeOver(QQuickItem *shellSurfaceItem)
{
    auto *item = qobject_cast<QWaylandQuickShellSurfaceItem *>(shellSurfaceItem);
    auto *xdgSurface = item ? qobject_cast<QWaylandXdgSurface *>(item->shellSurface()) : nullptr;
    QWaylandXdgToplevel *toplevel = xdgSurface ? xdgSurface->toplevel() : nullptr;
    if (!toplevel)
        return;
    // The integration is created with the item as its parent.
    const auto integrations = item->findChildren<QWaylandQuickShellIntegration *>(Qt::FindDirectChildrenOnly);
    for (QWaylandQuickShellIntegration *integration : integrations) {
        QObject::disconnect(toplevel, &QWaylandXdgToplevel::startMove, integration, nullptr);
        QObject::disconnect(toplevel, &QWaylandXdgToplevel::startResize, integration, nullptr);
        QObject::disconnect(toplevel, &QWaylandXdgToplevel::setMaximized, integration, nullptr);
        QObject::disconnect(toplevel, &QWaylandXdgToplevel::unsetMaximized, integration, nullptr);
        QObject::disconnect(toplevel, &QWaylandXdgToplevel::maximizedChanged, integration, nullptr);
        QObject::disconnect(toplevel, &QWaylandXdgToplevel::setFullscreen, integration, nullptr);
        QObject::disconnect(toplevel, &QWaylandXdgToplevel::unsetFullscreen, integration, nullptr);
        QObject::disconnect(toplevel, &QWaylandXdgToplevel::fullscreenChanged, integration, nullptr);
    }
}

void WindowManagement::releaseKeyboard(QWaylandCompositor *compositor)
{
    if (compositor && compositor->defaultSeat())
        compositor->defaultSeat()->setKeyboardFocus(nullptr);
}

bool WindowManagement::configureAcked(QObject *object) const
{
    // Validate the protocol object at the QML boundary before using Qt's private state.
    auto *toplevel = qobject_cast<QWaylandXdgToplevel *>(object);
    return toplevel && QWaylandXdgToplevelPrivate::get(toplevel)->m_pendingConfigures.isEmpty();
}

} // namespace qor::prism
