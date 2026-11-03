// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
#include "pointercapture.h"

#include <QtCore/QLoggingCategory>
#include <QtCore/QSet>
#include <QtCore/QVarLengthArray>
#include <QtGui/QGuiApplication>
#include <QtGui/QMouseEvent>
#include <QtGui/QWheelEvent>
#include <QtGui/QWindow>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <QtQuick/private/qquickdeliveryagent_p_p.h>
#include <QtQuick/private/qquickitem_p.h>
#include <QtQuick/private/qquickwindow_p.h>
#include <QtWaylandCompositor/QWaylandClient>
#include <QtWaylandCompositor/QWaylandDestroyListener>
#include <QtWaylandCompositor/QWaylandKeyboard>
#include <QtWaylandCompositor/QWaylandOutput>
#include <QtWaylandCompositor/QWaylandPointer>
#include <QtWaylandCompositor/QWaylandQuickItem>
#include <QtWaylandCompositor/QWaylandSeat>
#include <QtWaylandCompositor/QWaylandView>
#include <QtWaylandCompositor/private/qwaylandsurface_p.h>
#include <QtWaylandCompositor/private/qwlregion_p.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

// Its debug lines come on every motion over the shell: off unless asked for.
Q_LOGGING_CATEGORY(lcPointerCapture, "qor.prism.pointercapture", QtInfoMsg)

namespace qor::prism {

namespace {

// How long a person's action stands for a lock asked for after it. A game asks just
// after the click or key that means "play"; much later, it waits for the next one.
constexpr qint64 kActionFreshMs = 5000;
// What one client may keep open: more than any game needs. Each is memory here and
// work on every motion, so a client that goes past it is ended.
constexpr qsizetype kMostObjectsPerClient = 32;
// A region is walked on every motion of a confined pointer. One this broken up has
// no honest use, and counts as empty.
constexpr int kMostRegionRects = 1024;
// How many of a window's subsurfaces are looked through, all told: a client may make
// as many as it likes, and this is walked on every motion.
constexpr qsizetype kMostSubsurfaces = 256;
// How many of the shell's items are looked through to see what lies over a window.
// Far more than the shell ever shows; past it, the window counts as covered.
constexpr int kMostItemsLookedAt = 4096;
// wl_fixed_t's smallest step, the furthest short of a rectangle's edge a position gets.
constexpr double kFixedStep = 1.0 / 256.0;

// Whether `surface` is the window `root` or one of its subsurfaces.
//
// Walked from the root because Qt keeps a surface's children by QPointer, while a
// subsurface's link to its parent dangles once a client destroys the parent first.
// Bounded, because the tree is the client's to make, cycles and fan-outs of thousands
// included: no more than kMostSubsurfaces children are looked at in all, and a surface
// past them is not counted as the window's.
bool inWindow(QWaylandSurface *root, QWaylandSurface *surface)
{
    if (!root || !surface)
        return false;
    if (root == surface)
        return true;
    QVarLengthArray<QWaylandSurface *, 16> pending { root };
    QSet<const QWaylandSurface *> seen { root };
    qsizetype lookedAt = 0;
    for (qsizetype next = 0; next < pending.size(); ++next) {
        const auto &children = QWaylandSurfacePrivate::get(pending[next])->subsurfaceChildren;
        for (const QPointer<QWaylandSurface> &child : children) {
            if (++lookedAt > kMostSubsurfaces)
                return false;
            if (child == surface)
                return true;
            if (child && !seen.contains(child.data())) {
                seen.insert(child.data());
                pending.append(child.data());
            }
        }
    }
    return false;
}

// Whether an item is one of the shell's windows: AppWindow, the only item with both of
// these (Qt's ShellSurfaceItem has a shellSurface too).
bool isShellWindow(const QQuickItem *item)
{
    const QMetaObject *type = item->metaObject();
    return type->indexOfProperty("shellSurface") >= 0 && type->indexOfProperty("isClient") >= 0;
}

// The shell's window an item of a client's is drawn in, or the item itself.
const QQuickItem *windowItemOf(const QQuickItem *item)
{
    for (const QQuickItem *at = item; at; at = at->parentItem())
        if (isShellWindow(at))
            return at;
    return item;
}

enum class Hit { Nothing, Window, Covered };

// What a person sees at `scene` within `item`, from the top down: `window`, something of
// the shell's over it, or nothing. Something is an item that draws or takes hover,
// visible, enabled and not see-through, whose shape holds the point. Another of the
// shell's windows is all of its rectangle, and nothing beyond it. Items that only hold
// others are looked through, as are disabled ones: the screen's own filters (night light,
// brightness) are, and they lie over everything.
Hit hitAt(const QQuickItem *item, const QPointF &scene, const QQuickItem *window, int *budget, const QQuickItem **by)
{
    if (item == window)
        return Hit::Window;
    if (--*budget < 0)
        return Hit::Covered;
    const QQuickItemPrivate *d = QQuickItemPrivate::get(item);
    const bool onTheWay = item->isAncestorOf(window);
    // Hidden by an effect that draws it elsewhere (a ShaderEffectSource's hideSource).
    const bool hiddenByEffect = d->extra.isAllocated() && d->extra->hideRefCount > 0;
    if (!item->isVisible() || d->culled || hiddenByEffect || item->opacity() <= 0.0
        || (!onTheWay && !item->isEnabled()))
        return Hit::Nothing;
    const bool inside = item->contains(item->mapFromScene(scene));
    if (!onTheWay && isShellWindow(item)) {
        *by = item;
        return inside ? Hit::Covered : Hit::Nothing;
    }
    if (item->clip() && !inside)
        return Hit::Nothing;
    // An item that only takes presses and draws nothing (a list with nothing in it, a
    // click-catcher) is not seen, and Qt's own delivery decides what it takes.
    const bool takes =
        inside && !onTheWay && (item->flags().testFlag(QQuickItem::ItemHasContents) || item->acceptHoverEvents());
    // Children of z at or above 0 are drawn over the item, the others under it.
    const QList<QQuickItem *> children = d->paintOrderChildItems();
    bool itself = false;
    for (qsizetype i = children.size() - 1; i >= 0; --i) {
        const QQuickItem *child = children.at(i);
        if (!itself && child->z() < 0) {
            itself = true;
            if (takes) {
                *by = item;
                return Hit::Covered;
            }
        }
        const Hit hit = hitAt(child, scene, window, budget, by);
        if (hit != Hit::Nothing)
            return hit;
    }
    if (!itself && takes) {
        *by = item;
        return Hit::Covered;
    }
    return Hit::Nothing;
}

// Whether the person sees anything of the shell's, rather than the client's window, at
// `scene` on the client's `item`: a notification, a toast, the volume, a controller's
// prompt or the Rail lying over it. Inside the window Qt's own hover decides.
bool coveredByShell(const QQuickItem *item, const QPointF &scene)
{
    if (!item || !item->window())
        return true;
    int budget = kMostItemsLookedAt;
    const QQuickItem *by = nullptr;
    const Hit hit = hitAt(item->window()->contentItem(), scene, windowItemOf(item), &budget, &by);
    if (hit != Hit::Window)
        qCDebug(lcPointerCapture) << "covered at" << scene << "by" << by << (by ? by->parentItem() : nullptr);
    return hit != Hit::Window;
}

QPoint pixelAt(const QPointF &position) { return QPoint(int(std::floor(position.x())), int(std::floor(position.y()))); }

// The nearest position to `point` inside a non-empty region. Reads rectangles by
// their corners and in doubles: a client can ask for rectangles at the limits of an
// int, where QRect::width() itself overflows.
QPointF nearestIn(const QRegion &region, const QPointF &point)
{
    QPointF best = point;
    double bestDistance = std::numeric_limits<double>::infinity();
    for (const QRect &rect : region) {
        const QPointF inside(std::clamp(point.x(), double(rect.left()), double(rect.right()) + 1.0 - kFixedStep),
                             std::clamp(point.y(), double(rect.top()), double(rect.bottom()) + 1.0 - kFixedStep));
        const double dx = inside.x() - point.x();
        const double dy = inside.y() - point.y();
        const double distance = dx * dx + dy * dy;
        if (distance < bestDistance) {
            bestDistance = distance;
            best = inside;
        }
    }
    return best;
}

// A wl_region's value now: the protocol copies it, so the object may go at once.
// Null means the whole of the surface's input region.
std::optional<QRegion> regionOf(wl_resource *resource)
{
    if (!resource)
        return std::nullopt;
    const QRegion region = QtWayland::Region::fromResource(resource)->region();
    return region.rectCount() > kMostRegionRects ? QRegion() : region;
}

PointerCapture *captureOf(QWaylandCompositorExtension *extension)
{
    auto *compositor = qobject_cast<PrismCompositor *>(extension->extensionContainer());
    return compositor ? compositor->pointerCapture() : nullptr;
}

// Nested, all Prism has is the output windows' pointer events, so motion is the
// difference between one position and the next. That is less than the protocol has
// in mind, and it says so here rather than pretending:
//   - The host's pointer is not held still. It goes on moving under a lock, and at the
//     edge of its screen or of Prism's window the motion stops.
//   - Accelerated and unaccelerated are the same numbers: the host's acceleration
//     has been applied, and what it was before is not knowable from here.
//   - Time is the event's, in milliseconds.
//   - Warping would need the host's permission, which a Wayland host does not give.
class WindowMotionSource final : public PointerMotionSource
{
public:
    using PointerMotionSource::PointerMotionSource;

    void restart() override { m_window = nullptr; }

    void windowEvent(QWindow *window, QEvent *event) override
    {
        switch (event->type()) {
        case QEvent::Enter:
        case QEvent::Leave:
            // Where it went in between is not known: no jump on the way back.
            m_window = nullptr;
            break;
        case QEvent::MouseMove: {
            const auto *mouse = static_cast<QMouseEvent *>(event);
            const QPointF position = mouse->scenePosition();
            if (m_window == window && position != m_last) {
                const QPointF delta = position - m_last;
                Q_EMIT moved({ quint64(mouse->timestamp()) * 1000, delta, delta });
            }
            m_window = window;
            m_last = position;
            break;
        }
        default: break;
        }
    }

    void warp(QWindow *, const QPointF &) override { }

private:
    const QWindow *m_window = nullptr;
    QPointF m_last;
};

// The seat's pointer, with the one place every wl_pointer motion passes through.
class CapturingPointer final : public QWaylandPointer
{
public:
    CapturingPointer(QWaylandSeat *seat, PointerCapture *capture) : QWaylandPointer(seat), m_capture(capture) { }

    void sendMouseMoveEvent(QWaylandView *view, const QPointF &localPos, const QPointF &outputSpacePos) override
    {
        const std::optional<QPointF> allowed =
            m_capture ? m_capture->allowedPosition(view, localPos, outputSpacePos) : std::optional(localPos);
        if (!allowed) {
            // Over something of the shell's: as far as the client may know, the pointer
            // has left it. A held one stays where it is, and says nothing.
            if (m_capture && !m_capture->capturedBy() && view && mouseFocus() == view)
                seat()->setMouseFocus(nullptr);
            return;
        }
        QWaylandPointer::sendMouseMoveEvent(view, *allowed, outputSpacePos);
        if (m_capture)
            m_capture->pointerMoved();
    }

    // Qt calls this from the surface item a press was delivered to, once it holds the
    // pointer's focus. No client request reaches it.
    uint sendMousePressEvent(Qt::MouseButton button) override
    {
        const uint serial = QWaylandPointer::sendMousePressEvent(button);
        if (m_capture)
            m_capture->personActed(mouseFocus() ? mouseFocus()->surface() : nullptr, true);
        return serial;
    }

private:
    QPointer<PointerCapture> m_capture;
};

// The seat's keyboard, to see a key reach a surface. Qt has already dropped a held
// key's repeats by here (QWaylandSeat::sendFullKeyEvent), and Prism's own shortcuts
// never come here at all.
class WatchedKeyboard final : public QWaylandKeyboard
{
public:
    WatchedKeyboard(QWaylandSeat *seat, PointerCapture *capture) : QWaylandKeyboard(seat), m_capture(capture) { }

    void sendKeyPressEvent(uint code) override
    {
        QWaylandKeyboard::sendKeyPressEvent(code);
        // With Super held a key is on its way to the shell, or to a host's shortcut: the
        // way out pressed again, say, which must not put the person back in.
        if (m_capture && !isModifier(code) && !QGuiApplication::keyboardModifiers().testFlag(Qt::MetaModifier))
            m_capture->personActed(focus(), false);
    }

private:
    // A modifier alone is on its way to something else, a shortcut of the shell's most
    // likely, and a lock key or Compose changes what the next key means. Codes are
    // evdev's, plus 8.
    static bool isModifier(uint code)
    {
        switch (code) {
        case 29 + 8:  // left ctrl
        case 42 + 8:  // left shift
        case 54 + 8:  // right shift
        case 56 + 8:  // left alt
        case 58 + 8:  // caps lock
        case 69 + 8:  // num lock
        case 70 + 8:  // scroll lock
        case 97 + 8:  // right ctrl
        case 100 + 8: // right alt
        case 125 + 8: // left meta
        case 126 + 8: // right meta
        case 127 + 8: // compose
            return true;
        default: return false;
        }
    }

    QPointer<PointerCapture> m_capture;
};

} // namespace

// ── The protocol objects ─────────────────────────────────────────────────────

// One lock or confinement a client asked for. It outlives its surface and its
// wl_pointer if the client keeps it, inert: requests on it then do nothing.
class PointerConstraint
{
public:
    PointerConstraint(PointerCapture *capture, wl_client *client, QWaylandSurface *surface, wl_resource *pointer,
                      wl_resource *region, bool oneshot)
        : m_capture(capture), m_client(client), m_surface(surface), m_region(regionOf(region)), m_oneshot(oneshot)
    {
        m_pointerGone.listenForDestruction(pointer);
        QObject::connect(&m_pointerGone, &QWaylandDestroyListener::fired, capture,
                         [capture, this] { capture->orphan(this); });
        m_surfaceGone = QObject::connect(surface, &QWaylandSurface::surfaceDestroyed, capture,
                                         [capture, this] { capture->orphan(this); });
        // Double-buffered state applies on the surface's commit, and a commit may have
        // changed the input region as well.
        m_committed = QObject::connect(surface, &QWaylandSurface::redraw, capture, [capture, this] {
            applyPending();
            capture->update();
        });
        updateEffectiveRegion();
    }

    virtual ~PointerConstraint()
    {
        m_pointerGone.reset();
        QObject::disconnect(m_surfaceGone);
        QObject::disconnect(m_committed);
    }

    PointerConstraint(const PointerConstraint &) = delete;
    PointerConstraint &operator=(const PointerConstraint &) = delete;

    virtual bool isLock() const = 0;
    virtual void sendStarted() = 0;
    virtual void sendEnded() = 0;

    wl_client *client() const { return m_client; }
    // The surface, while this can still hold the pointer.
    QWaylandSurface *surface() const { return m_inert ? nullptr : m_surface.data(); }
    bool hasRegion() const { return m_region.has_value(); }
    // Its region within the surface's input region: where the pointer may be.
    const QRegion &effectiveRegion() const { return m_effective; }
    const std::optional<QPointF> &cursorHint() const { return m_hint; }

    void ended()
    {
        sendEnded();
        m_hasEnded = true;
        if (m_oneshot)
            makeInert();
    }

    // It has held the pointer before, and let go.
    bool hasEnded() const { return m_hasEnded; }

    void makeInert()
    {
        m_inert = true;
        m_pointerGone.reset();
        QObject::disconnect(m_surfaceGone);
        QObject::disconnect(m_committed);
    }

protected:
    void setPendingRegion(wl_resource *region)
    {
        if (m_inert)
            return;
        m_pendingRegion = regionOf(region);
        m_regionPending = true;
    }

    void setPendingHint(const QPointF &hint)
    {
        if (!m_inert)
            m_pendingHint = hint;
    }

    // The client's object went. Nothing can be sent on it any more.
    void gone()
    {
        if (m_capture)
            m_capture->forget(this);
    }

private:
    void applyPending()
    {
        if (m_regionPending) {
            m_region = m_pendingRegion;
            m_regionPending = false;
        }
        if (m_pendingHint) {
            m_hint = m_pendingHint;
            m_pendingHint.reset();
        }
        updateEffectiveRegion();
    }

    void updateEffectiveRegion()
    {
        if (!m_surface)
            return;
        const QRegion input = QWaylandSurfacePrivate::get(m_surface.data())
                                  ->inputRegion.intersected(QRect(QPoint(0, 0), m_surface->destinationSize()));
        m_effective = m_region ? m_region->intersected(input) : input;
        if (m_effective.rectCount() > kMostRegionRects)
            m_effective = QRegion();
    }

    QPointer<PointerCapture> m_capture;
    wl_client *m_client;
    QPointer<QWaylandSurface> m_surface;
    QWaylandDestroyListener m_pointerGone;
    QMetaObject::Connection m_surfaceGone;
    QMetaObject::Connection m_committed;
    std::optional<QRegion> m_region;
    std::optional<QRegion> m_pendingRegion;
    QRegion m_effective;
    std::optional<QPointF> m_hint;
    std::optional<QPointF> m_pendingHint;
    bool m_regionPending = false;
    bool m_oneshot;
    bool m_inert = false;
    bool m_hasEnded = false;
};

class LockedPointer final : public PointerConstraint, public QtWaylandServer::zwp_locked_pointer_v1
{
public:
    LockedPointer(PointerCapture *capture, wl_client *client, uint32_t id, int version, QWaylandSurface *surface,
                  wl_resource *pointer, wl_resource *region, bool oneshot)
        : PointerConstraint(capture, client, surface, pointer, region, oneshot),
          zwp_locked_pointer_v1(client, id, version)
    { }

    bool isLock() const override { return true; }
    void sendStarted() override { send_locked(); }
    void sendEnded() override { send_unlocked(); }

protected:
    void zwp_locked_pointer_v1_destroy(Resource *resource) override { wl_resource_destroy(resource->handle); }
    void zwp_locked_pointer_v1_destroy_resource(Resource *) override
    {
        gone();
        delete this;
    }
    void zwp_locked_pointer_v1_set_cursor_position_hint(Resource *, wl_fixed_t x, wl_fixed_t y) override
    {
        setPendingHint(QPointF(wl_fixed_to_double(x), wl_fixed_to_double(y)));
    }
    void zwp_locked_pointer_v1_set_region(Resource *, wl_resource *region) override { setPendingRegion(region); }
};

class ConfinedPointer final : public PointerConstraint, public QtWaylandServer::zwp_confined_pointer_v1
{
public:
    ConfinedPointer(PointerCapture *capture, wl_client *client, uint32_t id, int version, QWaylandSurface *surface,
                    wl_resource *pointer, wl_resource *region, bool oneshot)
        : PointerConstraint(capture, client, surface, pointer, region, oneshot),
          zwp_confined_pointer_v1(client, id, version)
    { }

    bool isLock() const override { return false; }
    void sendStarted() override { send_confined(); }
    void sendEnded() override { send_unconfined(); }

protected:
    void zwp_confined_pointer_v1_destroy(Resource *resource) override { wl_resource_destroy(resource->handle); }
    void zwp_confined_pointer_v1_destroy_resource(Resource *) override
    {
        gone();
        delete this;
    }
    void zwp_confined_pointer_v1_set_region(Resource *, wl_resource *region) override { setPendingRegion(region); }
};

// One zwp_relative_pointer_v1. Without its wl_pointer it hears nothing more.
class RelativePointer final : public QtWaylandServer::zwp_relative_pointer_v1
{
public:
    RelativePointer(PointerCapture *capture, wl_client *client, uint32_t id, int version, wl_resource *pointer)
        : zwp_relative_pointer_v1(client, id, version), m_capture(capture), m_client(client)
    {
        m_pointerGone.listenForDestruction(pointer);
        QObject::connect(&m_pointerGone, &QWaylandDestroyListener::fired, capture, [this] {
            m_inert = true;
            m_pointerGone.reset();
        });
    }

    ~RelativePointer() override { m_pointerGone.reset(); }

    wl_client *client() const { return m_client; }

    void send(const PointerMotion &motion)
    {
        if (!m_inert)
            send_relative_motion(uint32_t(motion.microseconds >> 32), uint32_t(motion.microseconds),
                                 wl_fixed_from_double(motion.delta.x()), wl_fixed_from_double(motion.delta.y()),
                                 wl_fixed_from_double(motion.unaccelerated.x()),
                                 wl_fixed_from_double(motion.unaccelerated.y()));
    }

protected:
    void zwp_relative_pointer_v1_destroy(Resource *resource) override { wl_resource_destroy(resource->handle); }
    void zwp_relative_pointer_v1_destroy_resource(Resource *) override
    {
        if (m_capture)
            m_capture->forget(this);
        delete this;
    }

private:
    QPointer<PointerCapture> m_capture;
    wl_client *m_client;
    QWaylandDestroyListener m_pointerGone;
    bool m_inert = false;
};

// ── The decisions ────────────────────────────────────────────────────────────

PointerCapture::PointerCapture(QWaylandCompositor *compositor)
    : QObject(compositor), m_compositor(compositor), m_source(std::make_unique<WindowMotionSource>())
{
    connect(m_source.get(), &PointerMotionSource::moved, this, &PointerCapture::deliver);
    connect(compositor, &QWaylandCompositor::outputAdded, this, [this](QWaylandOutput *output) {
        if (m_listening && output->window())
            listenTo(output->window(), true);
        if (m_active)
            holdSceneHoverOn(qobject_cast<QQuickWindow *>(output->window()), true);
    });
    connect(qApp, &QGuiApplication::applicationStateChanged, this, &PointerCapture::hostFocusChanged);
}

// Nested, the host's focus going elsewhere: the person has left, keys and all, and what
// they did before they left stands for nothing.
void PointerCapture::hostFocusChanged(Qt::ApplicationState state)
{
    if (state == Qt::ApplicationActive)
        return;
    forgetAction();
    release();
}

PointerCapture::~PointerCapture() = default;

void PointerCapture::attach(QWaylandSeat *seat)
{
    if (m_seat)
        return;
    m_seat = seat;
    connect(seat, &QWaylandSeat::keyboardFocusChanged, this, [this](QWaylandSurface *focus) {
        if (!inWindow(m_mayCapture.data(), focus))
            forgetAction();
        update();
    });
    connect(seat, &QWaylandSeat::mouseFocusChanged, this, [this] { update(); });
}

void PointerCapture::setMayCapture(QWaylandSurface *surface)
{
    if (m_mayCapture == surface)
        return;
    m_mayCapture = surface;
    if (!inWindow(surface, m_actedOn.data()))
        forgetAction();
    update();
}

QWaylandSurface *PointerCapture::capturedBy() const { return m_active ? m_active->surface() : nullptr; }

void PointerCapture::release()
{
    if (m_active)
        end(m_active);
}

void PointerCapture::setWithheld(bool withheld)
{
    m_withheld = withheld;
    if (withheld && m_seat)
        m_seat->setMouseFocus(nullptr);
    update();
}

void PointerCapture::personActed(QWaylandSurface *surface, bool pressed)
{
    m_actedOn = surface;
    m_actedByPress = pressed;
    m_actedAt.start();
    update();
}

std::optional<QPointF> PointerCapture::allowedPosition(QWaylandView *view, const QPointF &position,
                                                       const QPointF &scenePosition) const
{
    if (m_withheld)
        return std::nullopt;
    if (!m_active) {
        // A button pressed on the client keeps the pointer its own until it is let go.
        if (m_seat && view && m_seat->mouseFocus() == view && m_seat->pointer()->isButtonPressed())
            return position;
        const auto *item = view ? qobject_cast<QWaylandQuickItem *>(view->renderObject()) : nullptr;
        if (coveredByShell(item, scenePosition))
            return std::nullopt;
        return position;
    }
    // Locked: no motion. Confined: only on that surface, and never past its region.
    if (m_active->isLock() || !view || view->surface() != m_active->surface())
        return std::nullopt;
    return nearestIn(m_active->effectiveRegion(), position);
}

void PointerCapture::pointerMoved()
{
    // A constraint waiting for the pointer to come into its region.
    if (!m_active && !m_constraints.isEmpty())
        update();
}

bool PointerCapture::isConstrained(QWaylandSurface *surface) const
{
    return std::any_of(m_constraints.cbegin(), m_constraints.cend(),
                       [surface](const PointerConstraint *c) { return c->surface() == surface; });
}

bool PointerCapture::hasRoomFor(wl_client *client) const
{
    const auto ofClient = [client](const auto *object) { return object->client() == client; };
    return std::count_if(m_constraints.cbegin(), m_constraints.cend(), ofClient)
               + std::count_if(m_relative.cbegin(), m_relative.cend(), ofClient)
           < kMostObjectsPerClient;
}

void PointerCapture::add(PointerConstraint *constraint)
{
    m_constraints.append(constraint);
    updateListening();
    update();
}

void PointerCapture::forget(PointerConstraint *constraint)
{
    const bool wasActive = m_active == constraint;
    if (wasActive) {
        forgetAction();
        stopHolding();
    }
    m_constraints.removeOne(constraint);
    updateListening();
    if (wasActive)
        Q_EMIT capturedByChanged();
}

void PointerCapture::add(RelativePointer *pointer)
{
    m_relative.append(pointer);
    updateListening();
}

void PointerCapture::forget(RelativePointer *pointer)
{
    m_relative.removeOne(pointer);
    updateListening();
}

void PointerCapture::orphan(PointerConstraint *constraint)
{
    if (m_active == constraint)
        end(constraint);
    constraint->makeInert();
}

bool PointerCapture::mayHold(const PointerConstraint *constraint) const
{
    // A confinement with nowhere left to confine to cannot go on.
    if (!m_seat || (!constraint->isLock() && constraint->effectiveRegion().isEmpty()))
        return false;
    QWaylandSurface *window = m_mayCapture.data();
    QWaylandView *under = m_seat->mouseFocus();
    return inWindow(window, constraint->surface()) && inWindow(window, m_seat->keyboardFocus()) && under
           && inWindow(window, under->surface());
}

bool PointerCapture::mayStart(const PointerConstraint *constraint) const
{
    if (QGuiApplication::applicationState() != Qt::ApplicationActive || !mayHold(constraint)
        || !constraint->surface()->hasContent() || !inWindow(m_mayCapture.data(), m_actedOn.data())
        || m_actedAt.hasExpired(kActionFreshMs))
        return false;
    // Coming back, only for a pointer button pressed on it.
    if (constraint->hasEnded() && !m_actedByPress)
        return false;
    // The protocol promises the client that the surface it constrains has the pointer's
    // focus when it starts, and the pointer must be within its region there.
    if (m_seat->mouseFocus()->surface() != constraint->surface())
        return false;
    return constraint->effectiveRegion().contains(pixelAt(m_seat->pointer()->currentLocalPosition()));
}

// Whether the motion that ends at this position begins and ends on the capturing
// window: a delta that crossed its edge would say how far off the pointer went.
bool PointerCapture::pointerStaysOnSurface(const QWindow *window, const QPointF &scenePosition) const
{
    if (!m_seat)
        return false;
    QWaylandView *under = m_seat->mouseFocus();
    if (!under || !inWindow(m_mayCapture.data(), under->surface())
        || !inWindow(m_mayCapture.data(), m_seat->keyboardFocus()))
        return false;
    const auto *item = qobject_cast<QWaylandQuickItem *>(under->renderObject());
    return item && item->window() == window && item->inputRegionContains(item->mapFromScene(scenePosition))
           && !coveredByShell(item, scenePosition);
}

void PointerCapture::update()
{
    if (m_active && !mayHold(m_active))
        end(m_active);
    if (m_active)
        return;
    for (PointerConstraint *constraint : std::as_const(m_constraints)) {
        if (mayStart(constraint)) {
            start(constraint);
            return;
        }
    }
}

void PointerCapture::start(PointerConstraint *constraint)
{
    m_active = constraint;
    m_pressedBefore = QGuiApplication::mouseButtons();
    m_pressedDuring = {};
    holdSceneHover(true);
    qCDebug(lcPointerCapture, "captured");
    constraint->sendStarted();
    Q_EMIT capturedByChanged();
}

void PointerCapture::end(PointerConstraint *constraint)
{
    // Where the client asked the pointer to come back to, on its own surface only.
    QWaylandView *under = m_seat ? m_seat->mouseFocus() : nullptr;
    const auto *item = under ? qobject_cast<QWaylandQuickItem *>(under->renderObject()) : nullptr;
    QWaylandSurface *surface = constraint->surface();
    if (constraint->cursorHint() && surface && item && under->surface() == surface && item->window()) {
        const QSize size = surface->destinationSize();
        const QPointF local(std::clamp(constraint->cursorHint()->x(), 0.0, double(std::max(size.width(), 0))),
                            std::clamp(constraint->cursorHint()->y(), 0.0, double(std::max(size.height(), 0))));
        m_source->warp(item->window(), item->mapToScene(item->mapFromSurface(local)));
    }
    // It comes back only after a new action, on any lifetime. Forgotten first: letting go
    // tells Qt's scene, which may ask again at once.
    forgetAction();
    stopHolding();
    constraint->ended();
    qCDebug(lcPointerCapture, "let go");
    Q_EMIT capturedByChanged();
}

// While something in the scene changes Qt Quick hovers again where the pointer last was,
// to tell items that moved under it. Under a held pointer, a notification coming up there
// would take the hover and with it the pointer's focus, and end the capture. It also saves
// that walk on the frames of a running game. A negative interval is how Qt turns it off
// (QQuickDeliveryAgentPrivate::flushFrameSynchronousEvents).
void PointerCapture::holdSceneHover(bool hold)
{
    const QList<QWaylandOutput *> outputs = m_compositor->outputs();
    for (QWaylandOutput *output : outputs)
        holdSceneHoverOn(qobject_cast<QQuickWindow *>(output->window()), hold);
    if (!hold)
        m_sceneHoverIntervals.clear();
}

// Each window's own interval comes back as it was, whatever it was: a negative one set
// by QT_QUICK_FRAME_SYNCHRONOUS_HOVER_INTERVAL stays negative.
void PointerCapture::holdSceneHoverOn(QQuickWindow *window, bool hold)
{
    if (!window)
        return;
    QQuickDeliveryAgentPrivate *agent = QQuickWindowPrivate::get(window)->deliveryAgentPrivate();
    if (hold) {
        if (!m_sceneHoverIntervals.contains(window))
            m_sceneHoverIntervals.insert(window, agent->frameSynchronousHoverInterval);
        agent->frameSynchronousHoverInterval = -1;
    } else if (const auto held = m_sceneHoverIntervals.constFind(window); held != m_sceneHoverIntervals.cend()) {
        agent->frameSynchronousHoverInterval = *held;
    }
}

void PointerCapture::stopHolding()
{
    m_active = nullptr;
    m_pointerOnSurface = false;
    holdSceneHover(false);
    // Buttons the client was told went down, and Qt never saw.
    for (uint bit = 1; bit <= uint(Qt::MaxMouseButton); bit <<= 1) {
        const auto button = Qt::MouseButton(bit);
        if (m_seat && m_pressedDuring.testFlag(button))
            m_seat->sendMouseReleaseEvent(button);
    }
    m_pressedDuring = {};
    m_pressedBefore = {};
    // The host's pointer went while held, and Qt's scene still thinks it is there: it is
    // told now, and the client has the pointer's focus no more.
    const QList<QPointer<QWindow>> left = std::exchange(m_leftWhileHeld, {});
    if (!left.isEmpty() && m_seat)
        m_seat->setMouseFocus(nullptr);
    for (const QPointer<QWindow> &window : left) {
        if (window) {
            QEvent leave(QEvent::Leave);
            QCoreApplication::sendEvent(window.data(), &leave);
        }
    }
}

void PointerCapture::forgetAction()
{
    m_actedOn = nullptr;
    m_actedByPress = false;
}

void PointerCapture::listenTo(QWindow *window, bool listen)
{
    if (listen)
        window->installEventFilter(this);
    else
        window->removeEventFilter(this);
}

void PointerCapture::updateListening()
{
    const bool wanted = !m_constraints.isEmpty() || !m_relative.isEmpty();
    if (wanted == m_listening)
        return;
    m_listening = wanted;
    if (wanted)
        m_source->restart();
    const QList<QWaylandOutput *> outputs = m_compositor->outputs();
    for (QWaylandOutput *output : outputs)
        if (QWindow *window = output->window())
            listenTo(window, wanted);
}

void PointerCapture::deliver(const PointerMotion &motion)
{
    if (!m_pointerOnSurface || !m_mayCapture || !m_mayCapture->client())
        return;
    const wl_client *hearer = m_mayCapture->client()->client();
    for (RelativePointer *pointer : std::as_const(m_relative))
        if (pointer->client() == hearer)
            pointer->send(motion);
}

bool PointerCapture::eventFilter(QObject *watched, QEvent *event)
{
    auto *window = static_cast<QWindow *>(watched); // It is only ever installed on an output's window.
    switch (event->type()) {
    case QEvent::MouseMove:
        m_pointerOnSurface =
            m_active || pointerStaysOnSurface(window, static_cast<QMouseEvent *>(event)->scenePosition());
        m_source->windowEvent(window, event);
        break;
    case QEvent::Enter:
    case QEvent::Leave: m_source->windowEvent(window, event); break;
    default: break;
    }
    return m_active && filterWhileHeld(window, event);
}

bool PointerCapture::filterWhileHeld(QWindow *window, QEvent *event)
{
    switch (event->type()) {
    case QEvent::Enter: m_leftWhileHeld.removeAll(window); return true;
    case QEvent::Leave:
        if (!m_leftWhileHeld.contains(window))
            m_leftWhileHeld.append(window);
        return true;
    case QEvent::MouseButtonDblClick: return true; // Its press has gone to the client already.
    case QEvent::MouseMove:
        if (!m_active->isLock() && m_seat && m_seat->mouseFocus()) {
            // Confined: only the client's view of the position moves, within its region.
            QWaylandView *view = m_seat->mouseFocus();
            const auto *item = qobject_cast<QWaylandQuickItem *>(view->renderObject());
            if (item && item->window() == window) {
                const QPointF scene = static_cast<QMouseEvent *>(event)->scenePosition();
                m_seat->sendMouseMoveEvent(view, item->mapToSurface(item->mapFromScene(scene)), scene);
            }
        }
        return true;
    case QEvent::MouseButtonPress: {
        const Qt::MouseButton button = static_cast<QMouseEvent *>(event)->button();
        m_pressedDuring |= button;
        if (m_seat)
            m_seat->sendMousePressEvent(button);
        return true;
    }
    case QEvent::MouseButtonRelease: {
        const Qt::MouseButton button = static_cast<QMouseEvent *>(event)->button();
        if (m_pressedBefore.testFlag(button)) {
            m_pressedBefore &= ~Qt::MouseButtons(button);
            return false;
        }
        m_pressedDuring &= ~Qt::MouseButtons(button);
        if (m_seat)
            m_seat->sendMouseReleaseEvent(button);
        return true;
    }
    case QEvent::Wheel: {
        const QPoint angle = static_cast<QWheelEvent *>(event)->angleDelta();
        if (m_seat && angle.x() != 0)
            m_seat->sendMouseWheelEvent(Qt::Horizontal, angle.x());
        if (m_seat && angle.y() != 0)
            m_seat->sendMouseWheelEvent(Qt::Vertical, angle.y());
        return true;
    }
    default: return false;
    }
}

// ── The compositor ───────────────────────────────────────────────────────────

PrismCompositor::PrismCompositor() : m_capture(new PointerCapture(this))
{
    connect(m_capture, &PointerCapture::capturedByChanged, this, &PrismCompositor::pointerCapturedByChanged);
}

QWaylandSurface *PrismCompositor::mayCapturePointer() const { return m_capture->mayCapture(); }

void PrismCompositor::setMayCapturePointer(QWaylandSurface *surface)
{
    if (m_capture->mayCapture() == surface)
        return;
    m_capture->setMayCapture(surface);
    Q_EMIT mayCapturePointerChanged();
}

QWaylandSurface *PrismCompositor::pointerCapturedBy() const { return m_capture->capturedBy(); }

bool PrismCompositor::pointerWithheld() const { return m_capture->isWithheld(); }

void PrismCompositor::setPointerWithheld(bool withheld)
{
    if (m_capture->isWithheld() == withheld)
        return;
    m_capture->setWithheld(withheld);
    Q_EMIT pointerWithheldChanged();
}

void PrismCompositor::releasePointer() { m_capture->release(); }

QWaylandPointer *PrismCompositor::createPointerDevice(QWaylandSeat *seat)
{
    m_capture->attach(seat);
    return new CapturingPointer(seat, m_capture);
}

QWaylandKeyboard *PrismCompositor::createKeyboardDevice(QWaylandSeat *seat)
{
    return new WatchedKeyboard(seat, m_capture);
}

// ── The globals ──────────────────────────────────────────────────────────────

void RelativePointerManager::initialize()
{
    QWaylandCompositorExtensionTemplate::initialize();
    auto *compositor = qobject_cast<QWaylandCompositor *>(extensionContainer());
    if (!captureOf(this)) {
        qCWarning(lcPointerCapture, "RelativePointerManagerV1 must be declared inside a PrismCompositor");
        return;
    }
    init(compositor->display(), 1);
}

void RelativePointerManager::componentComplete()
{
    if (!isInitialized())
        initialize();
}

void RelativePointerManager::zwp_relative_pointer_manager_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

void RelativePointerManager::zwp_relative_pointer_manager_v1_get_relative_pointer(Resource *resource, uint32_t id,
                                                                                  wl_resource *pointer)
{
    PointerCapture *capture = captureOf(this);
    if (!capture || !capture->hasRoomFor(resource->client())) {
        wl_client_post_implementation_error(resource->client(), "too many pointer objects");
        return;
    }
    capture->add(new RelativePointer(capture, resource->client(), id, resource->version(), pointer));
}

void PointerConstraintsManager::initialize()
{
    QWaylandCompositorExtensionTemplate::initialize();
    auto *compositor = qobject_cast<QWaylandCompositor *>(extensionContainer());
    if (!captureOf(this)) {
        qCWarning(lcPointerCapture, "PointerConstraintsV1 must be declared inside a PrismCompositor");
        return;
    }
    init(compositor->display(), 1);
}

void PointerConstraintsManager::componentComplete()
{
    if (!isInitialized())
        initialize();
}

void PointerConstraintsManager::zwp_pointer_constraints_v1_destroy(Resource *resource)
{
    wl_resource_destroy(resource->handle);
}

// What every request must pass; on a refusal the client has been told, and is ended.
PointerCapture *PointerConstraintsManager::acceptable(Resource *resource, wl_resource *surfaceResource, uint32_t asked)
{
    PointerCapture *capture = captureOf(this);
    QWaylandSurface *surface = QWaylandSurface::fromResource(surfaceResource);
    if (!capture || !surface || (asked != lifetime_oneshot && asked != lifetime_persistent)) {
        wl_client_post_implementation_error(resource->client(), "not a surface, or no such lifetime");
        return nullptr;
    }
    if (capture->isConstrained(surface)) {
        wl_resource_post_error(resource->handle, error_already_constrained,
                               "the surface already has a pointer lock or confinement");
        return nullptr;
    }
    if (!capture->hasRoomFor(resource->client())) {
        wl_client_post_implementation_error(resource->client(), "too many pointer objects");
        return nullptr;
    }
    return capture;
}

void PointerConstraintsManager::zwp_pointer_constraints_v1_lock_pointer(Resource *resource, uint32_t id,
                                                                        wl_resource *surface, wl_resource *pointer,
                                                                        wl_resource *region, uint32_t asked)
{
    if (PointerCapture *capture = acceptable(resource, surface, asked))
        capture->add(new LockedPointer(capture, resource->client(), id, resource->version(),
                                       QWaylandSurface::fromResource(surface), pointer, region,
                                       asked == lifetime_oneshot));
}

void PointerConstraintsManager::zwp_pointer_constraints_v1_confine_pointer(Resource *resource, uint32_t id,
                                                                           wl_resource *surface, wl_resource *pointer,
                                                                           wl_resource *region, uint32_t asked)
{
    if (PointerCapture *capture = acceptable(resource, surface, asked))
        capture->add(new ConfinedPointer(capture, resource->client(), id, resource->version(),
                                         QWaylandSurface::fromResource(surface), pointer, region,
                                         asked == lifetime_oneshot));
}

} // namespace qor::prism
