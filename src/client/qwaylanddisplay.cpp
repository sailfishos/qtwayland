/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qwaylanddisplay_p.h"

#include "qwaylandintegration_p.h"
#include "qwaylandwindow_p.h"
#include "qwaylandscreen_p.h"
#include "qwaylandcursor_p.h"
#include "qwaylandinputdevice_p.h"
#include "qwaylandclipboard_p.h"
#include "qwaylanddatadevicemanager_p.h"
#include "qwaylandhardwareintegration_p.h"
#include "qwaylandxdgshell_p.h"
#include "qwaylandxdgsurface_p.h"
#include "qwaylandwlshellsurface_p.h"

#include "qwaylandwindowmanagerintegration_p.h"
#include "qwaylandshellintegration_p.h"
#include "qwaylandclientbufferintegration_p.h"

#include "qwaylandextendedsurface_p.h"
#include "qwaylandsubsurface_p.h"
#include "qwaylandtouch_p.h"
#include "qwaylandqtkey_p.h"

#include <QtWaylandClient/private/qwayland-text.h>
#include <QtWaylandClient/private/qwayland-xdg-shell.h>

#include <QtCore/QAbstractEventDispatcher>
#include <QtGui/private/qguiapplication_p.h>

#include <QtCore/QDebug>

#include <errno.h>
#include <poll.h>

QT_BEGIN_NAMESPACE

namespace QtWaylandClient {

struct wl_surface *QWaylandDisplay::createSurface(void *handle)
{
    struct wl_surface *surface = mCompositor.create_surface();
    wl_surface_set_user_data(surface, handle);
    return surface;
}

QWaylandShellSurface *QWaylandDisplay::createShellSurface(QWaylandWindow *window)
{
    Q_ASSERT(mWaylandIntegration->shellIntegration());
    return mWaylandIntegration->shellIntegration()->createShellSurface(window);
}

struct ::wl_region *QWaylandDisplay::createRegion(const QRegion &qregion)
{
    struct ::wl_region *region = mCompositor.create_region();

    Q_FOREACH (const QRect &rect, qregion.rects())
        wl_region_add(region, rect.x(), rect.y(), rect.width(), rect.height());

    return region;
}

::wl_subsurface *QWaylandDisplay::createSubSurface(QWaylandWindow *window, QWaylandWindow *parent)
{
    if (!mSubCompositor) {
        return NULL;
    }

    return mSubCompositor->get_subsurface(window->object(), parent->object());
}

QWaylandClientBufferIntegration * QWaylandDisplay::clientBufferIntegration() const
{
    return mWaylandIntegration->clientBufferIntegration();
}

QWaylandWindowManagerIntegration *QWaylandDisplay::windowManagerIntegration() const
{
    return mWindowManagerIntegration.data();
}

QWaylandDisplay::QWaylandDisplay(QWaylandIntegration *waylandIntegration)
    : mWaylandIntegration(waylandIntegration)
#ifndef QT_NO_DRAGANDDROP
    , mDndSelectionHandler(0)
#endif
    , mWindowExtension(0)
    , mSubCompositor(0)
    , mTouchExtension(0)
    , mQtKeyExtension(0)
    , mTextInputManager(0)
    , mHardwareIntegration(0)
    , mLastInputSerial(0)
    , mLastInputDevice(0)
    , mLastInputWindow(0)
    , mLastKeyboardFocus(Q_NULLPTR)
    , mSyncCallback(Q_NULLPTR)
{
    qRegisterMetaType<uint32_t>("uint32_t");

    mDisplay = wl_display_connect(NULL);
    if (mDisplay == NULL) {
        qErrnoWarning(errno, "Failed to create display");
        ::exit(1);
    }

    struct ::wl_registry *registry = wl_display_get_registry(mDisplay);
    init(registry);

    mWindowManagerIntegration.reset(new QWaylandWindowManagerIntegration(this));

    forceRoundTrip();
}

QWaylandDisplay::~QWaylandDisplay(void)
{
    qDeleteAll(mInputDevices);
    mInputDevices.clear();

    foreach (QWaylandScreen *screen, mScreens) {
        mWaylandIntegration->destroyScreen(screen);
    }
    mScreens.clear();
#ifndef QT_NO_DRAGANDDROP
    delete mDndSelectionHandler.take();
#endif
    wl_display_disconnect(mDisplay);
}

void QWaylandDisplay::checkError() const
{
    int ecode = wl_display_get_error(mDisplay);
    if ((ecode == EPIPE || ecode == ECONNRESET)) {
        // special case this to provide a nicer error
        qWarning("The Wayland connection broke. Did the Wayland compositor die?");
    } else {
        qErrnoWarning(ecode, "The Wayland connection experienced a fatal error");
    }
}

void QWaylandDisplay::flushRequests()
{
    if (wl_display_prepare_read(mDisplay) == 0) {
        // Between prepare_read() and read_events() we must flush, then
        // check that there is actually something to read.
        //
        // Flush first: read_events() can end up waiting on a reply to a
        // request that is still sitting in our outgoing buffer, which the
        // compositor cannot answer until it has actually been written.
        // Flushing only after the read (as this used to) is too late.
        //
        // Then poll with a zero timeout, and only commit to the read if
        // the socket is readable. libwayland's own physical recv() is
        // non-blocking (MSG_DONTWAIT), so an unconditional read_events()
        // doesn't wedge in the kernel - but the reader role it's part of
        // is process-wide: if another thread sharing this connection
        // still has an outstanding prepare_read() of its own when we
        // call read_events(), reader_count doesn't reach zero on our
        // decrement, and we instead sleep on reader_cond until whichever
        // thread does bring it to zero finishes - for as long as that
        // takes. A correctly written third party (e.g. GStreamer's gst-gl
        // Wayland event source, when it shares this display) prepares a
        // read and then sits in its own main loop's poll() until the
        // compositor has something to say, only cancelling if it does
        // not. While the compositor is quiet - a paused video surface,
        // or preroll before the first frame - that can be a long time,
        // and every such wait here blocks the whole GUI thread; observed
        // as multi-second application freezes.
        //
        // If the fd is readable we can safely proceed: whoever else
        // holds a slot is polling the same fd and will therefore wake
        // and complete their side too. If it is not readable there is
        // nothing to gain by waiting - back out with cancel_read() (the
        // only way to release the slot without corrupting reader_count)
        // and dispatch whatever is already queued. We are called again
        // from the QSocketNotifier the moment the fd does become
        // readable, so nothing is lost.
        wl_display_flush(mDisplay);

        struct pollfd pfd;
        pfd.fd = wl_display_get_fd(mDisplay);
        pfd.events = POLLIN;
        pfd.revents = 0;

        if (poll(&pfd, 1, 0) > 0) {
            if (wl_display_read_events(mDisplay) < 0) {
                checkError();
                exitWithError();
            }
        } else {
            wl_display_cancel_read(mDisplay);
        }
    }

    if (wl_display_dispatch_pending(mDisplay) < 0) {
        checkError();
        exitWithError();
    }

    wl_display_flush(mDisplay);
}


void QWaylandDisplay::blockingReadEvents()
{
    if (wl_display_dispatch(mDisplay) < 0) {
        checkError();
        exitWithError();
    }
}

// Like blockingReadEvents(), but bounded: if no events arrive within
// timeoutMs, gives up and returns false instead of blocking forever.
// Callers that can legitimately go without a response (e.g. waiting on
// a compositor frame callback that may never come while a surface is
// paused/not visible) must use this instead of blockingReadEvents(),
// since an unbounded wait here starves every other thread that shares
// this display connection - the reader/prepare_read protocol is
// process-wide, not per-caller.
//
// Correctness note: on timeout we must call wl_display_cancel_read()
// to undo the wl_display_prepare_read() below. Skipping that (or trying
// to "reclaim" the read some other way) corrupts the display's internal
// reader_count bookkeeping - confirmed the hard way in a throwaway
// experiment that made reader_count go negative. cancel_read() is the
// only safe way to back out of a prepared read.
bool QWaylandDisplay::blockingReadEventsWithTimeout(int timeoutMs)
{
    if (wl_display_prepare_read(mDisplay) != 0) {
        // Events are already queued locally; dispatch those instead of
        // trying to read more from the socket.
        if (wl_display_dispatch_pending(mDisplay) < 0) {
            checkError();
            exitWithError();
        }
        return true;
    }

    wl_display_flush(mDisplay);

    struct pollfd pfd;
    pfd.fd = wl_display_get_fd(mDisplay);
    pfd.events = POLLIN;
    pfd.revents = 0;

    int ret = poll(&pfd, 1, timeoutMs);

    if (ret <= 0) {
        wl_display_cancel_read(mDisplay);
        return false;
    }

    if (wl_display_read_events(mDisplay) < 0) {
        checkError();
        exitWithError();
    }

    if (wl_display_dispatch_pending(mDisplay) < 0) {
        checkError();
        exitWithError();
    }

    return true;
}

void QWaylandDisplay::exitWithError()
{
    ::exit(1);
}

QWaylandScreen *QWaylandDisplay::screenForOutput(struct wl_output *output) const
{
    for (int i = 0; i < mScreens.size(); ++i) {
        QWaylandScreen *screen = static_cast<QWaylandScreen *>(mScreens.at(i));
        if (screen->output() == output)
            return screen;
    }
    return 0;
}

void QWaylandDisplay::waitForScreens()
{
    flushRequests();

    while (true) {
        bool screensReady = !mScreens.isEmpty();

        for (int ii = 0; screensReady && ii < mScreens.count(); ++ii) {
            if (mScreens.at(ii)->geometry() == QRect(0, 0, 0, 0))
                screensReady = false;
        }

        if (!screensReady)
            blockingReadEvents();
        else
            return;
    }
}

void QWaylandDisplay::registry_global(uint32_t id, const QString &interface, uint32_t version)
{
    Q_UNUSED(version);

    struct ::wl_registry *registry = object();

    if (interface == QStringLiteral("wl_output")) {
        QWaylandScreen *screen = new QWaylandScreen(this, version, id);
        mScreens.append(screen);
        // We need to get the output events before creating surfaces
        forceRoundTrip();
        screen->init();
        mWaylandIntegration->screenAdded(screen);
    } else if (interface == QStringLiteral("wl_compositor")) {
        mCompositorVersion = qMin((int)version, 3);
        mCompositor.init(registry, id, mCompositorVersion);
    } else if (interface == QStringLiteral("wl_shm")) {
        mShm = static_cast<struct wl_shm *>(wl_registry_bind(registry, id, &wl_shm_interface,1));
    } else if (interface == QStringLiteral("wl_seat")) {
        QWaylandInputDevice *inputDevice = mWaylandIntegration->createInputDevice(this, version, id);
        mInputDevices.append(inputDevice);
#ifndef QT_NO_DRAGANDDROP
    } else if (interface == QStringLiteral("wl_data_device_manager")) {
        mDndSelectionHandler.reset(new QWaylandDataDeviceManager(this, id));
#endif
    } else if (interface == QStringLiteral("qt_surface_extension")) {
        mWindowExtension.reset(new QtWayland::qt_surface_extension(registry, id, 1));
    } else if (interface == QStringLiteral("wl_subcompositor")) {
        mSubCompositor.reset(new QtWayland::wl_subcompositor(registry, id, 1));
    } else if (interface == QStringLiteral("qt_touch_extension")) {
        mTouchExtension.reset(new QWaylandTouchExtension(this, id));
    } else if (interface == QStringLiteral("qt_key_extension")) {
        mQtKeyExtension.reset(new QWaylandQtKeyExtension(this, id));
    } else if (interface == QStringLiteral("wl_text_input_manager")) {
        mTextInputManager.reset(new QtWayland::wl_text_input_manager(registry, id, 1));
    } else if (interface == QStringLiteral("qt_hardware_integration")) {
        mHardwareIntegration.reset(new QWaylandHardwareIntegration(registry, id));
        // make a roundtrip here since we need to receive the events sent by
        // qt_hardware_integration before creating windows
        forceRoundTrip();
    }

    mGlobals.append(RegistryGlobal(id, interface, version, registry));

    foreach (Listener l, mRegistryListeners)
        (*l.listener)(l.data, registry, id, interface, version);
}

void QWaylandDisplay::registry_global_remove(uint32_t id)
{
    for (int i = 0, ie = mGlobals.count(); i != ie; ++i) {
        RegistryGlobal &global = mGlobals[i];
        if (global.id == id) {
            if (global.interface == QStringLiteral("wl_output")) {
                foreach (QWaylandScreen *screen, mScreens) {
                    if (screen->outputId() == id) {
                        mScreens.removeOne(screen);
                        mWaylandIntegration->destroyScreen(screen);
                        break;
                    }
                }
            }
            mGlobals.removeAt(i);
            break;
        }
    }
}

bool QWaylandDisplay::hasRegistryGlobal(const QString &interfaceName)
{
    Q_FOREACH (const RegistryGlobal &global, mGlobals)
        if (global.interface == interfaceName)
            return true;

    return false;
}

void QWaylandDisplay::addRegistryListener(RegistryListener listener, void *data)
{
    Listener l = { listener, data };
    mRegistryListeners.append(l);
    for (int i = 0, ie = mGlobals.count(); i != ie; ++i)
        (*l.listener)(l.data, mGlobals[i].registry, mGlobals[i].id, mGlobals[i].interface, mGlobals[i].version);
}

uint32_t QWaylandDisplay::currentTimeMillisec()
{
    //### we throw away the time information
    struct timeval tv;
    int ret = gettimeofday(&tv, 0);
    if (ret == 0)
        return tv.tv_sec*1000 + tv.tv_usec/1000;
    return 0;
}

static void
sync_callback(void *data, struct wl_callback *callback, uint32_t serial)
{
    Q_UNUSED(serial)
    bool *done = static_cast<bool *>(data);

    *done = true;
    wl_callback_destroy(callback);
}

static const struct wl_callback_listener sync_listener = {
    sync_callback
};

void QWaylandDisplay::forceRoundTrip()
{
    // wl_display_roundtrip() works on the main queue only,
    // but we use a separate one, so basically reimplement it here
    int ret = 0;
    bool done = false;
    wl_callback *callback = wl_display_sync(mDisplay);
    wl_callback_add_listener(callback, &sync_listener, &done);
    flushRequests();
    if (QThread::currentThread()->eventDispatcher()) {
        while (!done && ret >= 0) {
            QThread::currentThread()->eventDispatcher()->processEvents(QEventLoop::WaitForMoreEvents);
            ret = wl_display_dispatch_pending(mDisplay);
        }
    } else {
        while (!done && ret >= 0)
            ret = wl_display_dispatch(mDisplay);
    }

    if (ret == -1 && !done)
        wl_callback_destroy(callback);
}

bool QWaylandDisplay::supportsWindowDecoration() const
{
    static bool disabled = qgetenv("QT_WAYLAND_DISABLE_WINDOWDECORATION").toInt();
    // Stop early when disabled via the environment. Do not try to load the integration in
    // order to play nice with SHM-only, buffer integration-less systems.
    if (disabled)
        return false;

    static bool integrationSupport = clientBufferIntegration() && clientBufferIntegration()->supportsWindowDecoration();
    return integrationSupport;
}

QWaylandWindow *QWaylandDisplay::lastInputWindow() const
{
    return mLastInputWindow.data();
}

void QWaylandDisplay::setLastInputDevice(QWaylandInputDevice *device, uint32_t serial, QWaylandWindow *win)
{
    mLastInputDevice = device;
    mLastInputSerial = serial;
    mLastInputWindow = win;
}

void QWaylandDisplay::handleWindowActivated(QWaylandWindow *window)
{
    if (mActiveWindows.contains(window))
        return;

    mActiveWindows.append(window);
    requestWaylandSync();
}

void QWaylandDisplay::handleWindowDeactivated(QWaylandWindow *window)
{
    Q_ASSERT(!mActiveWindows.empty());

    if (mActiveWindows.last() == window)
        requestWaylandSync();

    mActiveWindows.removeOne(window);
}

void QWaylandDisplay::handleKeyboardFocusChanged(QWaylandInputDevice *inputDevice)
{
    QWaylandWindow *keyboardFocus = inputDevice->keyboardFocus();

    if (mLastKeyboardFocus == keyboardFocus)
        return;

    if (keyboardFocus && !keyboardFocus->shellManagesActiveState())
        handleWindowActivated(keyboardFocus);

    if (mLastKeyboardFocus && !mLastKeyboardFocus->shellManagesActiveState())
        handleWindowDeactivated(mLastKeyboardFocus);

    mLastKeyboardFocus = keyboardFocus;
}

void QWaylandDisplay::handleWindowDestroyed(QWaylandWindow *window)
{
    if (mActiveWindows.contains(window))
        handleWindowDeactivated(window);
}

void QWaylandDisplay::handleWaylandSync()
{
    // This callback is used to set the window activation because we may get an activate/deactivate
    // pair, and the latter one would be lost in the QWindowSystemInterface queue, if we issue the
    // handleWindowActivated() calls immediately.
    QWindow *activeWindow = mActiveWindows.empty() ? Q_NULLPTR : mActiveWindows.last()->window();
    if (activeWindow != QGuiApplication::focusWindow())
        QWindowSystemInterface::handleWindowActivated(activeWindow);
}

const wl_callback_listener QWaylandDisplay::syncCallbackListener = {
    [](void *data, struct wl_callback *callback, uint32_t time){
        Q_UNUSED(time);
        wl_callback_destroy(callback);
        QWaylandDisplay *display = static_cast<QWaylandDisplay *>(data);
        display->mSyncCallback = Q_NULLPTR;
        display->handleWaylandSync();
    }
};

void QWaylandDisplay::requestWaylandSync()
{
    if (mSyncCallback)
        return;

    mSyncCallback = wl_display_sync(mDisplay);
    wl_callback_add_listener(mSyncCallback, &syncCallbackListener, this);
}

}

QT_END_NAMESPACE
