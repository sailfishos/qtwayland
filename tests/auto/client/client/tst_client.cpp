/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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

#include "mockcompositor.h"

#include <QtWaylandClient/private/qwaylandeventthread_p.h>
#include <QtCore/private/qcore_unix_p.h>

#include <QBackingStore>
#include <QAtomicInt>
#include <QElapsedTimer>
#include <QPainter>
#include <QScreen>
#include <QTemporaryDir>
#include <QThread>
#include <QWindow>
#include <QMimeData>
#include <QPixmap>
#include <QDrag>

#include <QtGui/qpa/qplatformnativeinterface.h>
#include <QtTest/QtTest>

#include <errno.h>
#include <fcntl.h>
#include <poll.h>

static const QSize screenSize(1600, 1200);

class EventThreadGuard
{
public:
    EventThreadGuard()
        : display(wl_display_connect(NULL))
        , eventThread(display ? new QtWaylandClient::QWaylandEventThread(display) : 0)
    {
        if (!eventThread || !eventThread->isValid())
            return;
        eventThread->moveToThread(&thread);
        thread.start();
        eventThread->start();
    }

    ~EventThreadGuard()
    {
        if (!eventThread)
            return;
        eventThread->stop();
        thread.quit();
        thread.wait();
        delete eventThread;
    }

    bool isValid() const
    {
        return eventThread && eventThread->isValid();
    }

    QThread thread;
    wl_display *display;
    QtWaylandClient::QWaylandEventThread *eventThread;
};

class ForeignReader : public QThread
{
public:
    ForeignReader(wl_display *display, wl_event_queue *queue)
        : m_display(display)
        , m_queue(queue)
        , m_initializationError(0)
        , m_ready(false)
        , m_prepared(false)
        , m_stopping(0)
        , m_readResult(-1)
        , m_dispatchResult(-1)
    {
        m_stopPipe[0] = -1;
        m_stopPipe[1] = -1;
        if (qt_safe_pipe(m_stopPipe, O_NONBLOCK) < 0)
            m_initializationError = errno;
    }

    ~ForeignReader()
    {
        stop();
        wait();
        if (m_stopPipe[0] != -1)
            qt_safe_close(m_stopPipe[0]);
        if (m_stopPipe[1] != -1)
            qt_safe_close(m_stopPipe[1]);
    }

    bool isValid() const
    {
        return m_initializationError == 0;
    }

    bool waitUntilPrepared()
    {
        QElapsedTimer timer;
        timer.start();

        QMutexLocker locker(&m_mutex);
        while (!m_ready) {
            qint64 remaining = 5000 - timer.elapsed();
            if (remaining <= 0
                    || !m_waitCondition.wait(&m_mutex, static_cast<unsigned long>(remaining)))
                break;
        }
        return m_prepared;
    }

    int readResult() const { return m_readResult; }
    int dispatchResult() const { return m_dispatchResult; }

    void stop()
    {
        if (!m_stopping.testAndSetOrdered(0, 1))
            return;

        if (m_stopPipe[1] != -1) {
            char byte = 0;
            qt_safe_write(m_stopPipe[1], &byte, sizeof byte);
        }
    }

protected:
    void run() Q_DECL_OVERRIDE
    {
        while (wl_display_prepare_read_queue(m_display, m_queue) != 0) {
            if (m_stopping.loadAcquire()) {
                setPrepared(false);
                return;
            }
            if (wl_display_dispatch_queue_pending(m_display, m_queue) < 0) {
                setPrepared(false);
                return;
            }
        }

        if (m_stopping.loadAcquire()) {
            wl_display_cancel_read(m_display);
            setPrepared(false);
            return;
        }

        setPrepared(true);

        struct pollfd pollFds[2];
        pollFds[0].fd = wl_display_get_fd(m_display);
        pollFds[0].events = POLLIN | POLLERR | POLLHUP;
        pollFds[0].revents = 0;
        pollFds[1].fd = m_stopPipe[0];
        pollFds[1].events = POLLIN;
        pollFds[1].revents = 0;

        int ret;
        do {
            ret = poll(pollFds, 2, 5000);
        } while (ret < 0 && errno == EINTR);

        if (ret <= 0 || pollFds[1].revents
                || !(pollFds[0].revents & (POLLIN | POLLERR | POLLHUP))) {
            wl_display_cancel_read(m_display);
            return;
        }

        m_readResult = wl_display_read_events(m_display);
        if (m_readResult == 0)
            m_dispatchResult = wl_display_dispatch_queue_pending(m_display, m_queue);
    }

private:
    void setPrepared(bool prepared)
    {
        QMutexLocker locker(&m_mutex);
        m_prepared = prepared;
        m_ready = true;
        m_waitCondition.wakeAll();
    }

    wl_display *m_display;
    wl_event_queue *m_queue;
    int m_stopPipe[2];
    int m_initializationError;
    QMutex m_mutex;
    QWaitCondition m_waitCondition;
    bool m_ready;
    bool m_prepared;
    QAtomicInt m_stopping;
    int m_readResult;
    int m_dispatchResult;
};

class EventQueueGuard
{
public:
    explicit EventQueueGuard(wl_display *display)
        : m_queue(wl_display_create_queue(display))
    {
    }

    ~EventQueueGuard()
    {
        if (m_queue)
            wl_event_queue_destroy(m_queue);
    }

    bool isValid() const { return m_queue; }
    wl_event_queue *queue() const { return m_queue; }

private:
    Q_DISABLE_COPY(EventQueueGuard)
    wl_event_queue *m_queue;
};

class SyncCallback
{
public:
    SyncCallback(wl_display *display, wl_event_queue *queue = 0)
        : m_callback(wl_display_sync(display))
        , m_done(0)
    {
        if (!m_callback)
            return;
        if (queue)
            wl_proxy_set_queue(reinterpret_cast<wl_proxy *>(m_callback), queue);
        if (wl_callback_add_listener(m_callback, &syncListener, this) != 0) {
            wl_callback_destroy(m_callback);
            m_callback = 0;
        }
    }

    ~SyncCallback()
    {
        if (m_callback)
            wl_callback_destroy(m_callback);
    }

    bool isValid() const { return m_callback; }
    int isDone() const { return m_done.loadAcquire(); }

private:
    static void done(void *data, struct wl_callback *callback, uint32_t serial)
    {
        Q_UNUSED(serial)
        SyncCallback *sync = static_cast<SyncCallback *>(data);
        sync->m_callback = 0;
        sync->m_done.storeRelease(1);
        wl_callback_destroy(callback);
    }

    static const struct wl_callback_listener syncListener;

    Q_DISABLE_COPY(SyncCallback)
    wl_callback *m_callback;
    QAtomicInt m_done;
};

const struct wl_callback_listener SyncCallback::syncListener = {
    SyncCallback::done
};

class TestWindow : public QWindow
{
public:
    TestWindow()
        : focusInEventCount(0)
        , focusOutEventCount(0)
        , keyPressEventCount(0)
        , keyReleaseEventCount(0)
        , mousePressEventCount(0)
        , mouseReleaseEventCount(0)
        , touchEventCount(0)
        , keyCode(0)
    {
        setSurfaceType(QSurface::RasterSurface);
        setGeometry(0, 0, 32, 32);
        create();
    }

    void focusInEvent(QFocusEvent *)
    {
        ++focusInEventCount;
    }

    void focusOutEvent(QFocusEvent *)
    {
        ++focusOutEventCount;
    }

    void keyPressEvent(QKeyEvent *event)
    {
        ++keyPressEventCount;
        keyCode = event->nativeScanCode();
    }

    void keyReleaseEvent(QKeyEvent *event)
    {
        ++keyReleaseEventCount;
        keyCode = event->nativeScanCode();
    }

    void mousePressEvent(QMouseEvent *event)
    {
        ++mousePressEventCount;
        mousePressPos = event->pos();
    }

    void mouseReleaseEvent(QMouseEvent *)
    {
        ++mouseReleaseEventCount;
    }

    void touchEvent(QTouchEvent *event) Q_DECL_OVERRIDE
    {
        ++touchEventCount;
    }

    int focusInEventCount;
    int focusOutEventCount;
    int keyPressEventCount;
    int keyReleaseEventCount;
    int mousePressEventCount;
    int mouseReleaseEventCount;
    int touchEventCount;

    uint keyCode;
    QPoint mousePressPos;
};

class tst_WaylandClient : public QObject
{
    Q_OBJECT
public:
    tst_WaylandClient(MockCompositor *c)
        : compositor(c)
    {
        QSocketNotifier *notifier = new QSocketNotifier(compositor->waylandFileDescriptor(), QSocketNotifier::Read, this);
        connect(notifier, SIGNAL(activated(int)), this, SLOT(processWaylandEvents()));
        // connect to the event dispatcher to make sure to flush out the outgoing message queue
        connect(QCoreApplication::eventDispatcher(), &QAbstractEventDispatcher::awake, this, &tst_WaylandClient::processWaylandEvents);
        connect(QCoreApplication::eventDispatcher(), &QAbstractEventDispatcher::aboutToBlock, this, &tst_WaylandClient::processWaylandEvents);
    }

public slots:
    void processWaylandEvents()
    {
        compositor->processWaylandEvents();
    }

    void cleanup()
    {
        // make sure the surfaces from the last test are properly cleaned up
        // and don't show up as false positives in the next test
        QTRY_VERIFY(!compositor->surface());
    }

private slots:
    void screen();
    void createDestroyWindow();
    void events();
    void backingStore();
    void eventThreadSignalIsBounded();
    void eventThreadCooperatesWithForeignReader();
    void touchDrag();
    void mouseDrag();

private:
    MockCompositor *compositor;
};

void tst_WaylandClient::screen()
{
    QTRY_COMPARE(QGuiApplication::primaryScreen()->size(), screenSize);
}

void tst_WaylandClient::createDestroyWindow()
{
    TestWindow window;
    window.show();

    QTRY_VERIFY(compositor->surface());

    window.destroy();
    QTRY_VERIFY(!compositor->surface());
}

void tst_WaylandClient::events()
{
    TestWindow window;
    window.show();

    QSharedPointer<MockSurface> surface;
    QTRY_VERIFY(surface = compositor->surface());

    QCOMPARE(window.focusInEventCount, 0);
    compositor->setKeyboardFocus(surface);
    QTRY_COMPARE(window.focusInEventCount, 1);
    QTRY_COMPARE(QGuiApplication::focusWindow(), &window);

    QCOMPARE(window.focusOutEventCount, 0);
    compositor->setKeyboardFocus(QSharedPointer<MockSurface>(0));
    QTRY_COMPARE(window.focusOutEventCount, 1);
    QTRY_COMPARE(QGuiApplication::focusWindow(), static_cast<QWindow *>(0));

    compositor->setKeyboardFocus(surface);
    QTRY_COMPARE(window.focusInEventCount, 2);
    QTRY_COMPARE(QGuiApplication::focusWindow(), &window);

    uint keyCode = 80; // arbitrarily chosen
    QCOMPARE(window.keyPressEventCount, 0);
    compositor->sendKeyPress(surface, keyCode);
    QTRY_COMPARE(window.keyPressEventCount, 1);
    QTRY_COMPARE(window.keyCode, keyCode);

    QCOMPARE(window.keyReleaseEventCount, 0);
    compositor->sendKeyRelease(surface, keyCode);
    QTRY_COMPARE(window.keyReleaseEventCount, 1);
    QCOMPARE(window.keyCode, keyCode);

    QPoint mousePressPos(16, 16);
    QCOMPARE(window.mousePressEventCount, 0);
    compositor->sendMousePress(surface, mousePressPos);
    QTRY_COMPARE(window.mousePressEventCount, 1);
    QTRY_COMPARE(window.mousePressPos, mousePressPos);

    QCOMPARE(window.mouseReleaseEventCount, 0);
    compositor->sendMouseRelease(surface);
    QTRY_COMPARE(window.mouseReleaseEventCount, 1);

    const int touchId = 0;
    compositor->sendTouchDown(surface, QPoint(10, 10), touchId);
    compositor->sendTouchFrame(surface);
    QTRY_COMPARE(window.touchEventCount, 1);

    compositor->sendTouchUp(surface, touchId);
    compositor->sendTouchFrame(surface);
    QTRY_COMPARE(window.touchEventCount, 2);
}

void tst_WaylandClient::backingStore()
{
    TestWindow window;
    window.show();

    QSharedPointer<MockSurface> surface;
    QTRY_VERIFY(surface = compositor->surface());

    QRect rect(QPoint(), window.size());

    QBackingStore backingStore(&window);
    backingStore.resize(rect.size());

    backingStore.beginPaint(rect);

    QColor color = Qt::magenta;

    QPainter p(backingStore.paintDevice());
    p.fillRect(rect, color);
    p.end();

    backingStore.endPaint();

    QVERIFY(surface->image.isNull());

    backingStore.flush(rect);

    QTRY_COMPARE(surface->image.size(), window.frameGeometry().size());
    QTRY_COMPARE(surface->image.pixel(window.frameMargins().left(), window.frameMargins().top()), color.rgba());

    window.hide();

    // hiding the window should detach the buffer
    QTRY_VERIFY(surface->image.isNull());
}

void tst_WaylandClient::eventThreadSignalIsBounded()
{
    EventThreadGuard guard;
    QVERIFY(guard.isValid());
    int signalCount = 0;
    QObject signalReceiver;
    connect(guard.eventThread, &QtWaylandClient::QWaylandEventThread::newEventsRead,
            &signalReceiver, [&signalCount]() { ++signalCount; });

    wl_display *display = guard.eventThread->display();
    QVERIFY(wl_display_sync(display));
    QVERIFY(wl_display_flush(display) >= 0);
    QTRY_COMPARE(signalCount, 1);

    // Leave the first callback pending and make the socket readable again.
    // The old notifier implementation repeatedly emitted newEventsRead() in
    // this state because prepare_read() failed while the fd stayed readable.
    QVERIFY(wl_display_sync(display));
    QVERIFY(wl_display_flush(display) >= 0);
    QTest::qWait(100);
    QCOMPARE(signalCount, 1);
}

void tst_WaylandClient::eventThreadCooperatesWithForeignReader()
{
    QPlatformNativeInterface *nativeInterface = QGuiApplication::platformNativeInterface();
    wl_display *display = static_cast<wl_display *>(
                nativeInterface->nativeResourceForIntegration("display"));
    QVERIFY(display);

    EventQueueGuard foreignQueue(display);
    QVERIFY(foreignQueue.isValid());

    SyncCallback foreignCallback(display, foreignQueue.queue());
    QVERIFY(foreignCallback.isValid());
    SyncCallback defaultCallback(display);
    QVERIFY(defaultCallback.isValid());

    ForeignReader foreignReader(display, foreignQueue.queue());
    QVERIFY(foreignReader.isValid());
    foreignReader.start();
    QVERIFY(foreignReader.waitUntilPrepared());

    QVERIFY(wl_display_flush(display) >= 0);
    compositor->processWaylandEvents();

    QTRY_COMPARE(defaultCallback.isDone(), 1);
    QVERIFY(foreignReader.wait(6000));
    QCOMPARE(foreignReader.readResult(), 0);
    QVERIFY(foreignReader.dispatchResult() >= 0);
    QCOMPARE(foreignCallback.isDone(), 1);
}

class DndWindow : public QWindow
{
    Q_OBJECT

public:
    DndWindow(QWindow *parent = 0)
        : QWindow(parent)
        , dragStarted(false)
    {
        QImage cursorImage(64,64,QImage::Format_ARGB32);
        cursorImage.fill(Qt::blue);
        m_dragIcon = QPixmap::fromImage(cursorImage);
    }
    ~DndWindow(){}
    bool dragStarted;

protected:
    void mousePressEvent(QMouseEvent *event) Q_DECL_OVERRIDE
    {
        if (dragStarted)
            return;
        dragStarted = true;

        QByteArray dataBytes;
        QMimeData *mimeData = new QMimeData;
        mimeData->setData("application/x-dnditemdata", dataBytes);
        QDrag *drag = new QDrag(this);
        drag->setMimeData(mimeData);
        drag->setPixmap(m_dragIcon);
        drag->exec(Qt::CopyAction | Qt::MoveAction, Qt::CopyAction);
    }
private:
    QPixmap m_dragIcon;
};

void tst_WaylandClient::touchDrag()
{
    DndWindow window;
    window.show();

    QSharedPointer<MockSurface> surface;
    QTRY_VERIFY(surface = compositor->surface());

    compositor->setKeyboardFocus(surface);
    QTRY_COMPARE(QGuiApplication::focusWindow(), &window);

    const int id = 0;
    compositor->sendTouchDown(surface, QPoint(10, 10), id);
    compositor->sendTouchMotion(surface, QPoint(20, 20), id);
    compositor->sendTouchFrame(surface);
    compositor->waitForStartDrag();
    compositor->sendDataDeviceDataOffer(surface);
    compositor->sendDataDeviceEnter(surface, QPoint(20, 20));
    compositor->sendDataDeviceMotion( QPoint(21, 21));
    compositor->sendDataDeviceDrop(surface);
    compositor->sendDataDeviceLeave(surface);
    QTRY_VERIFY(window.dragStarted);
}

void tst_WaylandClient::mouseDrag()
{
    DndWindow window;
    window.show();

    QSharedPointer<MockSurface> surface;
    QTRY_VERIFY(surface = compositor->surface());

    compositor->setKeyboardFocus(surface);
    QTRY_COMPARE(QGuiApplication::focusWindow(), &window);

    compositor->sendMousePress(surface, QPoint(10, 10));
    compositor->sendDataDeviceDataOffer(surface);
    compositor->sendDataDeviceEnter(surface, QPoint(20, 20));
    compositor->sendDataDeviceMotion( QPoint(21, 21));
    compositor->waitForStartDrag();
    compositor->sendDataDeviceDrop(surface);
    compositor->sendDataDeviceLeave(surface);
    QTRY_VERIFY(window.dragStarted);
}

int main(int argc, char **argv)
{
    QTemporaryDir runtimeDir;
    if (!runtimeDir.isValid())
        return EXIT_FAILURE;
    qputenv("XDG_RUNTIME_DIR", runtimeDir.path().toLocal8Bit());
    setenv("QT_QPA_PLATFORM", "wayland", 1); // force QGuiApplication to use wayland plugin

    // wayland-egl hangs in the test setup when we try to initialize. Until it gets
    // figured out, avoid clientBufferIntegration() from being called in
    // QWaylandWindow::createDecorations().
    setenv("QT_WAYLAND_DISABLE_WINDOWDECORATION", "1", 1);

    MockCompositor compositor;
    compositor.setOutputGeometry(QRect(QPoint(), screenSize));

    QGuiApplication app(argc, argv);
    compositor.applicationInitialized();

    tst_WaylandClient tc(&compositor);
    return QTest::qExec(&tc, argc, argv);
}

#include <tst_client.moc>
