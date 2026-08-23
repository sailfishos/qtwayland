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

#include "qwaylandeventthread_p.h"

#include <QtCore/private/qcore_unix_p.h>

#include <fcntl.h>
#include <errno.h>
#include <poll.h>

QT_BEGIN_NAMESPACE

namespace QtWaylandClient {

QWaylandEventThread::QWaylandEventThread(struct wl_display *display, QObject *parent)
    : QObject(parent)
    , m_display(display)
    , m_fileDescriptor(wl_display_get_fd(display))
    , m_initializationError(0)
    , m_waitingForEventsDispatched(false)
    , m_stopping(false)
{
    m_stopPipe[0] = -1;
    m_stopPipe[1] = -1;
    if (qt_safe_pipe(m_stopPipe, O_NONBLOCK) < 0)
        m_initializationError = errno;
}

QWaylandEventThread::~QWaylandEventThread()
{
    if (m_stopPipe[0] != -1)
        qt_safe_close(m_stopPipe[0]);
    if (m_stopPipe[1] != -1)
        qt_safe_close(m_stopPipe[1]);
    if (m_display)
        wl_display_disconnect(m_display);
}

bool QWaylandEventThread::isValid() const
{
    return m_initializationError == 0;
}

int QWaylandEventThread::initializationError() const
{
    return m_initializationError;
}

void QWaylandEventThread::start()
{
    QMetaObject::invokeMethod(this, "readWaylandEvents", Qt::QueuedConnection);
}

// ### be careful what you do, this function may also be called from other
// threads to clean up & exit.
void QWaylandEventThread::checkError() const
{
    int ecode = wl_display_get_error(m_display);
    if ((ecode == EPIPE || ecode == ECONNRESET)) {
        // special case this to provide a nicer error
        qWarning("The Wayland connection broke. Did the Wayland compositor die?");
    } else {
        qErrnoWarning(ecode, "The Wayland connection experienced a fatal error");
    }
}

void QWaylandEventThread::eventsDispatched()
{
    QMutexLocker locker(&m_mutex);
    if (m_waitingForEventsDispatched) {
        m_waitingForEventsDispatched = false;
        m_waitCondition.wakeOne();
    }
}

void QWaylandEventThread::stop()
{
    {
        QMutexLocker locker(&m_mutex);
        if (m_stopping)
            return;
        m_stopping = true;
        m_waitCondition.wakeAll();
    }

    if (m_stopPipe[1] != -1) {
        char byte = 0;
        qt_safe_write(m_stopPipe[1], &byte, sizeof byte);
    }
}

bool QWaylandEventThread::waitForEventsDispatched()
{
    // Do not read again until the thread that owns the default queue has
    // dispatched everything the last read may have put there. Besides being
    // required before prepare_read() can succeed again, this keeps at most one
    // queued newEventsRead() signal while that thread is blocked.
    {
        QMutexLocker locker(&m_mutex);
        if (m_stopping)
            return false;
        m_waitingForEventsDispatched = true;
    }

    emit newEventsRead();

    QMutexLocker locker(&m_mutex);
    while (m_waitingForEventsDispatched && !m_stopping)
        m_waitCondition.wait(&m_mutex);

    return !m_stopping;
}

bool QWaylandEventThread::flushDisplay(bool *waitingForWrite)
{
    *waitingForWrite = false;

    int ret = wl_display_flush(m_display);
    if (ret >= 0)
        return true;
    if (errno == EAGAIN) {
        *waitingForWrite = true;
        return true;
    }

    checkError();
    return false;
}

void QWaylandEventThread::readWaylandEvents()
{
    for (;;) {
        // Keep the read preparation active across poll(). This prevents
        // another reader of the shared display from draining the fd and
        // leaving events on our queue without anything to wake its owner.
        while (wl_display_prepare_read(m_display) != 0) {
            if (!waitForEventsDispatched())
                return;
        }

        bool waitingForWrite;
        if (!flushDisplay(&waitingForWrite)) {
            wl_display_cancel_read(m_display);
            emit fatalError();
            return;
        }

        for (;;) {
            struct pollfd pollFds[2];
            pollFds[0].fd = m_fileDescriptor;
            pollFds[0].events = POLLIN | (waitingForWrite ? POLLOUT : 0);
            pollFds[0].revents = 0;
            pollFds[1].fd = m_stopPipe[0];
            pollFds[1].events = POLLIN;
            pollFds[1].revents = 0;

            int ret;
            do {
                ret = poll(pollFds, 2, -1);
            } while (ret < 0 && errno == EINTR);

            if (ret < 0) {
                wl_display_cancel_read(m_display);
                qErrnoWarning(errno, "Failed to poll the Wayland connection");
                emit fatalError();
                return;
            }

            if (pollFds[1].revents) {
                wl_display_cancel_read(m_display);
                return;
            }

            if (pollFds[0].revents & POLLOUT) {
                if (!flushDisplay(&waitingForWrite)) {
                    wl_display_cancel_read(m_display);
                    emit fatalError();
                    return;
                }
            }

            if (pollFds[0].revents & (POLLIN | POLLERR | POLLHUP | POLLNVAL)) {
                if (wl_display_read_events(m_display) < 0) {
                    checkError();
                    emit fatalError();
                    return;
                }
                break;
            }
        }

        if (!waitForEventsDispatched())
            return;
    }
}

wl_display *QWaylandEventThread::display() const
{
    return m_display;
}

}

QT_END_NAMESPACE
