// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#pragma once

#include <QtCore/QElapsedTimer>
#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QPointF>
#include <QtCore/QPointer>
#include <QtGui/QRegion>
#include <QtQml/QQmlParserStatus>
#include <QtQml/qqmlregistration.h>
#include <QtWaylandCompositor/QWaylandCompositorExtension>
#include <QtWaylandCompositor/QWaylandSurface>
#include <QtWaylandCompositor/private/qwaylandcompositorquickextensions_p.h>

#include "qwayland-server-pointer-constraints-unstable-v1.h"
#include "qwayland-server-relative-pointer-unstable-v1.h"

#include <memory>
#include <optional>

class QWaylandCompositor;
class QWaylandKeyboard;
class QWaylandPointer;
class QWaylandSeat;
class QWaylandView;
class QQuickWindow;
class QWindow;

// Pointer lock, confinement and relative motion: what lets a first-person game
// capture the mouse (QOR-0020, decision 2), and what gamescope will not start
// without. Everything a client sends here is hostile (QOR-0018).
//
// Who may capture the pointer:
//   - only the active window, with no shell surface open, and only while it has the
//     keyboard and the pointer is on the very surface that asks (`mayHold`,
//     `mayStart`), and Prism has the host's focus;
//   - only after a person's action on it. That is a pointer button pressed on the
//     surface or a key other than a modifier reaching it, in the last five seconds,
//     since it last lost the pointer. Those are what Qt Wayland Compositor delivers
//     to a client from the output windows' input; no client has a way to send input.
//     A held key's repeats, the wheel, and a key pressed with Super held are not
//     actions.
//   - A persistent capture that ends comes back only after a new press of a pointer
//     button on it, never after a key, as mutter does: the person who just let go
//     with Super+Esc may press Esc again, or anything else, and stay out.
//
// What ends a capture: another surface taking the keyboard or the pointer, the
// lock screen, any shell surface opening, Prism losing the host's focus, and a
// shortcut no client can intercept (Desktop's Super+Esc, whatever else is held:
// Qt's shortcut map takes it before any item, as it does all of Prism's own).
//
// Who hears the pointer at all, captured or not: no client while a shell surface is
// open or the screen is locked (`setWithheld`), and no client while the pointer is
// over anything of the shell's lying over its window, a notification or a toast say
// (`coveredByShell`). A button pressed on a client keeps it its own until let go, as
// on any Wayland compositor.
//
// While captured the output windows' pointer events go no further: nothing of the
// shell is hovered or clicked by a pointer the person cannot see, buttons and the
// wheel go straight to the client, and QWaylandPointer sends no motion, enter or
// leave, whatever Qt's scene does underneath.
//
// Idle when unused: with no object from either protocol alive, the output windows
// have no filter here, and no timer exists either way.
namespace qor::prism {

class PointerConstraint;
class RelativePointer;

// One physical motion, in surface units (zwp_relative_pointer_v1.relative_motion).
struct PointerMotion
{
    quint64 microseconds = 0;
    QPointF delta;
    QPointF unaccelerated;
};

// Where relative motion comes from, and what can be done to the real pointer.
//
// Prism's KMS backend (Phase 3) fills this from libinput: unclipped deltas, the
// accelerated and unaccelerated ones apart, and a pointer it really holds still.
class PointerMotionSource : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    // Something has begun listening: what came before says nothing of what comes next.
    virtual void restart() = 0;
    // Every event an output window gets while anything listens. A source that
    // reads a device has no use for them.
    virtual void windowEvent(QWindow *window, QEvent *event) = 0;
    // Moves the real pointer to where a client said it would look right when a lock
    // ended (zwp_locked_pointer_v1.set_cursor_position_hint), if the source can.
    virtual void warp(QWindow *window, const QPointF &scenePosition) = 0;

Q_SIGNALS:
    void moved(const qor::prism::PointerMotion &motion);
};

// The decisions, in one place: who holds the pointer, who hears it.
class PointerCapture : public QObject
{
    Q_OBJECT

public:
    explicit PointerCapture(QWaylandCompositor *compositor);
    ~PointerCapture() override;

    void attach(QWaylandSeat *seat);

    // The one surface that may capture the pointer or hear its motion, as the shell
    // sees it: the active window's, with nothing over it. Null otherwise.
    QWaylandSurface *mayCapture() const { return m_mayCapture; }
    void setMayCapture(QWaylandSurface *surface);
    // The surface whose lock or confinement is active, or null.
    QWaylandSurface *capturedBy() const;
    // The way out: ends the capture, until the next action.
    void release();
    // Locked: no client hears the pointer at all, though its window is still fading out.
    bool isWithheld() const { return m_withheld; }
    void setWithheld(bool withheld);

    // ── The input path (CapturingPointer, WatchedKeyboard) ──────────────────
    // `pressed`: a pointer button, not a key.
    void personActed(QWaylandSurface *surface, bool pressed);
    // What wl_pointer may say of a motion at `scenePosition` on `view`: where, or
    // nothing at all.
    std::optional<QPointF> allowedPosition(QWaylandView *view, const QPointF &position,
                                           const QPointF &scenePosition) const;
    void pointerMoved();

    // ── The protocol objects ────────────────────────────────────────────────
    bool isConstrained(QWaylandSurface *surface) const;
    // Whether a client may have another object of these protocols.
    bool hasRoomFor(wl_client *client) const;
    void add(PointerConstraint *constraint);
    void forget(PointerConstraint *constraint);
    void add(RelativePointer *pointer);
    void forget(RelativePointer *pointer);
    // A constraint's surface or wl_pointer went: it can never hold the pointer again.
    void orphan(PointerConstraint *constraint);
    // The state may have changed: ends what may not go on, and starts what may.
    void update();

Q_SIGNALS:
    void capturedByChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    bool mayHold(const PointerConstraint *constraint) const;
    bool mayStart(const PointerConstraint *constraint) const;
    bool pointerStaysOnSurface(const QWindow *window, const QPointF &scenePosition) const;
    void start(PointerConstraint *constraint);
    void end(PointerConstraint *constraint);
    void stopHolding();
    void holdSceneHover(bool hold);
    void holdSceneHoverOn(QQuickWindow *window, bool hold);
    void forgetAction();
    // Whatever was left of a person's last action: Prism lost the host's focus.
    void hostFocusChanged(Qt::ApplicationState state);
    void updateListening();
    void listenTo(QWindow *window, bool listen);
    void deliver(const PointerMotion &motion);
    bool filterWhileHeld(QWindow *window, QEvent *event);

    QWaylandCompositor *m_compositor;
    QPointer<QWaylandSeat> m_seat;
    QPointer<QWaylandSurface> m_mayCapture;
    std::unique_ptr<PointerMotionSource> m_source;
    QList<PointerConstraint *> m_constraints;
    QList<RelativePointer *> m_relative;
    PointerConstraint *m_active = nullptr;
    bool m_listening = false;
    bool m_pointerOnSurface = false;
    bool m_withheld = false;
    // Each output window's frame-synchronous hover interval, while held.
    QHash<QQuickWindow *, int> m_sceneHoverIntervals;
    // Output windows the host's pointer left while held: Qt's scene never heard it go.
    QList<QPointer<QWindow>> m_leftWhileHeld;

    // The last action of a person's, on what, and whether it was a pointer button.
    QPointer<QWaylandSurface> m_actedOn;
    QElapsedTimer m_actedAt;
    bool m_actedByPress = false;
    // A button pressed before the capture began still has Qt's grab, so its release
    // goes through Qt; one pressed during it went straight to the client, and is
    // released to the client if the capture ends first.
    Qt::MouseButtons m_pressedBefore;
    Qt::MouseButtons m_pressedDuring;
};

// WaylandCompositor with the seat's pointer and keyboard Prism needs to see what
// a person does, and to keep a captured pointer's motion from a client.
class PrismCompositor : public QWaylandQuickCompositorQuickExtensionContainer
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QWaylandSurface *mayCapturePointer READ mayCapturePointer WRITE setMayCapturePointer NOTIFY
                   mayCapturePointerChanged FINAL)
    Q_PROPERTY(QWaylandSurface *pointerCapturedBy READ pointerCapturedBy NOTIFY pointerCapturedByChanged FINAL)
    Q_PROPERTY(bool pointerWithheld READ pointerWithheld WRITE setPointerWithheld NOTIFY pointerWithheldChanged FINAL)

public:
    PrismCompositor();

    QWaylandSurface *mayCapturePointer() const;
    void setMayCapturePointer(QWaylandSurface *surface);
    QWaylandSurface *pointerCapturedBy() const;
    bool pointerWithheld() const;
    void setPointerWithheld(bool withheld);
    Q_INVOKABLE void releasePointer();

    PointerCapture *pointerCapture() const { return m_capture; }

Q_SIGNALS:
    void mayCapturePointerChanged();
    void pointerCapturedByChanged();
    void pointerWithheldChanged();

protected:
    QWaylandPointer *createPointerDevice(QWaylandSeat *seat) override;
    QWaylandKeyboard *createKeyboardDevice(QWaylandSeat *seat) override;

private:
    PointerCapture *m_capture;
};

// zwp_relative_pointer_manager_v1: motion a client hears apart from the pointer's
// position. Heard only by the client whose window has the keyboard, with the pointer
// on that window, and only for a motion that stays there.
class RelativePointerManager : public QWaylandCompositorExtension,
                               public QtWaylandServer::zwp_relative_pointer_manager_v1,
                               public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    QML_NAMED_ELEMENT(RelativePointerManagerV1)

public:
    void initialize() override;
    void classBegin() override { }
    void componentComplete() override;

    static const wl_interface *interface() { return zwp_relative_pointer_manager_v1::interface(); }
    const wl_interface *extensionInterface() const override { return interface(); }

protected:
    void zwp_relative_pointer_manager_v1_destroy(Resource *resource) override;
    void zwp_relative_pointer_manager_v1_get_relative_pointer(Resource *resource, uint32_t id,
                                                              wl_resource *pointer) override;
};

// zwp_pointer_constraints_v1: a client asks to hold the pointer still, or within a
// region of its surface. PointerCapture decides whether it ever does.
class PointerConstraintsManager : public QWaylandCompositorExtension,
                                  public QtWaylandServer::zwp_pointer_constraints_v1,
                                  public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    QML_NAMED_ELEMENT(PointerConstraintsV1)

public:
    void initialize() override;
    void classBegin() override { }
    void componentComplete() override;

    static const wl_interface *interface() { return zwp_pointer_constraints_v1::interface(); }
    const wl_interface *extensionInterface() const override { return interface(); }

protected:
    void zwp_pointer_constraints_v1_destroy(Resource *resource) override;
    void zwp_pointer_constraints_v1_lock_pointer(Resource *resource, uint32_t id, wl_resource *surface,
                                                 wl_resource *pointer, wl_resource *region, uint32_t asked) override;
    void zwp_pointer_constraints_v1_confine_pointer(Resource *resource, uint32_t id, wl_resource *surface,
                                                    wl_resource *pointer, wl_resource *region, uint32_t asked) override;

private:
    PointerCapture *acceptable(Resource *resource, wl_resource *surface, uint32_t asked);
};

} // namespace qor::prism
