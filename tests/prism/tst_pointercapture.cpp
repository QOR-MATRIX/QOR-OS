// SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
//
// Pointer lock, confinement and relative motion, from the client's side (QOR-0020,
// QOR-0018). Each client here is a raw libwayland one, free to do what Qt's would never
// send. A real Prism runs offscreen and in software, with no display and no graphics
// card, and a person's input comes through its script runner, one step at a time
// ({"follow": folder}), so that what comes next can depend on what a client was told.
//
// A case shows both halves: what is allowed works, and what is refused is refused. What a
// client was told is read after a roundtrip, so "it heard nothing" is a fact and not a
// matter of how long the test waited.

#include "wayland-cursor-shape-v1-client-protocol.h"
#include "wayland-fractional-scale-v1-client-protocol.h"
#include "wayland-pointer-constraints-unstable-v1-client-protocol.h"
#include "wayland-relative-pointer-unstable-v1-client-protocol.h"
#include "wayland-xdg-activation-v1-client-protocol.h"
#include "wayland-xdg-shell-client-protocol.h"

#include <QtCore/QDir>
#include <QtCore/QElapsedTimer>
#include <QtCore/QFile>
#include <QtCore/QHash>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QProcess>
#include <QtCore/QRect>
#include <QtCore/QSaveFile>
#include <QtCore/QTemporaryDir>
#include <QtTest/QTest>

#include <linux/input-event-codes.h>
#include <poll.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <wayland-client.h>

#include <algorithm>
#include <climits>
#include <csignal>
#include <functional>
#include <memory>
#include <vector>

using namespace Qt::StringLiterals;

namespace {

constexpr QSize kOutput(1600, 1000);
constexpr int kSeatVersion = 3; // wl_pointer.release, and no frame events to wait for.
// Past pointercapture.cpp's limit on what a client may keep, whatever it is exactly.
constexpr int kManyObjects = 64;
// A window's subsurfaces, side by side: far more than pointercapture.cpp looks through.
constexpr int kFanOut = 5000;

struct Motion
{
    QPointF delta;
    QPointF unaccelerated;
    quint64 microseconds = 0;
};

// What a client has been told.
struct Seen
{
    int pointerEnters = 0;
    int pointerLeaves = 0;
    QList<QPointF> motions;
    QList<uint32_t> buttonsDown;
    QList<uint32_t> buttonsUp;
    int axes = 0;
    int keyboardEnters = 0;
    int keyboardLeaves = 0;
    QList<uint32_t> keysDown;
    QList<Motion> relative;

    QPointF heardSince(qsizetype from) const
    {
        QPointF sum;
        for (qsizetype i = from; i < relative.size(); ++i)
            sum += relative.at(i).delta;
        return sum;
    }
};

class Client;

// A lock or a confinement, and what it has been told.
struct Capture
{
    Client *client = nullptr;
    zwp_locked_pointer_v1 *locked = nullptr;
    zwp_confined_pointer_v1 *confined = nullptr;
    int started = 0;
    int ended = 0;

    void setRegion(wl_region *region);
    void destroy();
};

class Window;

class Client
{
public:
    explicit Client(const QByteArray &socket);
    ~Client();
    Client(const Client &) = delete;
    Client &operator=(const Client &) = delete;

    bool isUsable() const { return m_display && compositor && shm && wmBase && seat && pointer && keyboard; }
    bool isAlive() const { return m_display && wl_display_get_error(m_display) == 0; }
    wl_display *display() const { return m_display; }
    // Everything sent has been handled, and everything Prism sent since has been heard.
    bool roundtrip() { return isAlive() && wl_display_roundtrip(m_display) >= 0; }
    // Whatever has arrived, without waiting for it.
    void dispatch();
    // Hangs up with no goodbye, as a client that crashed does.
    void vanish() { shutdown(wl_display_get_fd(m_display), SHUT_RDWR); }

    template<typename T>
    T *keep(T *proxy)
    {
        m_proxies.append(reinterpret_cast<wl_proxy *>(proxy));
        return proxy;
    }
    // A request has destroyed it.
    template<typename T>
    void dropped(T *proxy)
    {
        m_proxies.removeOne(reinterpret_cast<wl_proxy *>(proxy));
    }

    Capture *lock(wl_surface *surface, wl_region *region = nullptr,
                  uint32_t lifetime = ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT, wl_pointer *with = nullptr);
    Capture *confine(wl_surface *surface, wl_region *region = nullptr,
                     uint32_t lifetime = ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT);
    zwp_relative_pointer_v1 *listenToMotion(wl_pointer *with = nullptr);
    wl_pointer *anotherPointer();
    wl_region *region(const QList<QRect> &rects);
    Window *openWindow(const QString &title, QSize size);
    wl_proxy *bindGlobal(const wl_interface *type, uint32_t version);

    Seen seen;
    QHash<QByteArray, uint32_t> globals;
    QHash<QByteArray, int> globalAnnouncements;
    wl_compositor *compositor = nullptr;
    wl_subcompositor *subcompositor = nullptr;
    wl_shm *shm = nullptr;
    xdg_wm_base *wmBase = nullptr;
    wl_seat *seat = nullptr;
    wl_pointer *pointer = nullptr;
    wl_keyboard *keyboard = nullptr;
    zwp_pointer_constraints_v1 *constraints = nullptr;
    zwp_relative_pointer_manager_v1 *relativeManager = nullptr;

private:
    wl_display *m_display = nullptr;
    wl_registry *m_registry = nullptr;
    QHash<QByteArray, uint32_t> m_globalNames;
    QList<wl_proxy *> m_proxies;
    std::vector<std::unique_ptr<Capture>> m_captures;
    std::vector<std::unique_ptr<Window>> m_windows;
};

void Capture::setRegion(wl_region *region)
{
    if (locked)
        zwp_locked_pointer_v1_set_region(locked, region);
    else
        zwp_confined_pointer_v1_set_region(confined, region);
}

void Capture::destroy()
{
    if (locked) {
        client->dropped(locked);
        zwp_locked_pointer_v1_destroy(locked);
        locked = nullptr;
    } else if (confined) {
        client->dropped(confined);
        zwp_confined_pointer_v1_destroy(confined);
        confined = nullptr;
    }
}

// A toplevel with a picture in it, as a game's window would be.
class Window
{
public:
    Window(Client *client, const QString &title, QSize size) : m_client(client), m_size(size)
    {
        surface = client->keep(wl_compositor_create_surface(client->compositor));
        xdg = client->keep(xdg_wm_base_get_xdg_surface(client->wmBase, surface));
        static const xdg_surface_listener surfaceListener = [] {
            xdg_surface_listener listener {};
            listener.configure = [](void *data, xdg_surface *object, uint32_t serial) {
                auto *window = static_cast<Window *>(data);
                if (window->deferConfigures) {
                    window->deferredConfigures.append({ serial, window->m_configuredSize });
                    return;
                }
                xdg_surface_ack_configure(object, serial);
                window->draw();
            };
            return listener;
        }();
        xdg_surface_add_listener(xdg, &surfaceListener, this);
        toplevel = client->keep(xdg_surface_get_toplevel(xdg));
        static const xdg_toplevel_listener toplevelListener = [] {
            xdg_toplevel_listener listener {};
            listener.configure = [](void *data, xdg_toplevel *, int32_t width, int32_t height, wl_array *) {
                static_cast<Window *>(data)->m_configuredSize = QSize(width, height);
            };
            listener.close = [](void *, xdg_toplevel *) { };
            return listener;
        }();
        xdg_toplevel_add_listener(toplevel, &toplevelListener, this);
        xdg_toplevel_set_title(toplevel, title.toUtf8().constData());
        // The Shelf brings a window forward by it.
        xdg_toplevel_set_app_id(toplevel,
                                title.toLower().replace(u' ', u'-').prepend(u"qor-test-"_s).toUtf8().constData());
        wl_surface_commit(surface);
    }

    wl_buffer *makeBuffer(QSize size)
    {
        const int stride = size.width() * 4;
        const int bytes = stride * size.height();
        const int fd = memfd_create("qor-pointer-capture", MFD_CLOEXEC);
        if (fd < 0 || ftruncate(fd, bytes) != 0)
            qFatal("no memory for a buffer");
        void *pixels = mmap(nullptr, size_t(bytes), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (pixels == MAP_FAILED)
            qFatal("no memory for a buffer");
        auto *words = static_cast<uint32_t *>(pixels);
        for (int i = 0; i < bytes / 4; ++i)
            words[i] = 0xff20'2230;
        munmap(pixels, size_t(bytes));
        wl_shm_pool *pool = wl_shm_create_pool(m_client->shm, fd, bytes);
        wl_buffer *buffer = m_client->keep(
            wl_shm_pool_create_buffer(pool, 0, size.width(), size.height(), stride, WL_SHM_FORMAT_XRGB8888));
        wl_shm_pool_destroy(pool);
        close(fd);
        return buffer;
    }

    void draw()
    {
        if (!surface)
            return;
        if (!m_buffer)
            m_buffer = makeBuffer(m_size);
        wl_surface_attach(surface, m_buffer, 0, 0);
        wl_surface_damage(surface, 0, 0, m_size.width(), m_size.height());
        wl_surface_commit(surface);
    }

    // Goes properly: the role first, then the surface.
    void destroy()
    {
        m_client->dropped(toplevel);
        xdg_toplevel_destroy(toplevel);
        m_client->dropped(xdg);
        xdg_surface_destroy(xdg);
        m_client->dropped(surface);
        wl_surface_destroy(surface);
        surface = nullptr;
    }

    wl_surface *surface = nullptr;
    xdg_surface *xdg = nullptr;
    xdg_toplevel *toplevel = nullptr;
    struct Configure
    {
        uint32_t serial;
        QSize size;
    };
    bool deferConfigures = false;
    QList<Configure> deferredConfigures;

private:
    Client *m_client;
    QSize m_size;
    QSize m_configuredSize;
    wl_buffer *m_buffer = nullptr;
};

const wl_pointer_listener &pointerListener()
{
    static const wl_pointer_listener listener = [] {
        wl_pointer_listener l {};
        l.enter = [](void *data, wl_pointer *, uint32_t, wl_surface *, wl_fixed_t, wl_fixed_t) {
            ++static_cast<Client *>(data)->seen.pointerEnters;
        };
        l.leave = [](void *data, wl_pointer *, uint32_t, wl_surface *) {
            ++static_cast<Client *>(data)->seen.pointerLeaves;
        };
        l.motion = [](void *data, wl_pointer *, uint32_t, wl_fixed_t x, wl_fixed_t y) {
            static_cast<Client *>(data)->seen.motions.append(QPointF(wl_fixed_to_double(x), wl_fixed_to_double(y)));
        };
        l.button = [](void *data, wl_pointer *, uint32_t, uint32_t, uint32_t button, uint32_t state) {
            Seen &seen = static_cast<Client *>(data)->seen;
            (state == WL_POINTER_BUTTON_STATE_PRESSED ? seen.buttonsDown : seen.buttonsUp).append(button);
        };
        l.axis = [](void *data, wl_pointer *, uint32_t, uint32_t, wl_fixed_t) {
            ++static_cast<Client *>(data)->seen.axes;
        };
        return l;
    }();
    return listener;
}

Client::Client(const QByteArray &socket)
{
    m_display = wl_display_connect(socket.constData());
    if (!m_display)
        return;
    m_registry = keep(wl_display_get_registry(m_display));
    static const wl_registry_listener registryListener = [] {
        wl_registry_listener l {};
        l.global = [](void *data, wl_registry *object, uint32_t name, const char *interface,
                      uint32_t advertisedVersion) {
            auto *self = static_cast<Client *>(data);
            const QByteArray which(interface);
            self->globals.insert(which, advertisedVersion);
            ++self->globalAnnouncements[which];
            self->m_globalNames.insert(which, name);
            const auto bind = [&](const wl_interface *type, uint32_t version) {
                return self->keep(static_cast<wl_proxy *>(wl_registry_bind(object, name, type, version)));
            };
            if (which == wl_compositor_interface.name) {
                self->compositor = reinterpret_cast<wl_compositor *>(bind(&wl_compositor_interface, 4));
            } else if (which == wl_subcompositor_interface.name) {
                self->subcompositor = reinterpret_cast<wl_subcompositor *>(bind(&wl_subcompositor_interface, 1));
            } else if (which == wl_shm_interface.name) {
                self->shm = reinterpret_cast<wl_shm *>(bind(&wl_shm_interface, 1));
            } else if (which == xdg_wm_base_interface.name) {
                self->wmBase = reinterpret_cast<xdg_wm_base *>(bind(&xdg_wm_base_interface, 1));
                static const xdg_wm_base_listener wmListener = [] {
                    xdg_wm_base_listener w {};
                    w.ping = [](void *, xdg_wm_base *base, uint32_t serial) { xdg_wm_base_pong(base, serial); };
                    return w;
                }();
                xdg_wm_base_add_listener(self->wmBase, &wmListener, self);
            } else if (which == wl_seat_interface.name) {
                self->seat = reinterpret_cast<wl_seat *>(bind(&wl_seat_interface, kSeatVersion));
            } else if (which == zwp_pointer_constraints_v1_interface.name) {
                self->constraints =
                    reinterpret_cast<zwp_pointer_constraints_v1 *>(bind(&zwp_pointer_constraints_v1_interface, 1));
            } else if (which == zwp_relative_pointer_manager_v1_interface.name) {
                self->relativeManager = reinterpret_cast<zwp_relative_pointer_manager_v1 *>(
                    bind(&zwp_relative_pointer_manager_v1_interface, 1));
            }
        };
        l.global_remove = [](void *, wl_registry *, uint32_t) { };
        return l;
    }();
    wl_registry_add_listener(m_registry, &registryListener, this);
    wl_display_roundtrip(m_display);
    if (!seat)
        return;
    pointer = keep(wl_seat_get_pointer(seat));
    wl_pointer_add_listener(pointer, &pointerListener(), this);
    keyboard = keep(wl_seat_get_keyboard(seat));
    static const wl_keyboard_listener keyboardListener = [] {
        wl_keyboard_listener l {};
        l.keymap = [](void *, wl_keyboard *, uint32_t, int32_t fd, uint32_t) { close(fd); };
        l.enter = [](void *data, wl_keyboard *, uint32_t, wl_surface *, wl_array *) {
            ++static_cast<Client *>(data)->seen.keyboardEnters;
        };
        l.leave = [](void *data, wl_keyboard *, uint32_t, wl_surface *) {
            ++static_cast<Client *>(data)->seen.keyboardLeaves;
        };
        l.key = [](void *data, wl_keyboard *, uint32_t, uint32_t, uint32_t key, uint32_t state) {
            if (state == WL_KEYBOARD_KEY_STATE_PRESSED)
                static_cast<Client *>(data)->seen.keysDown.append(key);
        };
        l.modifiers = [](void *, wl_keyboard *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) { };
        return l;
    }();
    wl_keyboard_add_listener(keyboard, &keyboardListener, this);
    wl_display_roundtrip(m_display);
}

wl_proxy *Client::bindGlobal(const wl_interface *type, uint32_t version)
{
    if (!m_registry || !isAlive() || !m_globalNames.contains(type->name) || version == 0
        || version > globals.value(type->name))
        return nullptr;
    return keep(static_cast<wl_proxy *>(wl_registry_bind(m_registry, m_globalNames.value(type->name), type, version)));
}

Client::~Client()
{
    if (!m_display)
        return;
    // Client-side only: nothing is asked of Prism, and it learns from the hang-up.
    for (wl_proxy *proxy : std::as_const(m_proxies))
        wl_proxy_destroy(proxy);
    wl_display_disconnect(m_display);
}

void Client::dispatch()
{
    if (!isAlive())
        return;
    while (wl_display_prepare_read(m_display) != 0) {
        if (wl_display_dispatch_pending(m_display) < 0)
            return;
    }
    wl_display_flush(m_display);
    pollfd descriptor { wl_display_get_fd(m_display), POLLIN, 0 };
    if (poll(&descriptor, 1, 0) > 0)
        wl_display_read_events(m_display);
    else
        wl_display_cancel_read(m_display);
    wl_display_dispatch_pending(m_display);
}

Capture *Client::lock(wl_surface *surface, wl_region *region, uint32_t lifetime, wl_pointer *with)
{
    auto capture = std::make_unique<Capture>();
    capture->client = this;
    capture->locked =
        keep(zwp_pointer_constraints_v1_lock_pointer(constraints, surface, with ? with : pointer, region, lifetime));
    static const zwp_locked_pointer_v1_listener listener = [] {
        zwp_locked_pointer_v1_listener l {};
        l.locked = [](void *data, zwp_locked_pointer_v1 *) { ++static_cast<Capture *>(data)->started; };
        l.unlocked = [](void *data, zwp_locked_pointer_v1 *) { ++static_cast<Capture *>(data)->ended; };
        return l;
    }();
    zwp_locked_pointer_v1_add_listener(capture->locked, &listener, capture.get());
    m_captures.push_back(std::move(capture));
    return m_captures.back().get();
}

Capture *Client::confine(wl_surface *surface, wl_region *region, uint32_t lifetime)
{
    auto capture = std::make_unique<Capture>();
    capture->client = this;
    capture->confined =
        keep(zwp_pointer_constraints_v1_confine_pointer(constraints, surface, pointer, region, lifetime));
    static const zwp_confined_pointer_v1_listener listener = [] {
        zwp_confined_pointer_v1_listener l {};
        l.confined = [](void *data, zwp_confined_pointer_v1 *) { ++static_cast<Capture *>(data)->started; };
        l.unconfined = [](void *data, zwp_confined_pointer_v1 *) { ++static_cast<Capture *>(data)->ended; };
        return l;
    }();
    zwp_confined_pointer_v1_add_listener(capture->confined, &listener, capture.get());
    m_captures.push_back(std::move(capture));
    return m_captures.back().get();
}

zwp_relative_pointer_v1 *Client::listenToMotion(wl_pointer *with)
{
    auto *relative = keep(zwp_relative_pointer_manager_v1_get_relative_pointer(relativeManager, with ? with : pointer));
    static const zwp_relative_pointer_v1_listener listener = [] {
        zwp_relative_pointer_v1_listener l {};
        l.relative_motion = [](void *data, zwp_relative_pointer_v1 *, uint32_t high, uint32_t low, wl_fixed_t dx,
                               wl_fixed_t dy, wl_fixed_t rawX, wl_fixed_t rawY) {
            Motion motion;
            motion.delta = QPointF(wl_fixed_to_double(dx), wl_fixed_to_double(dy));
            motion.unaccelerated = QPointF(wl_fixed_to_double(rawX), wl_fixed_to_double(rawY));
            motion.microseconds = (quint64(high) << 32) | low;
            static_cast<Client *>(data)->seen.relative.append(motion);
        };
        return l;
    }();
    zwp_relative_pointer_v1_add_listener(relative, &listener, this);
    return relative;
}

wl_pointer *Client::anotherPointer()
{
    wl_pointer *another = keep(wl_seat_get_pointer(seat));
    wl_pointer_add_listener(another, &pointerListener(), this);
    return another;
}

wl_region *Client::region(const QList<QRect> &rects)
{
    wl_region *made = keep(wl_compositor_create_region(compositor));
    for (const QRect &rect : rects)
        wl_region_add(made, rect.x(), rect.y(), rect.width(), rect.height());
    return made;
}

Window *Client::openWindow(const QString &title, QSize size)
{
    m_windows.push_back(std::make_unique<Window>(this, title, size));
    return m_windows.back().get();
}

// A client with a window Prism has settled on the screen.
struct Game
{
    explicit Game(QList<Client *> *pumped) : m_pumped(pumped) { }
    ~Game()
    {
        if (client)
            m_pumped->removeOne(client.get());
    }

    std::unique_ptr<Client> client;
    Window *window = nullptr;
    QString title;
    QRect area;

    QPoint centre() const { return area.center(); }
    // A point on the surface, from its corner.
    QPoint at(int x, int y) const { return area.topLeft() + QPoint(x, y); }

private:
    QList<Client *> *m_pumped;
};

QRect rectOf(const QJsonValue &value)
{
    const QJsonArray array = value.toArray();
    return QRect(array.at(0).toInt(), array.at(1).toInt(), array.at(2).toInt(), array.at(3).toInt());
}

} // namespace

#define DO(expression) QVERIFY2((expression), qPrintable(m_problem))

class TestPointerCapture : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void cleanupTestCase();

    void offersBothGlobals();
    void offersAllProtocolGlobals();
    void protocolManagersCanBeReleasedAndRebound();
    void aClientSizeWaitsForTheLatestConfigureAcknowledgement();
    void aWindowOpeningAsAnotherGoesIsNotLost();
    void aLockWaitsForAPerson();
    void aKeyIsAnActionAndAModifierOrARepeatIsNot();
    void anOldActionDoesNotCount();
    void aWindowAppearingUnderThePointerDoesNotCapture();
    void aSurfaceWithNothingOnItTakesNothing();
    void onlyTheSurfaceWithTheKeyboardMayLock();
    void onlyTheSurfaceWithTheKeyboardHearsMotion();
    void losingTheKeyboardEndsTheLock();
    void theLockScreenEndsItAndItStaysEnded();
    void aShellSurfaceEndsIt();
    void theHostTakingTheFocusEndsIt();
    void aPointerThatLeftWhileHeldIsGoneAfter();
    void anActionBeforeTheShellCameUpIsForgotten();
    void theWayOutCannotBeHeardOrHeldOnTo();
    void guideOnAControllerIsAWayOutToo();
    void whileHeldNothingReachesTheShellOrSaysWhereThePointerIs();
    void aNotificationUnderAHeldPointerChangesNothing();
    void theShellOverAWindowKeepsThePointerFromIt();
    void aButtonPressedBeforeTheLockIsReleasedToTheClient();
    void aButtonHeldWhenItEndsIsReleased();
    void aOneshotEndsForGoodAndAPersistentComesBack();
    void aSecondConstraintOnASurfaceIsAnError();
    void anUnknownLifetimeIsAnError();
    void regionsSayWhereThePointerMustBe();
    void badRegionsDoNoHarm();
    void aDestroyedSurfaceEndsTheLock();
    void aDestroyedPointerEndsTheLock();
    void aClientVanishingEndsTheLock();
    void requestsOnAnInertObjectAreIgnored();
    void floodsAreRefused();
    void subsurfaces();
    void aHostileSubsurfaceTreeDoesNoHarm();
    void confinementKeepsThePointerInItsRegion();
    void aConfinementWithNowhereLeftEnds();
    void nothingIsDrawnWhileNothingMoves();
    void aHeldPointerGoesNowhereOnAnotherDisplay();

private:
    bool startPrism(int outputs = 1);
    bool stopPrism();
    bool run(const QString &json);
    bool moveTo(QPoint point) { return run(u"{\"move\":[%1,%2]}"_s.arg(point.x()).arg(point.y())); }
    // Motion is measured from one position to the next, so the first move after a client
    // begins listening says only where the pointer is. This makes sure that has happened.
    bool settleOn(QPoint point) { return moveTo(point + QPoint(1, 1)) && moveTo(point); }
    bool click(QPoint point) { return run(u"{\"click\":[%1,%2]}"_s.arg(point.x()).arg(point.y())); }
    bool press(QPoint point) { return run(u"{\"press\":[%1,%2]}"_s.arg(point.x()).arg(point.y())); }
    bool release(QPoint point) { return run(u"{\"release\":[%1,%2]}"_s.arg(point.x()).arg(point.y())); }
    bool key(const QString &sequence) { return run(u"{\"key\":\"%1\"}"_s.arg(sequence)); }
    bool call(const QString &method, const QString &args = u"[]"_s)
    {
        return run(u"{\"call\":\"%1\",\"args\":%2}"_s.arg(method, args));
    }
    bool wheel(QPoint point)
    {
        return run(u"{\"scroll\":{\"at\":[%1,%2],\"notches\":1}}"_s.arg(point.x()).arg(point.y()));
    }
    bool state(QJsonObject *out);
    bool windowState(const QString &title, QJsonObject *out);
    bool settled(Game *game);
    void pump(int milliseconds);
    // Opens a game's window, and waits for it to come to rest.
    std::unique_ptr<Game> open(const QString &title, QSize size = QSize(640, 480));
    // With its keyboard and the pointer on it, and locked by a click: the usual start.
    Capture *lockedBy(Game *game, uint32_t lifetime = ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT);
    std::unique_ptr<Client> connectClient();

    QProcess m_prism;
    std::unique_ptr<QTemporaryDir> m_home;
    QByteArray m_socket;
    int m_nextStep = 0;
    int m_nextState = 0;
    QString m_problem;
    QList<Client *> m_clients; // Those a game has: heard from while waiting on Prism.
};

// ── The harness ──────────────────────────────────────────────────────────────

bool TestPointerCapture::startPrism(int outputs)
{
    m_home = std::make_unique<QTemporaryDir>();
    m_nextStep = 0;
    m_problem = u"could not make a home for Prism"_s;
    if (!m_home->isValid())
        return false;
    const QDir home(m_home->path());
    if (!home.mkdir(u"runtime"_s)
        || !QFile::setPermissions(home.filePath(u"runtime"_s), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner))
        return false;
    QFile script(home.filePath(u"script.json"_s));
    if (!script.open(QIODevice::WriteOnly))
        return false;
    script.write(R"({"steps":[{"follow":"."}]})");
    script.close();

    // As tools/dev/prism-products.py: nothing of the session it runs in, nothing shown, no
    // graphics card, no sound, no real controller.
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    for (const QString &name : { u"DISPLAY"_s, u"WAYLAND_DISPLAY"_s, u"XDG_ACTIVATION_TOKEN"_s })
        env.remove(name);
    env.insert(u"XDG_RUNTIME_DIR"_s, home.filePath(u"runtime"_s));
    env.insert(u"XDG_CONFIG_HOME"_s, home.filePath(u"config"_s));
    env.insert(u"XDG_DATA_HOME"_s, home.filePath(u"data"_s));
    env.insert(u"QT_QPA_PLATFORM"_s, u"offscreen"_s);
    env.insert(u"QT_QUICK_BACKEND"_s, u"software"_s);
    env.insert(u"GALLIUM_DRIVER"_s, u"llvmpipe"_s);
    env.insert(u"LIBGL_ALWAYS_SOFTWARE"_s, u"1"_s);
    env.insert(u"QT_FORCE_STDERR_LOGGING"_s, u"1"_s);
    env.insert(u"QOR_AUDIO_OUTPUT"_s, u"null"_s);
    env.insert(u"QOR_GAMEPADS"_s, u"virtual"_s);
    env.insert(u"QOR_TOUCHPAD"_s, u"off"_s);
    m_prism.setProcessEnvironment(env);
    m_prism.setProcessChannelMode(QProcess::ForwardedChannels);
    // Should this suite be killed, Prism goes with it.
    m_prism.setChildProcessModifier([] { prctl(PR_SET_PDEATHSIG, SIGKILL); });
    m_prism.start(QStringLiteral(QOR_PRISM),
                  { u"--unlocked"_s, u"--size"_s, u"%1x%2"_s.arg(kOutput.width()).arg(kOutput.height()), u"--outputs"_s,
                    QString::number(outputs), u"--socket"_s, u"wayland-capture"_s, u"--script"_s, script.fileName() });
    m_problem = u"Prism did not start"_s;
    if (!m_prism.waitForStarted())
        return false;
    m_socket = home.filePath(u"runtime/wayland-capture"_s).toUtf8();
    QElapsedTimer clock;
    clock.start();
    while (!QFile::exists(QString::fromUtf8(m_socket))) {
        if (clock.elapsed() > 60000 || m_prism.waitForFinished(20))
            return false;
    }
    return run(uR"({"wait":1})"_s);
}

void TestPointerCapture::initTestCase() { DO(startPrism()); }

// A Prism that stopped has cost the case it stopped in. It is not left to cost the others.
void TestPointerCapture::init()
{
    if (m_prism.state() != QProcess::Running)
        DO(startPrism());
}

void TestPointerCapture::cleanup()
{
    if (m_prism.state() != QProcess::Running)
        return;
    // Whatever a case left: the shell in the state the next one expects.
    QJsonObject now;
    DO(state(&now));
    if (now[u"locked"_s].toBool())
        DO(call(u"tryUnlock"_s, uR"(["any"])"_s));
    if (!now[u"surface"_s].toString().isEmpty())
        DO(call(u"setSurface"_s, uR"([""])"_s));
    DO(run(uR"({"waitFor":{"windows":0,"timeout":20000}})"_s));
}

void TestPointerCapture::cleanupTestCase()
{
    if (m_prism.state() == QProcess::Running)
        DO(stopPrism());
}

// A sanitizer's finding shows here too: Prism exits with something other than 0.
bool TestPointerCapture::stopPrism()
{
    if (!run(uR"({"stop":true})"_s))
        return false;
    if (!m_prism.waitForFinished(30000)) {
        m_problem = u"Prism did not stop"_s;
        return false;
    }
    m_problem = u"Prism ended with %1"_s.arg(m_prism.exitCode());
    return m_prism.exitStatus() == QProcess::NormalExit && m_prism.exitCode() == 0;
}

bool TestPointerCapture::run(const QString &json)
{
    const int index = m_nextStep++;
    const QDir home(m_home->path());
    QSaveFile step(home.filePath(QString::number(index) + u".json"_s));
    if (!step.open(QIODevice::WriteOnly)) {
        m_problem = u"cannot write a step"_s;
        return false;
    }
    step.write(json.toUtf8());
    if (!step.commit()) {
        m_problem = u"cannot write a step"_s;
        return false;
    }
    if (json.contains(u"\"stop\""_s))
        return true;
    const QString done = home.filePath(QString::number(index) + u".done"_s);
    QElapsedTimer clock;
    clock.start();
    while (!QFile::exists(done)) {
        for (Client *client : std::as_const(m_clients))
            client->dispatch();
        if (m_prism.waitForFinished(2) || m_prism.state() == QProcess::NotRunning) {
            m_problem = u"Prism stopped at %1, exit code %2"_s.arg(json).arg(m_prism.exitCode());
            return false;
        }
        if (clock.elapsed() > 60000) {
            m_problem = u"Prism did not do %1 within a minute"_s.arg(json);
            return false;
        }
    }
    for (Client *client : std::as_const(m_clients))
        client->dispatch();
    return true;
}

void TestPointerCapture::pump(int milliseconds)
{
    QElapsedTimer clock;
    clock.start();
    while (clock.elapsed() < milliseconds) {
        for (Client *client : std::as_const(m_clients))
            client->dispatch();
        QTest::qSleep(5);
    }
}

bool TestPointerCapture::state(QJsonObject *out)
{
    const QString name = u"state-%1.json"_s.arg(m_nextState++);
    if (!run(u"{\"state\":\"%1\"}"_s.arg(name)))
        return false;
    QFile file(QDir(m_home->path()).filePath(name));
    if (!file.open(QIODevice::ReadOnly)) {
        m_problem = u"no state file"_s;
        return false;
    }
    *out = QJsonDocument::fromJson(file.readAll()).object();
    file.close();
    file.remove();
    return true;
}

bool TestPointerCapture::windowState(const QString &title, QJsonObject *out)
{
    QJsonObject now;
    if (!state(&now))
        return false;
    const QJsonArray windows = now[u"windows"_s].toArray();
    for (const QJsonValue &window : windows) {
        if (window[u"title"_s].toString() == title) {
            *out = window.toObject();
            return true;
        }
    }
    m_problem = u"no window called %1"_s.arg(title);
    return false;
}

// Its springs have stopped: the same place twice, a little apart.
bool TestPointerCapture::settled(Game *game)
{
    QElapsedTimer clock;
    clock.start();
    QRect last;
    while (clock.elapsed() < 30000) {
        QJsonObject window;
        if (!windowState(game->title, &window) && m_prism.state() == QProcess::NotRunning)
            return false;
        const QJsonObject item = window[u"surfaceItem"_s].toObject();
        const QRect area = rectOf(item[u"onScreen"_s]);
        if (!window[u"awaitingSize"_s].toBool() && item[u"surface"_s].toObject()[u"hasContent"_s].toBool()
            && window[u"visible"_s].toBool() && area == last && !area.isEmpty()) {
            game->area = area;
            return true;
        }
        last = area;
        pump(150);
    }
    m_problem = u"%1 did not come to rest"_s.arg(game->title);
    return false;
}

std::unique_ptr<Client> TestPointerCapture::connectClient()
{
    auto client = std::make_unique<Client>(m_socket);
    if (!client->isUsable())
        m_problem = u"a client could not connect"_s;
    return client;
}

std::unique_ptr<Game> TestPointerCapture::open(const QString &title, QSize size)
{
    auto game = std::make_unique<Game>(&m_clients);
    game->title = title;
    game->client = connectClient();
    if (!game->client->isUsable())
        return nullptr;
    m_clients.append(game->client.get());
    game->window = game->client->openWindow(title, size);
    if (!run(u"{\"waitFor\":{\"window\":{\"title\":\"%1\"},\"timeout\":20000}}"_s.arg(title)) || !settled(game.get()))
        return nullptr;
    return game;
}

Capture *TestPointerCapture::lockedBy(Game *game, uint32_t lifetime)
{
    Capture *capture = game->client->lock(game->window->surface, nullptr, lifetime);
    if (!click(game->centre()) || !game->client->roundtrip()) {
        if (m_problem.isEmpty())
            m_problem = u"the client died"_s;
        return nullptr;
    }
    if (capture->started != 1) {
        m_problem = u"the lock did not start on a click: %1 starts"_s.arg(capture->started);
        return nullptr;
    }
    return capture;
}

// ── The cases ────────────────────────────────────────────────────────────────

void TestPointerCapture::offersBothGlobals()
{
    std::unique_ptr<Client> client = connectClient();
    DO(client->isUsable());
    QVERIFY(client->constraints);
    QVERIFY(client->relativeManager);
}

void TestPointerCapture::offersAllProtocolGlobals()
{
    std::unique_ptr<Client> client = connectClient();
    DO(client->isUsable());
    QCOMPARE(client->globals.value("wp_fractional_scale_manager_v1"), 1U);
    QCOMPARE(client->globals.value("wp_cursor_shape_manager_v1"), 2U);
    QCOMPARE(client->globals.value("xdg_activation_v1"), 1U);
    QCOMPARE(client->globals.value("zwp_relative_pointer_manager_v1"), 1U);
    QCOMPARE(client->globals.value("zwp_pointer_constraints_v1"), 1U);
    for (const char *name : { "wp_fractional_scale_manager_v1", "wp_cursor_shape_manager_v1", "xdg_activation_v1",
                              "zwp_relative_pointer_manager_v1", "zwp_pointer_constraints_v1" })
        QCOMPARE(client->globalAnnouncements.value(name), 1);
}

void TestPointerCapture::protocolManagersCanBeReleasedAndRebound()
{
    std::unique_ptr<Client> client = connectClient();
    DO(client->isUsable());
    QVERIFY(client->relativeManager);
    QVERIFY(client->constraints);
    // The normal client binds these eagerly. Release them first so the loop
    // also proves that destroying the last binding leaves each global usable.
    client->dropped(client->relativeManager);
    zwp_relative_pointer_manager_v1_destroy(client->relativeManager);
    client->relativeManager = nullptr;
    client->dropped(client->constraints);
    zwp_pointer_constraints_v1_destroy(client->constraints);
    client->constraints = nullptr;
    QVERIFY(client->roundtrip());
    const auto rebind = [&](const wl_interface *type, uint32_t version, auto destroy) {
        for (int attempt = 0; attempt < 2; ++attempt) {
            wl_proxy *manager = client->bindGlobal(type, version);
            if (!manager || !client->roundtrip())
                return false;
            // The protocol destructor releases the proxy locally too. Do not
            // let the client's fallback cleanup destroy it a second time.
            client->dropped(manager);
            destroy(manager);
            if (!client->roundtrip())
                return false;
        }
        return true;
    };
    QVERIFY(rebind(&wp_fractional_scale_manager_v1_interface, 1, [](wl_proxy *manager) {
        wp_fractional_scale_manager_v1_destroy(reinterpret_cast<wp_fractional_scale_manager_v1 *>(manager));
    }));
    // Both cursor-shape versions are supported; neither binding may remove
    // the global or break a later binding when its manager is released.
    for (uint32_t version : { 1U, 2U }) {
        QVERIFY(rebind(&wp_cursor_shape_manager_v1_interface, version, [](wl_proxy *manager) {
            wp_cursor_shape_manager_v1_destroy(reinterpret_cast<wp_cursor_shape_manager_v1 *>(manager));
        }));
    }
    QVERIFY(rebind(&xdg_activation_v1_interface, 1, [](wl_proxy *manager) {
        xdg_activation_v1_destroy(reinterpret_cast<xdg_activation_v1 *>(manager));
    }));
    QVERIFY(rebind(&zwp_relative_pointer_manager_v1_interface, 1, [](wl_proxy *manager) {
        zwp_relative_pointer_manager_v1_destroy(reinterpret_cast<zwp_relative_pointer_manager_v1 *>(manager));
    }));
    QVERIFY(rebind(&zwp_pointer_constraints_v1_interface, 1, [](wl_proxy *manager) {
        zwp_pointer_constraints_v1_destroy(reinterpret_cast<zwp_pointer_constraints_v1 *>(manager));
    }));
}

void TestPointerCapture::aClientSizeWaitsForTheLatestConfigureAcknowledgement()
{
    const QSize original(640, 480);
    std::unique_ptr<Game> game = open(u"Delayed acknowledgement"_s, original);
    DO(game);
    Client &client = *game->client;
    Window &window = *game->window;
    QVERIFY(client.roundtrip());
    window.deferConfigures = true;

    // Focus changes send real configure events without imposing a maximized
    // size. The client may legally keep drawing at its old normal geometry.
    std::unique_ptr<Game> other = open(u"Other focus"_s, QSize(320, 240));
    DO(other);
    QJsonObject state;
    DO(windowState(game->title, &state));
    QVERIFY(state[u"focused"_s].toBool());
    DO(windowState(other->title, &state));
    QVERIFY(!state[u"focused"_s].toBool());
    // A different client cannot steal focus merely by opening a window. Use
    // the same activation path as the Shelf, as a person choosing it would.
    DO(run(uR"({"restore":{"title":"Other focus"}})"_s));
    DO(windowState(other->title, &state));
    QVERIFY(state[u"focused"_s].toBool());
    QTRY_VERIFY_WITH_TIMEOUT((client.roundtrip() && !window.deferredConfigures.isEmpty()), 5000);
    QCOMPARE(window.deferredConfigures.last().size, original);
    DO(windowState(game->title, &state));
    QVERIFY(!state[u"focused"_s].toBool());
    const uint32_t older = window.deferredConfigures.last().serial;

    // Click an exposed part of the first client, avoiding the other window's
    // entire frame, including its title bar. No production test hook changes focus.
    DO(windowState(other->title, &state));
    const QRect otherFrame = rectOf(state[u"onScreen"_s]);
    const QRect inset = game->area.adjusted(8, 8, -8, -8);
    bool clicked = false;
    for (const QPoint point : { inset.topLeft(), inset.topRight(), inset.bottomLeft(), inset.bottomRight() }) {
        if (!otherFrame.contains(point)) {
            DO(click(point));
            clicked = true;
            break;
        }
    }
    QVERIFY(clicked);
    QTRY_VERIFY_WITH_TIMEOUT((client.roundtrip() && window.deferredConfigures.last().serial != older
                              && window.deferredConfigures.last().size == original),
                             5000);
    const uint32_t latest = window.deferredConfigures.last().serial;
    DO(settled(game.get()));
    DO(windowState(game->title, &state));
    QVERIFY(state[u"focused"_s].toBool());
    QVERIFY(!state[u"maximized"_s].toBool());
    QCOMPARE(state[u"configuredSize"_s].toArray(), (QJsonArray { 640, 480 }));
    const QSize originalFrame = rectOf(state[u"frameRect"_s]).size();
    const QSize decoration = originalFrame - original;

    // Geometry is within the existing buffer. A commit before any acknowledgement
    // must not replace the shell's configured size with this older-sized content.
    xdg_surface_set_window_geometry(window.xdg, 0, 0, 520, 360);
    window.draw();
    QVERIFY(client.roundtrip());
    DO(settled(game.get()));
    DO(windowState(game->title, &state));
    QCOMPARE(rectOf(state[u"clientGeometry"_s]).size(), QSize(520, 360));
    QCOMPARE(state[u"configuredSize"_s].toArray(), (QJsonArray { 640, 480 }));
    QCOMPARE(rectOf(state[u"frameRect"_s]).size(), originalFrame);
    QCOMPARE(window.deferredConfigures.last().serial, latest);

    // Answering only an older configure still leaves the latest one pending.
    xdg_surface_ack_configure(window.xdg, older);
    xdg_surface_set_window_geometry(window.xdg, 0, 0, 500, 340);
    window.draw();
    QVERIFY(client.roundtrip());
    DO(settled(game.get()));
    DO(windowState(game->title, &state));
    QCOMPARE(rectOf(state[u"clientGeometry"_s]).size(), QSize(500, 340));
    QCOMPARE(state[u"configuredSize"_s].toArray(), (QJsonArray { 640, 480 }));
    QCOMPARE(rectOf(state[u"frameRect"_s]).size(), originalFrame);
    QCOMPARE(window.deferredConfigures.last().serial, latest);

    // After the latest acknowledgement, redraw the SAME geometry. Adoption must
    // happen on commit even though there is no new geometry-change notification.
    xdg_surface_ack_configure(window.xdg, latest);
    window.draw();
    QVERIFY(client.roundtrip());
    DO(settled(game.get()));
    DO(windowState(game->title, &state));
    QCOMPARE(rectOf(state[u"clientGeometry"_s]).size(), QSize(500, 340));
    QCOMPARE(state[u"configuredSize"_s].toArray(), (QJsonArray { 500, 340 }));
    QCOMPARE(rectOf(state[u"frameRect"_s]).size(), QSize(500, 340) + decoration);
    QVERIFY(client.isAlive());
}

void TestPointerCapture::aWindowOpeningAsAnotherGoesIsNotLost()
{
    // Not about the pointer, but this suite is what found it: a window on its way out is
    // listed until its spring settles, with its client's objects gone, and counting a new
    // client's windows through it threw, and lost the new window.
    {
        std::unique_ptr<Game> going = open(u"Going"_s);
        DO(going);
    }
    std::unique_ptr<Game> coming = open(u"Coming"_s);
    DO(coming);
}

void TestPointerCapture::aLockWaitsForAPerson()
{
    std::unique_ptr<Game> game = open(u"Waits"_s);
    DO(game);
    Client &client = *game->client;
    const QPoint p = game->centre();
    DO(moveTo(p));
    QVERIFY(client.roundtrip());
    // It has the keyboard and the pointer; the pointer reports where it is.
    QCOMPARE(client.seen.keyboardEnters, 1);
    QCOMPARE(client.seen.pointerEnters, 1);
    QVERIFY(!client.seen.motions.isEmpty());

    client.listenToMotion();
    Capture *lock = client.lock(game->window->surface);
    QVERIFY(client.roundtrip());
    QCOMPARE(lock->started, 0);
    // Moving over it, or turning the wheel, is not acting on it.
    DO(moveTo(p + QPoint(5, 5)));
    DO(wheel(p + QPoint(5, 5)));
    QVERIFY(client.roundtrip());
    QCOMPARE(lock->started, 0);
    const qsizetype motions = client.seen.motions.size();
    QVERIFY(motions > 0);

    // A click is.
    DO(click(p + QPoint(5, 5)));
    QVERIFY(client.roundtrip());
    QCOMPARE(lock->started, 1);
    QCOMPARE(client.seen.buttonsDown, QList<uint32_t> { BTN_LEFT });
    QCOMPARE(client.seen.buttonsUp, QList<uint32_t> { BTN_LEFT });
    QJsonObject now;
    DO(state(&now));
    QVERIFY(now[u"pointerCaptured"_s].toBool());
    // A person is told how to get out.
    QVERIFY(now[u"toast"_s].toString().contains(u"Super+Esc"_s));

    // Held: no position, only motion.
    const qsizetype heard = client.seen.relative.size();
    const qsizetype motionsWhenLocked = client.seen.motions.size();
    DO(moveTo(p + QPoint(15, 12)));
    QVERIFY(client.roundtrip());
    QCOMPARE(client.seen.motions.size(), motionsWhenLocked);
    QCOMPARE(client.seen.relative.size(), heard + 1);
    QCOMPARE(client.seen.relative.last().delta, QPointF(10, 7));
    // Nested there is nothing to tell the two apart by (WindowMotionSource).
    QCOMPARE(client.seen.relative.last().unaccelerated, QPointF(10, 7));
    QVERIFY(client.seen.relative.last().microseconds > 0);

    // Buttons and the wheel still reach it.
    const int axes = client.seen.axes;
    DO(click(p + QPoint(15, 12)));
    DO(wheel(p + QPoint(15, 12)));
    QVERIFY(client.roundtrip());
    QCOMPARE(client.seen.buttonsDown.size(), 2);
    QCOMPARE(client.seen.buttonsUp.size(), 2);
    QCOMPARE(client.seen.axes, axes + 1);
    QCOMPARE(client.seen.motions.size(), motionsWhenLocked);
    QCOMPARE(lock->ended, 0);

    // Let go: the position again.
    lock->destroy();
    QVERIFY(client.roundtrip());
    DO(state(&now));
    QVERIFY(!now[u"pointerCaptured"_s].toBool());
    DO(moveTo(p + QPoint(30, 30)));
    QVERIFY(client.roundtrip());
    QVERIFY(client.seen.motions.size() > motionsWhenLocked);
    QCOMPARE(client.isAlive(), true);
}

void TestPointerCapture::aKeyIsAnActionAndAModifierOrARepeatIsNot()
{
    std::unique_ptr<Game> game = open(u"Keys"_s);
    DO(game);
    Client &client = *game->client;
    DO(moveTo(game->centre()));
    Capture *lock = client.lock(game->window->surface);
    QVERIFY(client.roundtrip());

    // A modifier alone reaches the client and is not an action. Here it is on its way to
    // one of Prism's shortcuts, which the client never hears.
    DO(key(u"Ctrl+Up"_s));
    QVERIFY(client.roundtrip());
    QVERIFY(client.seen.keysDown.contains(KEY_LEFTCTRL));
    QVERIFY(!client.seen.keysDown.contains(KEY_UP));
    QCOMPARE(lock->started, 0);
    DO(key(u"Esc"_s));
    DO(settled(game.get()));
    DO(moveTo(game->centre()));
    QVERIFY(client.roundtrip());
    QCOMPARE(lock->started, 0);

    // A key held down is one action, and not a new one each time it repeats.
    DO(run(uR"({"keyRepeat":"a"})"_s));
    QVERIFY(client.roundtrip());
    QCOMPARE(lock->started, 0);

    // A key pressed is an action.
    DO(key(u"a"_s));
    QVERIFY(client.roundtrip());
    QVERIFY(client.seen.keysDown.contains(KEY_A));
    QCOMPARE(lock->started, 1);
}

void TestPointerCapture::anOldActionDoesNotCount()
{
    std::unique_ptr<Game> game = open(u"Old"_s);
    DO(game);
    Client &client = *game->client;
    DO(click(game->centre()));
    pump(5300);
    Capture *late = client.lock(game->window->surface);
    QVERIFY(client.roundtrip());
    DO(moveTo(game->centre() + QPoint(3, 3)));
    QVERIFY(client.roundtrip());
    QCOMPARE(late->started, 0);
    late->destroy();

    // Soon after, it does: a game asks just after the click that means "play".
    DO(click(game->centre()));
    Capture *soon = client.lock(game->window->surface);
    QVERIFY(client.roundtrip());
    QCOMPARE(soon->started, 1);
}

void TestPointerCapture::aWindowAppearingUnderThePointerDoesNotCapture()
{
    // The pointer waits where the window will open.
    const QPoint waiting(kOutput.width() / 2, kOutput.height() / 2);
    DO(moveTo(waiting));

    auto game = std::make_unique<Game>(&m_clients);
    game->title = u"Under"_s;
    game->client = connectClient();
    DO(game->client->isUsable());
    Client &client = *game->client;
    m_clients.append(&client);
    game->window = client.openWindow(game->title, QSize(640, 480));
    // It asks at once, before it is even on the screen.
    Capture *lock = client.lock(game->window->surface);
    client.listenToMotion();
    DO(run(uR"({"waitFor":{"window":{"title":"Under"},"timeout":20000}})"_s));
    DO(settled(game.get()));
    QVERIFY(game->area.contains(waiting));
    DO(moveTo(waiting + QPoint(4, 4)));
    pump(600);
    QVERIFY(client.roundtrip());
    // It got the keyboard, as a first window does, and the pointer is on it. No more.
    QCOMPARE(client.seen.keyboardEnters, 1);
    QVERIFY(client.seen.pointerEnters >= 1);
    QCOMPARE(lock->started, 0);

    // A person's click does it, as anywhere.
    DO(click(waiting + QPoint(4, 4)));
    QVERIFY(client.roundtrip());
    QCOMPARE(lock->started, 1);
}

void TestPointerCapture::aSurfaceWithNothingOnItTakesNothing()
{
    // A window that takes its picture away just after a person's click on it.
    std::unique_ptr<Game> game = open(u"Blank"_s);
    DO(game);
    Client &client = *game->client;
    const QPoint p = game->centre();
    DO(click(p));
    wl_surface_attach(game->window->surface, nullptr, 0, 0);
    wl_surface_commit(game->window->surface);
    QVERIFY(client.roundtrip());
    pump(300);
    Capture *lock = client.lock(game->window->surface);
    QVERIFY(client.roundtrip());
    DO(moveTo(p + QPoint(2, 2)));
    QVERIFY(client.roundtrip());
    QCOMPARE(lock->started, 0);
    QVERIFY(client.isAlive());
}

void TestPointerCapture::onlyTheSurfaceWithTheKeyboardMayLock()
{
    // A small window with the keyboard, over a big one without it.
    std::unique_ptr<Game> front = open(u"Front"_s, QSize(320, 240));
    DO(front);
    DO(click(front->centre()));
    std::unique_ptr<Game> back = open(u"Back"_s, QSize(1200, 700));
    DO(back);
    QJsonObject window;
    DO(windowState(back->title, &window));
    QVERIFY(!window[u"focused"_s].toBool());
    QVERIFY(window[u"demandsAttention"_s].toBool());

    // A point on the big one that the small one does not cover.
    const QPoint onBack = back->at(60, 60);
    QVERIFY(!front->area.contains(onBack));

    Capture *backLock = back->client->lock(back->window->surface);
    back->client->listenToMotion();
    QVERIFY(back->client->roundtrip());
    // The pointer is on it, and it has no keyboard: it hears wl_pointer and nothing else.
    DO(moveTo(onBack));
    DO(moveTo(onBack + QPoint(10, 10)));
    QVERIFY(back->client->roundtrip());
    QCOMPARE(back->client->seen.pointerEnters, 1);
    // Over its content its controls light up, though it is not in front.
    DO(windowState(back->title, &window));
    QVERIFY(window[u"pointerOver"_s].toBool());
    DO(windowState(front->title, &window));
    QVERIFY(!window[u"pointerOver"_s].toBool());
    QVERIFY(!back->client->seen.motions.isEmpty());
    QCOMPARE(backLock->started, 0);
    QVERIFY(back->client->seen.relative.isEmpty());
    // A key goes to the window that has the keyboard, and is no action on this one.
    DO(key(u"b"_s));
    QVERIFY(back->client->roundtrip());
    QVERIFY(front->client->roundtrip());
    QVERIFY(front->client->seen.keysDown.contains(KEY_B));
    QVERIFY(!back->client->seen.keysDown.contains(KEY_B));
    QCOMPARE(backLock->started, 0);

    // The one with the keyboard, its pointer elsewhere, does not get it either.
    Capture *frontLock = front->client->lock(front->window->surface);
    QVERIFY(front->client->roundtrip());
    DO(key(u"c"_s));
    QVERIFY(front->client->roundtrip());
    QCOMPARE(frontLock->started, 0);

    // A click moves the keyboard to the big one, and it is a person acting on it.
    DO(click(onBack + QPoint(10, 10)));
    QVERIFY(back->client->roundtrip());
    QVERIFY(front->client->roundtrip());
    QCOMPARE(backLock->started, 1);
    QCOMPARE(frontLock->started, 0);
    QVERIFY(back->client->seen.keyboardEnters >= 1);
    QVERIFY(front->client->seen.keyboardLeaves >= 1);
}

void TestPointerCapture::onlyTheSurfaceWithTheKeyboardHearsMotion()
{
    std::unique_ptr<Game> game = open(u"Hears"_s);
    DO(game);
    Client &client = *game->client;
    client.listenToMotion();
    QVERIFY(client.roundtrip());
    const QPoint p = game->centre();
    DO(settleOn(p));
    qsizetype from = client.seen.relative.size();

    // On its window, with the keyboard, and no lock needed.
    DO(moveTo(p + QPoint(20, -10)));
    QVERIFY(client.roundtrip());
    QCOMPARE(client.seen.heardSince(from), QPointF(20, -10));

    // Off its window nothing is heard, the motion that crossed its edge included: that
    // would say how far off the pointer went. Nor the one coming back.
    const QPoint away(40, kOutput.height() - 200);
    QVERIFY(!game->area.contains(away));
    from = client.seen.relative.size();
    DO(moveTo(away));
    DO(moveTo(away + QPoint(30, 30)));
    DO(moveTo(p));
    QVERIFY(client.roundtrip());
    QCOMPARE(client.seen.relative.size(), from);
    // And back on it again, it hears once more.
    DO(moveTo(p + QPoint(1, 2)));
    QVERIFY(client.roundtrip());
    QCOMPARE(client.seen.heardSince(from), QPointF(1, 2));

    // With a shell surface over the window, nothing.
    DO(key(u"Ctrl+Space"_s));
    from = client.seen.relative.size();
    DO(moveTo(p + QPoint(9, 9)));
    DO(moveTo(p + QPoint(19, 19)));
    QVERIFY(client.roundtrip());
    QCOMPARE(client.seen.relative.size(), from);
    DO(key(u"Esc"_s));
    DO(settled(game.get()));

    // Locked out, nothing.
    DO(key(u"Ctrl+Alt+L"_s));
    from = client.seen.relative.size();
    DO(moveTo(p + QPoint(4, 4)));
    DO(moveTo(p + QPoint(8, 8)));
    QVERIFY(client.roundtrip());
    QCOMPARE(client.seen.relative.size(), from);
    DO(call(u"tryUnlock"_s, uR"(["any"])"_s));

    // A window without the keyboard, with the pointer on it, hears nothing, and neither
    // does the one with the keyboard, with the pointer off it.
    std::unique_ptr<Game> other = open(u"Other"_s, QSize(1200, 700));
    DO(other);
    other->client->listenToMotion();
    QVERIFY(other->client->roundtrip());
    const QPoint onOther = other->at(40, 40);
    QVERIFY(!game->area.contains(onOther));
    DO(moveTo(onOther));
    from = client.seen.relative.size();
    DO(moveTo(onOther + QPoint(7, 7)));
    QVERIFY(client.roundtrip());
    QVERIFY(other->client->roundtrip());
    QVERIFY(!other->client->seen.motions.isEmpty());
    QVERIFY(other->client->seen.relative.isEmpty());
    QCOMPARE(client.seen.relative.size(), from);
}

void TestPointerCapture::losingTheKeyboardEndsTheLock()
{
    std::unique_ptr<Game> game = open(u"Loses"_s);
    DO(game);
    Client &client = *game->client;
    client.listenToMotion();
    Capture *lock = lockedBy(game.get());
    DO(lock);

    // Prism's own window comes up by its shortcut, which works while the pointer is held.
    DO(key(u"Ctrl+,"_s));
    DO(run(uR"({"waitFor":{"window":{"appId":"settings"},"timeout":20000}})"_s));
    QVERIFY(client.roundtrip());
    QCOMPARE(lock->ended, 1);
    const qsizetype heard = client.seen.relative.size();
    DO(moveTo(game->centre() + QPoint(6, 6)));
    QVERIFY(client.roundtrip());
    QCOMPARE(client.seen.relative.size(), heard);
    DO(run(uR"({"close":"settings"})"_s));
    DO(run(uR"({"waitFor":{"gone":{"appId":"settings"},"timeout":20000}})"_s));
    DO(settled(game.get()));

    // Back in front: it does not come back by itself.
    DO(moveTo(game->centre()));
    QVERIFY(client.roundtrip());
    QCOMPARE(lock->started, 1);
    DO(click(game->centre()));
    QVERIFY(client.roundtrip());
    QCOMPARE(lock->started, 2);
    QCOMPARE(lock->ended, 1);

    // Another client's window taking the keyboard ends it as well: here by the way the
    // Shelf brings a window forward, since the pointer cannot click anything.
    std::unique_ptr<Game> other = open(u"Taker"_s, QSize(320, 240));
    DO(other);
    QVERIFY(client.roundtrip());
    QCOMPARE(lock->ended, 1); // It only asks for attention, and takes nothing.
    DO(run(uR"({"restore":{"title":"Taker"}})"_s));
    QVERIFY(client.roundtrip());
    QVERIFY(other->client->roundtrip());
    QCOMPARE(lock->ended, 2);
    QVERIFY(client.seen.keyboardLeaves >= 1);
    QCOMPARE(lock->started, 2);
}

void TestPointerCapture::theLockScreenEndsItAndItStaysEnded()
{
    std::unique_ptr<Game> game = open(u"Screen"_s);
    DO(game);
    Client &client = *game->client;
    client.listenToMotion();
    Capture *lock = lockedBy(game.get());
    DO(lock);

    // Its shortcut works while the pointer is held.
    DO(key(u"Ctrl+Alt+L"_s));
    QVERIFY(client.roundtrip());
    QCOMPARE(lock->ended, 1);
    QVERIFY(client.seen.keyboardLeaves >= 1);
    QVERIFY(client.seen.pointerLeaves >= 1);
    QJsonObject now;
    DO(state(&now));
    QVERIFY(now[u"locked"_s].toBool());
    QVERIFY(!now[u"pointerCaptured"_s].toBool());

    // On the lock screen a client hears nothing and takes nothing, whatever a person does.
    const qsizetype heard = client.seen.relative.size();
    const qsizetype motions = client.seen.motions.size();
    DO(moveTo(game->centre() + QPoint(4, 4)));
    DO(click(game->centre() + QPoint(4, 4)));
    DO(key(u"x"_s));
    QVERIFY(client.roundtrip());
    QCOMPARE(lock->started, 1);
    QCOMPARE(client.seen.relative.size(), heard);
    QCOMPARE(client.seen.motions.size(), motions);
    QVERIFY(!client.seen.keysDown.contains(KEY_X));

    // Unlocked, nothing comes back by itself, though it is a persistent lock on the window
    // in front.
    DO(call(u"tryUnlock"_s, uR"(["any"])"_s));
    DO(settled(game.get()));
    DO(moveTo(game->centre()));
    QVERIFY(client.roundtrip());
    QCOMPARE(lock->started, 1);
    DO(state(&now));
    QVERIFY(!now[u"locked"_s].toBool());
    QVERIFY(!now[u"pointerCaptured"_s].toBool());

    // A person acting on it again does.
    DO(click(game->centre()));
    QVERIFY(client.roundtrip());
    QCOMPARE(lock->started, 2);
}

void TestPointerCapture::aShellSurfaceEndsIt()
{
    std::unique_ptr<Game> game = open(u"Covered"_s);
    DO(game);
    Client &client = *game->client;
    Capture *lock = lockedBy(game.get());
    DO(lock);

    // Each brought up by a key the client cannot hear, or by the shell's own means.
    struct Way
    {
        QString name;
        std::function<bool()> open;
        std::function<bool()> close;
    };
    const QList<Way> ways {
        { u"Overview"_s, [this] { return key(u"F3"_s); }, [this] { return key(u"Esc"_s); } },
        { u"Beam"_s, [this] { return key(u"Ctrl+Space"_s); }, [this] { return key(u"Esc"_s); } },
        { u"Home"_s, [this] { return call(u"openHome"_s, uR"(["home"])"_s); }, [this] { return key(u"Esc"_s); } },
        { u"editing widgets"_s, [this] { return call(u"editWidgets"_s, uR"([true])"_s); },
          [this] { return key(u"Esc"_s); } },
        { u"a system gesture"_s,
          [this] {
              return run(uR"({"gesture":{"begin":{"fingers":3}}})"_s)
                     && run(uR"({"gesture":{"move":[0,-30],"steps":3}})"_s);
          },
          // The fingers stop before they lift, or the flick would open Overview.
          [this] { return run(uR"({"gesture":{"end":true,"after":300}})"_s); } },
    };
    int rounds = 0;
    for (const Way &way : ways) {
        ++rounds;
        DO(way.open());
        QVERIFY(client.roundtrip());
        QVERIFY2(lock->ended == rounds, qPrintable(way.name));
        DO(way.close());
        DO(settled(game.get()));
        DO(moveTo(game->centre()));
        QVERIFY(client.roundtrip());
        // Closed again, it does not come back by itself.
        QVERIFY2(lock->started == rounds, qPrintable(way.name));
        DO(click(game->centre()));
        QVERIFY(client.roundtrip());
        QVERIFY2(lock->started == rounds + 1, qPrintable(way.name));
    }
    QVERIFY(!client.seen.keysDown.contains(KEY_F3));
    QVERIFY(!client.seen.keysDown.contains(KEY_SPACE));
}

void TestPointerCapture::theHostTakingTheFocusEndsIt()
{
    // Nested, the person going to another of the host's windows has left, keys and all.
    std::unique_ptr<Game> game = open(u"Host"_s);
    DO(game);
    Client &client = *game->client;
    Capture *lock = lockedBy(game.get());
    DO(lock);
    DO(run(uR"({"hostFocus":false})"_s));
    QVERIFY(client.roundtrip());
    QCOMPARE(lock->ended, 1);
    DO(run(uR"({"hostFocus":true})"_s));
    QVERIFY(client.roundtrip());
    QCOMPARE(lock->started, 1);
    DO(click(game->centre()));
    QVERIFY(client.roundtrip());
    QCOMPARE(lock->started, 2);
    lock->destroy();
    QVERIFY(client.roundtrip());

    // Away, nothing starts, whatever arrives.
    DO(run(uR"({"hostFocus":false})"_s));
    Capture *away = client.lock(game->window->surface);
    QVERIFY(client.roundtrip());
    DO(click(game->centre()));
    QVERIFY(client.roundtrip());
    QCOMPARE(away->started, 0);
    away->destroy();
    QVERIFY(client.roundtrip());
    DO(run(uR"({"hostFocus":true})"_s));

    // What the person did before they went stands for nothing when they come back.
    DO(click(game->centre()));
    DO(run(uR"({"hostFocus":false})"_s));
    DO(run(uR"({"hostFocus":true})"_s));
    Capture *back = client.lock(game->window->surface);
    QVERIFY(client.roundtrip());
    DO(moveTo(game->centre() + QPoint(2, 2)));
    QVERIFY(client.roundtrip());
    QCOMPARE(back->started, 0);
    DO(click(game->centre()));
    QVERIFY(client.roundtrip());
    QCOMPARE(back->started, 1);
}

void TestPointerCapture::aPointerThatLeftWhileHeldIsGoneAfter()
{
    // Nested, the host's pointer can leave Prism's window while a client holds it.
    std::unique_ptr<Game> game = open(u"Left"_s);
    DO(game);
    Client &client = *game->client;
    Capture *lock = lockedBy(game.get());
    DO(lock);
    const Seen said = client.seen;
    DO(run(uR"({"hostPointer":"left"})"_s));
    QVERIFY(client.roundtrip());
    // The capture goes on, and says nothing of it.
    QCOMPARE(lock->ended, 0);
    QCOMPARE(client.seen.pointerLeaves, said.pointerLeaves);

    // Let go out there: the pointer is gone from the client, and it is told so.
    DO(key(u"Meta+Esc"_s));
    QVERIFY(client.roundtrip());
    QCOMPARE(lock->ended, 1);
    QVERIFY(client.seen.pointerLeaves > said.pointerLeaves);
    // So a lock it asks for now waits for the pointer to come back, whatever key comes.
    lock->destroy();
    Capture *fresh = client.lock(game->window->surface);
    QVERIFY(client.roundtrip());
    DO(key(u"a"_s));
    QVERIFY(client.roundtrip());
    QVERIFY(client.seen.keysDown.contains(KEY_A));
    QCOMPARE(fresh->started, 0);

    // Back, and a click on it: it does.
    DO(run(u"{\"hostPointer\":[%1,%2]}"_s.arg(game->centre().x()).arg(game->centre().y())));
    DO(moveTo(game->centre() + QPoint(1, 1)));
    DO(click(game->centre()));
    QVERIFY(client.roundtrip());
    QCOMPARE(fresh->started, 1);
}

void TestPointerCapture::anActionBeforeTheShellCameUpIsForgotten()
{
    // A click with nothing asked for yet, then the shell in front for a moment, all well
    // within the time an action stands for: what the client asks for after waits.
    std::unique_ptr<Game> game = open(u"Before"_s);
    DO(game);
    Client &client = *game->client;
    const QPoint p = game->centre();

    // Beam, opened and closed.
    DO(click(p));
    DO(key(u"Ctrl+Space"_s));
    DO(key(u"Esc"_s));
    DO(settled(game.get()));
    Capture *lock = client.lock(game->window->surface);
    QVERIFY(client.roundtrip());
    DO(moveTo(p + QPoint(3, 3)));
    QVERIFY(client.roundtrip());
    QCOMPARE(lock->started, 0);
    lock->destroy();
    QVERIFY(client.roundtrip());

    // A system gesture begun and given up: the keyboard never leaves the window.
    DO(click(p));
    DO(run(uR"({"gesture":{"begin":{"fingers":3}}})"_s));
    DO(run(uR"({"gesture":{"move":[0,-30],"steps":3}})"_s));
    DO(run(uR"({"gesture":{"end":true,"after":300}})"_s));
    DO(settled(game.get()));
    lock = client.lock(game->window->surface);
    QVERIFY(client.roundtrip());
    DO(moveTo(p + QPoint(5, 5)));
    QVERIFY(client.roundtrip());
    QCOMPARE(lock->started, 0);
    QCOMPARE(client.seen.keyboardLeaves, 0);

    // A new action does it.
    DO(click(p));
    QVERIFY(client.roundtrip());
    QCOMPARE(lock->started, 1);
}

void TestPointerCapture::theWayOutCannotBeHeardOrHeldOnTo()
{
    std::unique_ptr<Game> game = open(u"Out"_s);
    DO(game);
    Client &client = *game->client;
    client.listenToMotion();
    Capture *lock = lockedBy(game.get());
    DO(lock);

    DO(key(u"Meta+Esc"_s));
    QVERIFY(client.roundtrip());
    QCOMPARE(lock->ended, 1);
    QVERIFY(!client.seen.keysDown.contains(KEY_ESC));
    QJsonObject now;
    DO(state(&now));
    QVERIFY(!now[u"pointerCaptured"_s].toBool());
    // Nothing else moved: no shell surface came up, and the window has its keyboard.
    QCOMPARE(now[u"surface"_s].toString(), QString());
    QCOMPARE(client.seen.keyboardLeaves, 0);

    // The position comes back to it as the person moves, and the lock stays off.
    const qsizetype motions = client.seen.motions.size();
    DO(moveTo(game->centre() + QPoint(11, 11)));
    QVERIFY(client.roundtrip());
    QVERIFY(client.seen.motions.size() > motions);
    QCOMPARE(lock->started, 1);

    // With nothing captured the same keys are the client's, as any others are: Esc is not
    // taken from it for nothing. But a key pressed with Super held is no action, so the way
    // out pressed twice, or bouncing, does not put the person back in.
    DO(key(u"Meta+Esc"_s));
    QVERIFY(client.roundtrip());
    QVERIFY(client.seen.keysDown.contains(KEY_ESC));
    QCOMPARE(lock->started, 1);
    // Nor does any other key: a persistent lock comes back only for a button pressed on it.
    DO(key(u"a"_s));
    QVERIFY(client.roundtrip());
    QVERIFY(client.seen.keysDown.contains(KEY_A));
    QCOMPARE(lock->started, 1);
    DO(click(game->centre()));
    QVERIFY(client.roundtrip());
    QCOMPARE(lock->started, 2);

    // The way out works whatever else is held: Shift to sprint, Ctrl to crouch, or all of them.
    int ended = 1;
    for (const QString &held : { u"Shift+"_s, u"Ctrl+"_s, u"Alt+"_s, u"Ctrl+Alt+Shift+"_s }) {
        DO(key(held + u"Meta+Esc"_s));
        QVERIFY(client.roundtrip());
        ++ended;
        QVERIFY2(lock->ended == ended, qPrintable(held));
        DO(click(game->centre()));
        QVERIFY(client.roundtrip());
        QVERIFY2(lock->started == ended + 1, qPrintable(held));
    }
    QCOMPARE(client.seen.keysDown.count(KEY_ESC), 1);

    // A lock that has never held the pointer starts on a key, but not on one pressed with
    // Super held: that is on its way to the shell, or to the host.
    DO(key(u"Meta+Esc"_s));
    lock->destroy();
    Capture *fresh = client.lock(game->window->surface);
    QVERIFY(client.roundtrip());
    DO(key(u"Meta+Esc"_s));
    QVERIFY(client.roundtrip());
    QCOMPARE(client.seen.keysDown.count(KEY_ESC), 2);
    QCOMPARE(fresh->started, 0);
    DO(key(u"b"_s));
    QVERIFY(client.roundtrip());
    QCOMPARE(fresh->started, 1);
}

void TestPointerCapture::guideOnAControllerIsAWayOutToo()
{
    std::unique_ptr<Game> game = open(u"Pad"_s);
    DO(game);
    Client &client = *game->client;
    Capture *lock = lockedBy(game.get());
    DO(lock);

    // A controller connected, and its question answered: the shell's, not the client's.
    DO(run(uR"({"gamepad":{"connect":"xbox"}})"_s));
    DO(run(uR"({"gamepad":{"tap":"south"}})"_s));
    QJsonObject now;
    DO(state(&now));
    QVERIFY(now[u"settings"_s].toObject()[u"controllerInterface"_s].toBool());
    QVERIFY(client.roundtrip());
    QCOMPARE(lock->ended, 0);

    DO(run(uR"({"gamepad":{"tap":"guide"}})"_s));
    QVERIFY(client.roundtrip());
    QCOMPARE(lock->ended, 1);
    DO(state(&now));
    QCOMPARE(now[u"surface"_s].toString(), u"home"_s);
    QVERIFY(!now[u"pointerCaptured"_s].toBool());

    DO(call(u"setSurface"_s, uR"([""])"_s));
    DO(run(uR"({"gamepad":{"disconnect":true}})"_s));
    QVERIFY(client.roundtrip());
    QCOMPARE(lock->started, 1);
}

void TestPointerCapture::whileHeldNothingReachesTheShellOrSaysWhereThePointerIs()
{
    std::unique_ptr<Game> game = open(u"Silent"_s);
    DO(game);
    Client &client = *game->client;
    client.listenToMotion();
    QVERIFY(client.roundtrip());
    DO(settleOn(game->centre()));
    Capture *lock = lockedBy(game.get());
    DO(lock);
    QJsonObject before;
    DO(state(&before));
    const int windows = before[u"windows"_s].toArray().size();

    const Seen said = client.seen;
    const qsizetype heard = client.seen.relative.size();

    // Over the Shelf, off the window, where a host's pointer may well wander.
    DO(run(uR"({"move":{"item":"shelf"}})"_s));
    QVERIFY(client.roundtrip());
    // The whole motion is heard: it is relative motion's point that the edge does not stop it.
    QCOMPARE(client.seen.relative.size(), heard + 1);
    QCOMPARE(client.seen.motions.size(), said.motions.size());
    QCOMPARE(client.seen.pointerEnters, said.pointerEnters);
    QCOMPARE(client.seen.pointerLeaves, said.pointerLeaves);

    // A click out there is the client's, and nothing of the shell's is pressed.
    DO(run(uR"({"click":{"item":"shelf"}})"_s));
    DO(wheel(QPoint(40, kOutput.height() - 200)));
    pump(300);
    QVERIFY(client.roundtrip());
    QCOMPARE(client.seen.buttonsDown.size(), said.buttonsDown.size() + 1);
    QCOMPARE(client.seen.buttonsUp.size(), said.buttonsUp.size() + 1);
    QCOMPARE(client.seen.axes, said.axes + 1);
    QJsonObject after;
    DO(state(&after));
    QCOMPARE(after[u"windows"_s].toArray().size(), windows);
    QCOMPARE(after[u"surface"_s].toString(), before[u"surface"_s].toString());
    QVERIFY(after[u"pointerCaptured"_s].toBool());

    // Beyond the window's edge and back: still nothing but motion, and still held.
    DO(moveTo(QPoint(0, 0)));
    DO(moveTo(game->centre()));
    QVERIFY(client.roundtrip());
    QCOMPARE(client.seen.motions.size(), said.motions.size());
    QCOMPARE(client.seen.pointerLeaves, said.pointerLeaves);
    QCOMPARE(lock->ended, 0);

    // The shell's keys keep working underneath: Volume is one that leaves the window be.
    DO(key(u"Volume Up"_s));
    QVERIFY(client.roundtrip());
    QCOMPARE(lock->ended, 0);
    QVERIFY(client.isAlive());

    // Once let go the shell has the pointer again.
    DO(key(u"Meta+Esc"_s));
    DO(run(uR"({"move":{"item":"shelf"}})"_s));
    QVERIFY(client.roundtrip());
    QVERIFY(client.seen.pointerLeaves > said.pointerLeaves);
}

void TestPointerCapture::aNotificationUnderAHeldPointerChangesNothing()
{
    // A window as wide as there is, the pointer where notifications come up.
    std::unique_ptr<Game> game = open(u"Notified"_s, QSize(1500, 800));
    DO(game);
    Client &client = *game->client;
    const QPoint corner(kOutput.width() - 180, 110);
    QVERIFY(game->area.contains(corner));
    // Held away from the column the simulated session posts its own banners in, then taken
    // there: held, the pointer is the client's wherever it goes.
    client.listenToMotion();
    QVERIFY(client.roundtrip());
    DO(settleOn(game->centre()));
    Capture *lock = client.lock(game->window->surface);
    DO(click(game->centre()));
    QVERIFY(client.roundtrip());
    QCOMPARE(lock->started, 1);
    DO(moveTo(corner));
    QVERIFY(client.roundtrip());
    const Seen said = client.seen;

    DO(call(u"runAction"_s, uR"(["notify.test",{}])"_s));
    pump(800);
    QVERIFY(client.roundtrip());
    QCOMPARE(lock->ended, 0);
    QCOMPARE(client.seen.pointerLeaves, said.pointerLeaves);
    QCOMPARE(client.seen.motions.size(), said.motions.size());

    // Held, a motion over the card is the client's: the person is playing.
    DO(moveTo(corner + QPoint(6, 4)));
    QVERIFY(client.roundtrip());
    QCOMPARE(client.seen.heardSince(said.relative.size()), QPointF(6, 4));
    QCOMPARE(client.seen.motions.size(), said.motions.size());

    // Let go over the card: the client hears nothing more of the pointer there.
    DO(key(u"Meta+Esc"_s));
    QVERIFY(client.roundtrip());
    QCOMPARE(lock->ended, 1);
    const Seen out = client.seen;
    DO(moveTo(corner + QPoint(12, 8)));
    DO(moveTo(corner + QPoint(18, 12)));
    QVERIFY(client.roundtrip());
    QCOMPARE(client.seen.motions.size(), out.motions.size());
    QCOMPARE(client.seen.relative.size(), out.relative.size());
    QCOMPARE(client.seen.pointerEnters, out.pointerEnters);
}

void TestPointerCapture::theShellOverAWindowKeepsThePointerFromIt()
{
    // A window as wide as there is, under where notifications and the volume come up.
    std::unique_ptr<Game> game = open(u"Overlaid"_s, QSize(1500, 840));
    DO(game);
    Client &client = *game->client;
    client.listenToMotion();
    QVERIFY(client.roundtrip());
    const QPoint p = game->centre();

    // The volume over it takes no pointer of its own: still the client hears nothing
    // there, not even the motion that took the pointer onto it.
    const QPoint hud(kOutput.width() / 2, kOutput.height() - 112 - 25);
    const QPoint offHud = hud - QPoint(0, 200);
    QVERIFY2(
        game->area.contains(hud),
        qPrintable(
            u"%1,%2 %3x%4"_s.arg(game->area.x()).arg(game->area.y()).arg(game->area.width()).arg(game->area.height())));
    DO(settleOn(offHud));
    DO(key(u"Volume Up"_s));
    Seen said = client.seen;
    DO(moveTo(hud));
    DO(moveTo(hud + QPoint(10, 4)));
    QVERIFY(client.roundtrip());
    QCOMPARE(client.seen.motions.size(), said.motions.size());
    QCOMPARE(client.seen.relative.size(), said.relative.size());
    QVERIFY(client.seen.pointerLeaves > said.pointerLeaves);
    // Off it, on the window again, the pointer is the client's.
    DO(moveTo(offHud));
    QVERIFY(client.roundtrip());
    const qsizetype back = client.seen.relative.size();
    DO(moveTo(offHud + QPoint(3, 2)));
    QVERIFY(client.roundtrip());
    QVERIFY(client.seen.motions.size() > said.motions.size());
    QCOMPARE(client.seen.heardSince(back), QPointF(3, 2));
    // A button pressed on the client keeps the pointer the client's until let go, over the
    // volume as anywhere: a drag in a game's own window is the game's.
    DO(key(u"Volume Up"_s));
    DO(press(offHud));
    said = client.seen;
    DO(moveTo(hud));
    QVERIFY(client.roundtrip());
    QCOMPARE(client.seen.motions.size(), said.motions.size() + 1);
    DO(release(hud));
    DO(moveTo(hud + QPoint(6, 2)));
    QVERIFY(client.roundtrip());
    QCOMPARE(client.seen.motions.size(), said.motions.size() + 1);
    QCOMPARE(client.seen.buttonsUp.size(), said.buttonsUp.size() + 1);
    DO(moveTo(offHud));

    // A notification over it, the same.
    const QPoint card(kOutput.width() - 180, 110);
    const QPoint offCard(card.x(), game->area.bottom() - 260);
    QVERIFY(game->area.contains(card));
    DO(settleOn(offCard));
    DO(call(u"runAction"_s, uR"(["notify.test",{}])"_s));
    pump(800);
    QVERIFY(client.roundtrip());
    said = client.seen;
    DO(moveTo(card));
    DO(moveTo(card + QPoint(8, 6)));
    QVERIFY(client.roundtrip());
    QCOMPARE(client.seen.motions.size(), said.motions.size());
    QCOMPARE(client.seen.relative.size(), said.relative.size());
    QCOMPARE(client.seen.pointerEnters, said.pointerEnters);
    DO(moveTo(offCard));
    QVERIFY(client.roundtrip());
    QVERIFY(client.seen.motions.size() > said.motions.size());

    // Nothing a person does over Home or Beam reaches the client underneath, whether it held
    // the pointer before or not: no enter, no position, no motion.
    struct Way
    {
        QString name;
        std::function<bool()> open;
    };
    const QList<Way> ways {
        { u"Home"_s, [this] { return call(u"openHome"_s, uR"(["home"])"_s); } },
        { u"Beam"_s, [this] { return key(u"Ctrl+Space"_s); } },
        // Its panel is at the side, and nothing of it lies over the window's middle.
        { u"Control Center"_s, [this] { return call(u"setSurface"_s, uR"(["control-center"])"_s); } },
    };
    for (const bool captured : { false, true }) {
        for (const Way &way : ways) {
            const QByteArray name = (way.name + (captured ? u", captured"_s : QString())).toUtf8();
            DO(settleOn(p));
            Capture *lock = captured ? lockedBy(game.get()) : nullptr;
            if (captured)
                DO(lock);
            const int leaves = client.seen.pointerLeaves;
            DO(way.open());
            QVERIFY(client.roundtrip());
            QVERIFY2(!lock || lock->ended == 1, name.constData());
            QVERIFY2(client.seen.pointerLeaves > leaves, name.constData());
            said = client.seen;
            for (int i = 1; i <= 4; ++i)
                DO(moveTo(p + QPoint(i * 30, i * 20)));
            QVERIFY(client.roundtrip());
            QVERIFY2(client.seen.motions.size() == said.motions.size(), name.constData());
            QVERIFY2(client.seen.pointerEnters == said.pointerEnters, name.constData());
            QVERIFY2(client.seen.relative.size() == said.relative.size(), name.constData());
            QVERIFY2(!lock || lock->started == 1, name.constData());
            QJsonObject now;
            DO(state(&now));
            if (!now[u"surface"_s].toString().isEmpty())
                DO(key(u"Esc"_s));
            DO(settled(game.get()));
            if (lock)
                lock->destroy();
            // Closed, the pointer is the client's again.
            DO(moveTo(p + QPoint(3, 3)));
            QVERIFY(client.roundtrip());
            QVERIFY2(client.seen.motions.size() > said.motions.size(), name.constData());
        }
    }
}

void TestPointerCapture::aButtonPressedBeforeTheLockIsReleasedToTheClient()
{
    std::unique_ptr<Game> game = open(u"Held"_s);
    DO(game);
    Client &client = *game->client;
    client.listenToMotion();
    Capture *lock = client.lock(game->window->surface);
    QVERIFY(client.roundtrip());
    const QPoint p = game->centre();
    DO(settleOn(p));
    const qsizetype motions = client.seen.motions.size();

    // Hold a button to look around: the press itself starts the lock.
    DO(press(p));
    QVERIFY(client.roundtrip());
    QCOMPARE(lock->started, 1);
    QCOMPARE(client.seen.buttonsDown.size(), 1);
    const qsizetype motionsHeld = client.seen.motions.size();
    QVERIFY(motionsHeld >= motions);
    const qsizetype heard = client.seen.relative.size();
    DO(moveTo(p + QPoint(40, 30)));
    QVERIFY(client.roundtrip());
    QCOMPARE(client.seen.heardSince(heard), QPointF(40, 30));
    QCOMPARE(client.seen.motions.size(), motionsHeld);

    DO(release(p + QPoint(40, 30)));
    QVERIFY(client.roundtrip());
    QCOMPARE(client.seen.buttonsUp.size(), 1);
    QCOMPARE(client.seen.motions.size(), motionsHeld);
    QCOMPARE(lock->ended, 0);
}

void TestPointerCapture::aButtonHeldWhenItEndsIsReleased()
{
    std::unique_ptr<Game> game = open(u"Stuck"_s);
    DO(game);
    Client &client = *game->client;
    Capture *lock = lockedBy(game.get());
    DO(lock);
    QCOMPARE(client.seen.buttonsDown.size(), 1);
    QCOMPARE(client.seen.buttonsUp.size(), 1);

    // Pressed while held, so Qt never saw it, and let go of only after the capture ends.
    DO(press(game->centre()));
    QVERIFY(client.roundtrip());
    QCOMPARE(client.seen.buttonsDown.size(), 2);
    QCOMPARE(client.seen.buttonsUp.size(), 1);
    DO(key(u"Meta+Esc"_s));
    QVERIFY(client.roundtrip());
    QCOMPARE(lock->ended, 1);
    // The client is not left believing it is still down, and is not told twice.
    QCOMPARE(client.seen.buttonsUp.size(), 2);
    DO(release(game->centre()));
    QVERIFY(client.roundtrip());
    QCOMPARE(client.seen.buttonsUp.size(), 2);
}

void TestPointerCapture::aOneshotEndsForGoodAndAPersistentComesBack()
{
    std::unique_ptr<Game> game = open(u"Lifetimes"_s);
    DO(game);
    Client &client = *game->client;
    Capture *oneshot = lockedBy(game.get(), ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_ONESHOT);
    DO(oneshot);
    DO(key(u"Meta+Esc"_s));
    QVERIFY(client.roundtrip());
    QCOMPARE(oneshot->ended, 1);
    DO(click(game->centre()));
    DO(key(u"a"_s));
    QVERIFY(client.roundtrip());
    QCOMPARE(oneshot->started, 1);
    QCOMPARE(oneshot->ended, 1);

    // Spent, it no longer stands in the way of another on the same surface.
    Capture *persistent = lockedBy(game.get());
    DO(persistent);
    QVERIFY(client.roundtrip());
    DO(key(u"Meta+Esc"_s));
    DO(click(game->centre()));
    QVERIFY(client.roundtrip());
    QCOMPARE(persistent->ended, 1);
    QCOMPARE(persistent->started, 2);
    QCOMPARE(oneshot->started, 1);
    QVERIFY(client.isAlive());
}

void TestPointerCapture::aSecondConstraintOnASurfaceIsAnError()
{
    enum class Second { Lock, Confine, LockWithAnotherPointer };
    for (const Second second : { Second::Lock, Second::Confine, Second::LockWithAnotherPointer }) {
        std::unique_ptr<Game> game = open(u"Twice"_s);
        DO(game);
        Client &client = *game->client;
        client.lock(game->window->surface);
        QVERIFY(client.roundtrip());
        if (second == Second::Lock)
            client.lock(game->window->surface);
        else if (second == Second::Confine)
            client.confine(game->window->surface);
        else
            client.lock(game->window->surface, nullptr, ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT,
                        client.anotherPointer());
        QVERIFY(!client.roundtrip());
        const wl_interface *interface = nullptr;
        uint32_t object = 0;
        QCOMPARE(wl_display_get_protocol_error(client.display(), &interface, &object),
                 uint32_t(ZWP_POINTER_CONSTRAINTS_V1_ERROR_ALREADY_CONSTRAINED));
        QCOMPARE(QByteArray(interface->name), QByteArray(zwp_pointer_constraints_v1_interface.name));
        game.reset();
        // That client is gone, and Prism is not.
        DO(run(uR"({"waitFor":{"windows":0,"timeout":20000}})"_s));
    }

    // What is allowed: one each on two surfaces, and another once the first has gone.
    std::unique_ptr<Game> game = open(u"Twice"_s);
    DO(game);
    Client &client = *game->client;
    Window *second = client.openWindow(u"Twice too"_s, QSize(200, 200));
    Capture *first = client.lock(game->window->surface);
    client.lock(second->surface);
    QVERIFY(client.roundtrip());
    first->destroy();
    client.confine(game->window->surface);
    QVERIFY(client.roundtrip());
    QVERIFY(client.isAlive());
}

void TestPointerCapture::anUnknownLifetimeIsAnError()
{
    for (const bool lock : { true, false }) {
        std::unique_ptr<Game> game = open(u"Lifetime"_s);
        DO(game);
        Client &client = *game->client;
        if (lock)
            client.lock(game->window->surface, nullptr, 99);
        else
            client.confine(game->window->surface, nullptr, 0);
        QVERIFY(!client.roundtrip());
        QVERIFY(!client.isAlive());
        game.reset();
        DO(run(uR"({"waitFor":{"windows":0,"timeout":20000}})"_s));
    }
}

void TestPointerCapture::regionsSayWhereThePointerMustBe()
{
    std::unique_ptr<Game> game = open(u"Region"_s);
    DO(game);
    Client &client = *game->client;
    const QRect allowed(100, 100, 50, 50);
    Capture *lock = client.lock(game->window->surface, client.region({ allowed }));
    QVERIFY(client.roundtrip());

    // Acting on the window outside its region does not start it.
    DO(click(game->at(400, 300)));
    QVERIFY(client.roundtrip());
    QCOMPARE(lock->started, 0);
    // Soon after, the pointer coming into the region does.
    DO(moveTo(game->at(120, 120)));
    QVERIFY(client.roundtrip());
    QCOMPARE(lock->started, 1);
    DO(key(u"Meta+Esc"_s));
    QVERIFY(client.roundtrip());
    QCOMPARE(lock->ended, 1);

    // A new region takes effect on the next commit, not before.
    lock->setRegion(client.region({ QRect(300, 300, 50, 50) }));
    QVERIFY(client.roundtrip());
    DO(click(game->at(320, 320)));
    QVERIFY(client.roundtrip());
    QCOMPARE(lock->started, 1);
    wl_surface_commit(game->window->surface);
    QVERIFY(client.roundtrip());
    pump(100);
    QVERIFY(client.roundtrip());
    QCOMPARE(lock->started, 2);
    DO(key(u"Meta+Esc"_s));

    // Null: the whole surface again.
    lock->setRegion(nullptr);
    wl_surface_commit(game->window->surface);
    QVERIFY(client.roundtrip());
    DO(click(game->at(500, 400)));
    QVERIFY(client.roundtrip());
    QCOMPARE(lock->started, 3);
}

void TestPointerCapture::badRegionsDoNoHarm()
{
    std::unique_ptr<Game> game = open(u"Bad regions"_s);
    DO(game);
    Client &client = *game->client;
    // Backwards, at the limits of an int, and so broken up it would cost a walk on every
    // motion. None is an error of the protocol's, and none lets a lock start.
    wl_region *backwards = client.region({});
    wl_region_add(backwards, 300, 300, -50, -50);
    wl_region_add(backwards, 0, 0, 0, 0);
    wl_region *atTheLimits = client.region({});
    wl_region_add(atTheLimits, INT_MAX, INT_MAX, INT_MAX, INT_MAX);
    wl_region_add(atTheLimits, INT_MIN, INT_MIN, 1, 1);
    wl_region_subtract(atTheLimits, INT_MIN, INT_MIN, INT_MAX, INT_MAX);
    wl_region *comb = client.region({});
    for (int i = 0; i < 2500; ++i)
        wl_region_add(comb, i * 2, 0, 1, 1000);
    QVERIFY(client.roundtrip());
    for (wl_region *region : { backwards, atTheLimits, comb }) {
        Capture *lock = client.lock(game->window->surface, region);
        QVERIFY(client.roundtrip());
        DO(click(game->centre()));
        DO(moveTo(game->centre() + QPoint(1, 1)));
        QVERIFY(client.roundtrip());
        QCOMPARE(lock->started, 0);
        // The same by set_region, on a lock that had a good one.
        lock->destroy();
        Capture *reset = client.lock(game->window->surface, client.region({ QRect(600, 460, 10, 10) }));
        reset->setRegion(region);
        wl_surface_commit(game->window->surface);
        QVERIFY(client.roundtrip());
        DO(click(game->centre()));
        QVERIFY(client.roundtrip());
        QCOMPARE(reset->started, 0);
        reset->destroy();
    }
    // Two regions, each well within bounds, whose meeting is not: forty rows of input, and a
    // lock on forty columns, meet in 1600 rectangles. That counts as empty too.
    wl_region *rows = client.region({});
    for (int i = 0; i < 40; ++i)
        wl_region_add(rows, 0, i * 12, 640, 6);
    wl_region *columns = client.region({});
    for (int i = 0; i < 40; ++i)
        wl_region_add(columns, i * 16, 0, 8, 480);
    wl_surface_set_input_region(game->window->surface, rows);
    Capture *grid = client.lock(game->window->surface, columns);
    wl_surface_commit(game->window->surface);
    QVERIFY(client.roundtrip());
    pump(100);
    const QPoint cell = game->at(16 * 20 + 2, 12 * 20 + 2);
    DO(moveTo(cell));
    DO(click(cell));
    QVERIFY(client.roundtrip());
    QCOMPARE(grid->started, 0);
    grid->destroy();
    wl_surface_set_input_region(game->window->surface, nullptr);
    wl_surface_commit(game->window->surface);
    QVERIFY(client.roundtrip());
    pump(100);

    // A rectangle far larger than the surface is only as large as the surface, and a hint
    // nowhere near it is nothing to worry about.
    wl_region *huge = client.region({});
    wl_region_add(huge, -1000000, -1000000, INT_MAX, INT_MAX);
    Capture *lock = client.lock(game->window->surface, huge);
    zwp_locked_pointer_v1_set_cursor_position_hint(lock->locked, wl_fixed_from_int(8000000),
                                                   wl_fixed_from_int(-8000000));
    wl_surface_commit(game->window->surface);
    QVERIFY(client.roundtrip());
    DO(click(game->centre()));
    QVERIFY(client.roundtrip());
    QCOMPARE(lock->started, 1);
    DO(key(u"Meta+Esc"_s));
    QVERIFY(client.roundtrip());
    QCOMPARE(lock->ended, 1);
    QVERIFY(client.isAlive());
}

void TestPointerCapture::aDestroyedSurfaceEndsTheLock()
{
    std::unique_ptr<Game> game = open(u"Destroyed"_s);
    DO(game);
    Client &client = *game->client;
    client.listenToMotion();
    Capture *lock = lockedBy(game.get());
    DO(lock);

    game->window->destroy();
    QVERIFY(client.roundtrip());
    pump(100);
    QVERIFY(client.roundtrip());
    QCOMPARE(lock->ended, 1);
    QJsonObject now;
    DO(state(&now));
    QVERIFY(!now[u"pointerCaptured"_s].toBool());
    DO(run(uR"({"waitFor":{"windows":0,"timeout":20000}})"_s));

    // The object is defunct, and may still be spoken to.
    lock->setRegion(client.region({ QRect(0, 0, 10, 10) }));
    zwp_locked_pointer_v1_set_cursor_position_hint(lock->locked, wl_fixed_from_int(1), wl_fixed_from_int(1));
    QVERIFY(client.roundtrip());
    const qsizetype heard = client.seen.relative.size();
    DO(moveTo(game->centre() + QPoint(3, 3)));
    DO(click(game->centre() + QPoint(3, 3)));
    QVERIFY(client.roundtrip());
    QCOMPARE(client.seen.relative.size(), heard);
    lock->destroy();
    QVERIFY(client.roundtrip());
    QVERIFY(client.isAlive());
}

void TestPointerCapture::aDestroyedPointerEndsTheLock()
{
    std::unique_ptr<Game> game = open(u"No pointer"_s);
    DO(game);
    Client &client = *game->client;
    wl_pointer *second = client.anotherPointer();
    client.listenToMotion(second);
    client.listenToMotion();
    QVERIFY(client.roundtrip());
    DO(settleOn(game->centre()));
    const qsizetype heardBefore = client.seen.relative.size();
    Capture *lock = lockedBy(game.get());
    DO(lock);
    DO(moveTo(game->centre() + QPoint(2, 2)));
    QVERIFY(client.roundtrip());
    // Two relative pointers, each heard.
    QCOMPARE(client.seen.relative.size(), heardBefore + 2);

    client.dropped(client.pointer);
    wl_pointer_release(client.pointer);
    client.pointer = nullptr;
    QVERIFY(client.roundtrip());
    QCOMPARE(lock->ended, 1);

    // Inert now: it does not come back, whatever a person does.
    DO(click(game->centre()));
    DO(key(u"a"_s));
    QVERIFY(client.roundtrip());
    QCOMPARE(lock->started, 1);
    // The other wl_pointer still has its relative one; the first's hears no more.
    const qsizetype heard = client.seen.relative.size();
    DO(moveTo(game->centre() + QPoint(9, 9)));
    QVERIFY(client.roundtrip());
    QCOMPARE(client.seen.relative.size(), heard + 1);
    QVERIFY(client.isAlive());
}

void TestPointerCapture::aClientVanishingEndsTheLock()
{
    std::unique_ptr<Game> game = open(u"Vanishes"_s);
    DO(game);
    Capture *lock = lockedBy(game.get());
    DO(lock);
    DO(press(game->centre())); // With a button down that Qt never saw.
    game->client->vanish();
    DO(run(uR"({"waitFor":{"windows":0,"timeout":20000}})"_s));
    QJsonObject now;
    DO(state(&now));
    QVERIFY(!now[u"pointerCaptured"_s].toBool());
    DO(release(game->centre()));
    game.reset();

    // Prism goes on, and the pointer is for whoever is next.
    std::unique_ptr<Game> next = open(u"Next"_s);
    DO(next);
    DO(moveTo(next->centre()));
    DO(moveTo(next->centre() + QPoint(5, 5)));
    QVERIFY(next->client->roundtrip());
    QVERIFY(!next->client->seen.motions.isEmpty());
    Capture *again = lockedBy(next.get());
    DO(again);
}

void TestPointerCapture::requestsOnAnInertObjectAreIgnored()
{
    std::unique_ptr<Game> game = open(u"Inert"_s);
    DO(game);
    Client &client = *game->client;
    Capture *oneshot = lockedBy(game.get(), ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_ONESHOT);
    DO(oneshot);
    DO(key(u"Meta+Esc"_s));
    QVERIFY(client.roundtrip());
    QCOMPARE(oneshot->ended, 1);

    oneshot->setRegion(client.region({ QRect(0, 0, 640, 480) }));
    oneshot->setRegion(nullptr);
    zwp_locked_pointer_v1_set_cursor_position_hint(oneshot->locked, wl_fixed_from_int(4), wl_fixed_from_int(4));
    wl_surface_commit(game->window->surface);
    QVERIFY(client.roundtrip());
    DO(click(game->centre()));
    QVERIFY(client.roundtrip());
    QCOMPARE(oneshot->started, 1);
    QCOMPARE(oneshot->ended, 1);
    oneshot->destroy();
    QVERIFY(client.roundtrip());

    // Not yet started, then spent by its surface going: inert before it ever ran.
    Window *second = client.openWindow(u"Inert too"_s, QSize(200, 200));
    Capture *neverRan = client.confine(second->surface);
    QVERIFY(client.roundtrip());
    second->destroy();
    QVERIFY(client.roundtrip());
    neverRan->setRegion(nullptr);
    QVERIFY(client.roundtrip());
    QCOMPARE(neverRan->started, 0);
    QCOMPARE(neverRan->ended, 0);
    QVERIFY(client.isAlive());
}

void TestPointerCapture::floodsAreRefused()
{
    // Made and destroyed in a loop: nothing piles up, and it is not refused.
    {
        std::unique_ptr<Game> game = open(u"Churn"_s);
        DO(game);
        Client &client = *game->client;
        for (int i = 0; i < 20000; ++i) {
            zwp_relative_pointer_v1 *relative =
                zwp_relative_pointer_manager_v1_get_relative_pointer(client.relativeManager, client.pointer);
            zwp_relative_pointer_v1_destroy(relative);
            zwp_locked_pointer_v1 *locked =
                zwp_pointer_constraints_v1_lock_pointer(client.constraints, game->window->surface, client.pointer,
                                                        nullptr, ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT);
            zwp_locked_pointer_v1_destroy(locked);
            if (i % 500 == 0)
                QVERIFY(client.roundtrip());
        }
        QVERIFY(client.roundtrip());
        // And it all still works.
        Capture *lock = lockedBy(game.get());
        DO(lock);
    }
    DO(run(uR"({"waitFor":{"windows":0,"timeout":20000}})"_s));

    // Kept: a limit, past which the client is ended, and nobody else is touched.
    std::unique_ptr<Game> bystander = open(u"Bystander"_s);
    DO(bystander);
    DO(click(bystander->centre()));
    Capture *lock = bystander->client->lock(bystander->window->surface);
    QVERIFY(bystander->client->roundtrip());
    QCOMPARE(lock->started, 1);

    {
        std::unique_ptr<Client> flooder = connectClient();
        DO(flooder->isUsable());
        bool refused = false;
        for (int i = 0; i < kManyObjects && !refused; ++i) {
            flooder->listenToMotion();
            refused = !flooder->roundtrip();
        }
        QVERIFY(refused);
        const wl_interface *interface = nullptr;
        uint32_t object = 0;
        QCOMPARE(wl_display_get_protocol_error(flooder->display(), &interface, &object),
                 uint32_t(WL_DISPLAY_ERROR_IMPLEMENTATION));
    }
    {
        // Constraints too, each on a surface of its own, and inert ones count.
        std::unique_ptr<Client> flooder = connectClient();
        DO(flooder->isUsable());
        bool refused = false;
        for (int i = 0; i < kManyObjects && !refused; ++i) {
            wl_surface *surface = flooder->keep(wl_compositor_create_surface(flooder->compositor));
            flooder->lock(surface);
            flooder->dropped(surface);
            wl_surface_destroy(surface);
            refused = !flooder->roundtrip();
        }
        QVERIFY(refused);
    }
    QVERIFY(bystander->client->roundtrip());
    QCOMPARE(lock->ended, 0);
    DO(moveTo(bystander->centre() + QPoint(1, 1)));
    QVERIFY(bystander->client->roundtrip());
    QVERIFY(bystander->client->isAlive());
}

void TestPointerCapture::subsurfaces()
{
    // A game drawn in a subsurface, as gamescope draws its planes.
    std::unique_ptr<Game> game = open(u"Layers"_s);
    DO(game);
    Client &client = *game->client;
    QVERIFY(client.subcompositor);
    wl_surface *child = client.keep(wl_compositor_create_surface(client.compositor));
    wl_subsurface *subsurface =
        client.keep(wl_subcompositor_get_subsurface(client.subcompositor, child, game->window->surface));
    wl_subsurface_set_position(subsurface, 100, 100);
    wl_subsurface_set_desync(subsurface);
    wl_buffer *buffer = game->window->makeBuffer(QSize(200, 200));
    wl_surface_attach(child, buffer, 0, 0);
    wl_surface_damage(child, 0, 0, 200, 200);
    wl_surface_commit(child);
    wl_surface_commit(game->window->surface);
    QVERIFY(client.roundtrip());
    pump(300);

    client.listenToMotion();
    // A lock on the window, with the pointer on the subsurface, and one on the subsurface.
    Capture *lock = client.lock(child);
    QVERIFY(client.roundtrip());
    const QPoint onChild = game->at(200, 200);
    DO(settleOn(onChild));
    DO(click(onChild));
    QVERIFY(client.roundtrip());
    QCOMPARE(lock->started, 1);
    const qsizetype heard = client.seen.relative.size();
    DO(moveTo(onChild + QPoint(3, 4)));
    QVERIFY(client.roundtrip());
    QCOMPARE(client.seen.heardSince(heard), QPointF(3, 4));
    DO(key(u"Meta+Esc"_s));
    QVERIFY(client.roundtrip());
    QCOMPARE(lock->ended, 1);
    lock->destroy();

    // On the window, with the pointer on the subsurface: the protocol promises the locked
    // surface has the pointer's focus, so it waits for the pointer to be on it.
    Capture *onParent = client.lock(game->window->surface);
    QVERIFY(client.roundtrip());
    DO(click(onChild));
    QVERIFY(client.roundtrip());
    QCOMPARE(onParent->started, 0);
    DO(click(game->at(40, 40)));
    QVERIFY(client.roundtrip());
    QCOMPARE(onParent->started, 1);
}

void TestPointerCapture::aHostileSubsurfaceTreeDoesNoHarm()
{
    std::unique_ptr<Game> game = open(u"Tree"_s);
    DO(game);
    Client &client = *game->client;

    // A child kept on after its parent is destroyed: Qt's link to the parent dangles.
    wl_surface *parent = client.keep(wl_compositor_create_surface(client.compositor));
    wl_surface *orphan = client.keep(wl_compositor_create_surface(client.compositor));
    client.keep(wl_subcompositor_get_subsurface(client.subcompositor, orphan, parent));
    wl_surface_commit(parent);
    QVERIFY(client.roundtrip());
    client.dropped(parent);
    wl_surface_destroy(parent);
    QVERIFY(client.roundtrip());
    Capture *onOrphan = client.lock(orphan);
    QVERIFY(client.roundtrip());
    DO(click(game->centre()));
    QVERIFY(client.roundtrip());
    QCOMPARE(onOrphan->started, 0);

    // A deep chain under the window: no lock from anywhere it should not be.
    wl_surface *above = game->window->surface;
    for (int i = 0; i < 12; ++i) {
        wl_surface *next = client.keep(wl_compositor_create_surface(client.compositor));
        client.keep(wl_subcompositor_get_subsurface(client.subcompositor, next, above));
        above = next;
    }
    Capture *deep = client.lock(above);
    QVERIFY(client.roundtrip());
    DO(click(game->centre()));
    QVERIFY(client.roundtrip());
    // Never drawn, so the pointer never was on it.
    QCOMPARE(deep->started, 0);
    QVERIFY(client.isAlive());

    // A cycle, whatever Prism makes of it: nothing that walks the tree goes on for ever.
    std::unique_ptr<Client> cyclic = connectClient();
    DO(cyclic->isUsable());
    wl_surface *a = cyclic->keep(wl_compositor_create_surface(cyclic->compositor));
    wl_surface *b = cyclic->keep(wl_compositor_create_surface(cyclic->compositor));
    cyclic->keep(wl_subcompositor_get_subsurface(cyclic->subcompositor, a, b));
    cyclic->keep(wl_subcompositor_get_subsurface(cyclic->subcompositor, b, a));
    cyclic->lock(a);
    cyclic->roundtrip();
    DO(click(game->centre()));
    QVERIFY(client.roundtrip());
    cyclic.reset();
    DO(moveTo(game->centre() + QPoint(1, 1)));
    QVERIFY(client.isAlive());

    // Thousands side by side under the window, the pointer over it, the client listening, and
    // locks waiting on the furthest: every motion looks through the tree, for each of them.
    client.listenToMotion();
    QVERIFY(client.roundtrip());
    const auto tenMoves = [&](QPoint from) -> qint64 {
        QElapsedTimer clock;
        clock.start();
        for (int i = 1; i <= 10; ++i) {
            if (!moveTo(from + QPoint(i, i)))
                return -1;
        }
        return clock.elapsed();
    };
    DO(settleOn(game->centre()));
    const qint64 before = tenMoves(game->centre());
    QVERIFY(before > 0);
    QList<wl_surface *> wide;
    for (int i = 0; i < kFanOut; ++i) {
        wl_surface *child = client.keep(wl_compositor_create_surface(client.compositor));
        client.keep(wl_subcompositor_get_subsurface(client.subcompositor, child, game->window->surface));
        wide.append(child);
        if (i % 1000 == 999)
            QVERIFY(client.roundtrip());
    }
    wl_surface_commit(game->window->surface);
    QList<Capture *> far;
    for (int i = 0; i < 20; ++i)
        far.append(client.lock(wide.at(kFanOut - 1 - i)));
    QVERIFY(client.roundtrip());
    DO(click(game->centre()));
    pump(300);
    QVERIFY(client.roundtrip());
    const qint64 after = tenMoves(game->centre());
    QVERIFY(after > 0);
    qInfo("ten motions: %lld ms with the window alone, %lld ms with %d subsurfaces", before, after, kFanOut);
    // A step of the script takes 100 ms; what Prism adds is small beside it.
    QVERIFY2(after < before + 10 * 40, qPrintable(u"%1 ms, against %2"_s.arg(after).arg(before)));
    for (const Capture *capture : std::as_const(far))
        QCOMPARE(capture->started, 0);
    QVERIFY(client.isAlive());
}

void TestPointerCapture::confinementKeepsThePointerInItsRegion()
{
    std::unique_ptr<Game> game = open(u"Confined"_s);
    DO(game);
    Client &client = *game->client;
    client.listenToMotion();
    const QRect allowed(100, 100, 200, 150);
    Capture *confined = client.confine(game->window->surface, client.region({ allowed }));
    QVERIFY(client.roundtrip());

    // As a lock: nothing by itself.
    DO(moveTo(game->at(150, 150)));
    QVERIFY(client.roundtrip());
    QCOMPARE(confined->started, 0);
    DO(click(game->at(150, 150)));
    QVERIFY(client.roundtrip());
    QCOMPARE(confined->started, 1);

    // Inside its region the client sees where the pointer is.
    DO(moveTo(game->at(160, 170)));
    QVERIFY(client.roundtrip());
    QCOMPARE(client.seen.motions.last(), QPointF(160, 170));

    // Beyond it, the client sees it stopped at the edge; the motion is still heard.
    const qsizetype heard = client.seen.relative.size();
    const Seen said = client.seen;
    DO(moveTo(game->at(500, 170)));
    QVERIFY(client.roundtrip());
    const QPointF right = client.seen.motions.last();
    QVERIFY(right.x() >= allowed.left() && right.x() < allowed.left() + allowed.width());
    QVERIFY(right.x() > allowed.left() + allowed.width() - 1);
    QCOMPARE(right.y(), 170.0);
    QCOMPARE(client.seen.relative.size(), heard + 1);

    // Off the surface entirely, over the shell: the same, with no leave, and the shell's
    // items are not pressed.
    QJsonObject before;
    DO(state(&before));
    DO(run(uR"({"move":{"item":"shelf"}})"_s));
    DO(run(uR"({"click":{"item":"shelf"}})"_s));
    QVERIFY(client.roundtrip());
    const QPointF last = client.seen.motions.last();
    QVERIFY(QRectF(allowed).contains(last));
    QCOMPARE(client.seen.pointerLeaves, said.pointerLeaves);
    QCOMPARE(client.seen.buttonsDown.size(), said.buttonsDown.size() + 1);
    QJsonObject after;
    DO(state(&after));
    QCOMPARE(after[u"windows"_s].toArray().size(), before[u"windows"_s].toArray().size());
    QCOMPARE(after[u"surface"_s].toString(), before[u"surface"_s].toString());

    // Let go, it sees where the pointer really is.
    DO(key(u"Meta+Esc"_s));
    QVERIFY(client.roundtrip());
    QCOMPARE(confined->ended, 1);
    DO(moveTo(game->at(500, 400)));
    QVERIFY(client.roundtrip());
    QCOMPARE(client.seen.motions.last(), QPointF(500, 400));
}

void TestPointerCapture::aConfinementWithNowhereLeftEnds()
{
    std::unique_ptr<Game> game = open(u"Nowhere"_s);
    DO(game);
    Client &client = *game->client;
    Capture *confined = client.confine(game->window->surface);
    DO(click(game->centre()));
    QVERIFY(client.roundtrip());
    QCOMPARE(confined->started, 1);

    confined->setRegion(client.region({}));
    QVERIFY(client.roundtrip());
    // Not before the commit.
    QCOMPARE(confined->ended, 0);
    wl_surface_commit(game->window->surface);
    QVERIFY(client.roundtrip());
    pump(100);
    QVERIFY(client.roundtrip());
    QCOMPARE(confined->ended, 1);
    DO(click(game->centre()));
    QVERIFY(client.roundtrip());
    QCOMPARE(confined->started, 1);
    QVERIFY(client.isAlive());
}

static double framesPerSecond(qint64 frames, qint64 milliseconds)
{
    return double(frames) * 1000.0 / double(std::max<qint64>(milliseconds, 1));
}

void TestPointerCapture::nothingIsDrawnWhileNothingMoves()
{
    std::unique_ptr<Game> game = open(u"Quiet"_s);
    DO(game);
    Client &client = *game->client;
    DO(moveTo(game->centre()));
    pump(1500);

    QJsonObject now;
    QElapsedTimer clock;
    const auto frames = [&](qint64 *out) {
        const bool read = state(&now);
        *out = qint64(now[u"frames"_s].toDouble());
        return read;
    };
    // What the shell draws for its own reasons, with no object of these protocols alive.
    qint64 start = 0;
    qint64 stop = 0;
    DO(frames(&start));
    clock.start();
    pump(1500);
    DO(frames(&stop));
    const double before = framesPerSecond(stop - start, clock.elapsed());

    client.listenToMotion();
    QVERIFY(client.roundtrip());
    DO(settleOn(game->centre()));
    Capture *lock = lockedBy(game.get());
    DO(lock);
    pump(4000); // The toast has come and gone.

    // A capture sitting there adds nothing.
    DO(frames(&start));
    clock.restart();
    pump(1500);
    DO(frames(&stop));
    const double held = framesPerSecond(stop - start, clock.elapsed());

    // And a mouse moving under a lock is no reason to draw: only the client hears it.
    const qsizetype heard = client.seen.relative.size();
    DO(frames(&start));
    clock.restart();
    for (int i = 1; i <= 12; ++i)
        DO(moveTo(game->centre() + QPoint(i * 5, i * 3)));
    DO(frames(&stop));
    const double moving = framesPerSecond(stop - start, clock.elapsed());
    QVERIFY(client.roundtrip());
    QCOMPARE(client.seen.relative.size(), heard + 12);

    qInfo("frames a second: %.1f with nothing bound, %.1f held, %.1f moving under the lock", before, held, moving);
    const double allowed = before * 1.25 + 3.0;
    QVERIFY2(held <= allowed, qPrintable(u"held: %1, before: %2"_s.arg(held).arg(before)));
    QVERIFY2(moving <= allowed, qPrintable(u"moving: %1, before: %2"_s.arg(moving).arg(before)));
}

void TestPointerCapture::aHeldPointerGoesNowhereOnAnotherDisplay()
{
    DO(stopPrism());
    DO(startPrism(2));
    {
        std::unique_ptr<Game> game = open(u"Displays"_s);
        DO(game);
        Client &client = *game->client;
        client.listenToMotion();
        QVERIFY(client.roundtrip());
        DO(settleOn(game->centre()));
        Capture *lock = lockedBy(game.get());
        DO(lock);
        const Seen said = client.seen;

        // Across the edge onto the other display: the window filter is there too.
        DO(run(uR"({"move":{"display":1,"at":[200,200]}})"_s));
        DO(run(uR"({"click":{"display":1,"at":[200,200]}})"_s));
        QVERIFY(client.roundtrip());
        QCOMPARE(client.seen.motions.size(), said.motions.size());
        QCOMPARE(client.seen.pointerLeaves, said.pointerLeaves);
        QCOMPARE(client.seen.buttonsDown.size(), said.buttonsDown.size() + 1);
        QCOMPARE(client.seen.buttonsUp.size(), said.buttonsUp.size() + 1);
        QCOMPARE(lock->ended, 0);
        QJsonObject now;
        DO(state(&now));
        QVERIFY(now[u"pointerCaptured"_s].toBool());

        // Motion on the other display is measured there, with no jump from the crossing.
        const qsizetype heard = client.seen.relative.size();
        DO(run(uR"({"move":{"display":1,"at":[230,220]}})"_s));
        QVERIFY(client.roundtrip());
        QCOMPARE(client.seen.heardSince(heard), QPointF(30, 20));
    }
    // The cases to come have one display again.
    DO(run(uR"({"waitFor":{"windows":0,"timeout":20000}})"_s));
    DO(stopPrism());
}

QTEST_GUILESS_MAIN(TestPointerCapture)
#include "tst_pointercapture.moc"
