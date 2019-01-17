/****************************************************************************
**
** Copyright (C) 2014 Jolla Ltd, author: <giulio.camuffo@jollamobile.com>
** Contact: http://www.qt-project.org/legal
**
** This file is part of the plugins of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <QSGTexture>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLTexture>
#include <QQuickWindow>
#include <QDebug>
#include <QQmlPropertyMap>
#include <QThreadStorage>

#include "qwaylandquicksurface.h"
#include "qwaylandquickcompositor.h"
#include "qwaylandsurfaceitem.h"
#include <QtCompositor/qwaylandbufferref.h>
#include <QtCompositor/private/qwaylandsurface_p.h>

QT_BEGIN_NAMESPACE

class QWaylandSurfaceTexture : public QSGTexture
{
public:
    QWaylandSurfaceTexture(const QWaylandBufferRef &ref, QWaylandQuickSurface *surface)
        : m_bufferRef(ref)
        , m_textureSize(surface->size())
        , m_textureId(m_bufferRef.createTexture())
        , m_hasAlphaChannel(surface->useTextureAlpha())
    {
    }

    ~QWaylandSurfaceTexture()
    {
        m_bufferRef.destroyTexture();
    }

    int textureId() const { return m_textureId; }
    QSize textureSize() const { return m_textureSize; }
    bool hasAlphaChannel() const { return m_hasAlphaChannel; }
    bool hasMipmaps() const { return false; }

    void bind()
    {
        glBindTexture(GL_TEXTURE_2D, m_textureId);
        updateBindOptions();
    }

private:
    QWaylandBufferRef m_bufferRef;
    QSize m_textureSize;
    int m_textureId;
    bool m_hasAlphaChannel;
};

class BufferAttacher : public QWaylandBufferAttacher
{
public:
    BufferAttacher()
        : surface(0)
        , texture(0)
        , update(false)
    {

    }

    ~BufferAttacher()
    {
        if (texture)
            texture->deleteLater();
        bufferRef = QWaylandBufferRef();
        nextBuffer = QWaylandBufferRef();
    }

    void attach(const QWaylandBufferRef &ref) Q_DECL_OVERRIDE
    {
        nextBuffer = ref;
        update = true;
    }

    void createTexture()
    {
        bufferRef = nextBuffer;
        delete texture;
        texture = 0;

        QQuickWindow *window = static_cast<QQuickWindow *>(surface->compositor()->window());
        if (nextBuffer) {
            if (bufferRef.isShm()) {
                texture = window->createTextureFromImage(bufferRef.image());
            } else {
                texture = new QWaylandSurfaceTexture(bufferRef, surface);
            }
            texture->bind();
        }

        update = false;
    }

    void unmapped() Q_DECL_OVERRIDE
    {
        nextBuffer = QWaylandBufferRef();
        update = true;
    }

    void invalidateTexture()
    {
        if (bufferRef)
            bufferRef.destroyTexture();
        delete texture;
        texture = 0;
        update = true;
        bufferRef = QWaylandBufferRef();
    }

    QWaylandQuickSurface *surface;
    QWaylandBufferRef bufferRef;
    QWaylandBufferRef nextBuffer;
    QSGTexture *texture;
    bool update;
};


namespace {

class BufferReleaser : public QEvent
{
public:
    BufferReleaser(const QVector<QWaylandBufferRef> &buffers)
        : QEvent(MaxUser)
        , m_buffers(buffers)
    {
    }

    static void enqueue(const QWaylandBufferRef &buffer, QQuickWindow *window)
    {
        if (!buffer) {
            return;
        }

        // With a threaded renderer this is one window -> one thread so the vector will never
        // have more than one item and the thread will be destroyed before the window is so this
        // could have just been a vector of buffers. But a non threaded renderer there are multiple
        // windows in a thread that will outlive them all.
        struct WindowBuffers { QPointer<QQuickWindow> window; QVector<QWaylandBufferRef> buffers; };
        static QThreadStorage<QVector<WindowBuffers>> windowBufferStorage;
        QVector<WindowBuffers> &windowBuffers = windowBufferStorage.localData();
        for (auto it = windowBuffers.begin(); it != windowBuffers.end(); ) {
            if (it->window == window) {
                it->buffers.append(buffer);
                return;
            } else if (!it->window) {
                it = windowBuffers.erase(it);
            } else {
                ++it;
            }
        }
        windowBuffers.append({ window, { buffer } });

        QObject::connect(window, &QQuickWindow::frameSwapped, [&windowBuffers, window]() {
            auto it = windowBuffers.begin();
            do {
                if (it == windowBuffers.end()) {
                    return; // This should never happen
                }
            } while (it->window != window);

            if (!it->buffers.isEmpty()) {
                BufferReleaser *releaser = new BufferReleaser(it->buffers);
                it->buffers.clear();
                QCoreApplication::postEvent(window, releaser);
            };
        });
    }

private:
    const QVector<QWaylandBufferRef> m_buffers;
};

}

class QWaylandQuickSurfacePrivate : public QWaylandSurfacePrivate
{
public:
    QWaylandQuickSurfacePrivate(wl_client *client, quint32 id, int version, QWaylandQuickCompositor *c, QWaylandQuickSurface *surf)
        : QWaylandSurfacePrivate(client, id, version, c, surf)
        , buffer(new BufferAttacher)
        , compositor(c)
        , useTextureAlpha(true)
        , windowPropertyMap(new QQmlPropertyMap)
        , clientRenderingEnabled(true)
    {

    }

    ~QWaylandQuickSurfacePrivate()
    {
        windowPropertyMap->deleteLater();
        // buffer is deleted automatically by ~Surface(), since it is the assigned attacher
    }

    void surface_commit(Resource *resource) Q_DECL_OVERRIDE
    {
        QWaylandSurfacePrivate::surface_commit(resource);

        compositor->update();
    }

    BufferAttacher *buffer;
    QWaylandQuickCompositor *compositor;
    bool useTextureAlpha;
    QQmlPropertyMap *windowPropertyMap;
    bool clientRenderingEnabled;
};

QWaylandQuickSurface::QWaylandQuickSurface(wl_client *client, quint32 id, int version, QWaylandQuickCompositor *compositor)
                    : QWaylandSurface(new QWaylandQuickSurfacePrivate(client, id, version, compositor, this))
{
    Q_D(QWaylandQuickSurface);
    d->buffer->surface = this;
    setBufferAttacher(d->buffer);

    QQuickWindow *window = static_cast<QQuickWindow *>(compositor->window());
    connect(window, &QQuickWindow::beforeSynchronizing, this, [this, window]() { updateTexture(window); }, Qt::DirectConnection);
    connect(window, &QQuickWindow::sceneGraphInvalidated, this, &QWaylandQuickSurface::invalidateTexture, Qt::DirectConnection);
    connect(window, &QQuickWindow::sceneGraphAboutToStop, this, &QWaylandQuickSurface::invalidateTexture, Qt::DirectConnection);
    connect(this, &QWaylandSurface::windowPropertyChanged, d->windowPropertyMap, &QQmlPropertyMap::insert);
    connect(d->windowPropertyMap, &QQmlPropertyMap::valueChanged, this, &QWaylandSurface::setWindowProperty);

}

QWaylandQuickSurface::~QWaylandQuickSurface()
{

}

QSGTexture *QWaylandQuickSurface::texture() const
{
    Q_D(const QWaylandQuickSurface);
    return d->buffer->texture;
}

bool QWaylandQuickSurface::useTextureAlpha() const
{
    Q_D(const QWaylandQuickSurface);
    return d->useTextureAlpha;
}

void QWaylandQuickSurface::setUseTextureAlpha(bool useTextureAlpha)
{
    Q_D(QWaylandQuickSurface);
    if (d->useTextureAlpha != useTextureAlpha) {
        d->useTextureAlpha = useTextureAlpha;
        emit useTextureAlphaChanged();
        emit configure(d->buffer->bufferRef);
    }
}

QObject *QWaylandQuickSurface::windowPropertyMap() const
{
    Q_D(const QWaylandQuickSurface);
    return d->windowPropertyMap;
}


void QWaylandQuickSurface::updateTexture(QQuickWindow *window)
{
    Q_D(QWaylandQuickSurface);
    const bool update = d->buffer->update;
    if (d->buffer->update)
        d->buffer->createTexture();
    foreach (QWaylandSurfaceView *view, views())
        static_cast<QWaylandSurfaceItem *>(view)->updateTexture(update);

    BufferReleaser::enqueue(d->buffer->bufferRef, window);
}

void QWaylandQuickSurface::invalidateTexture()
{
    Q_D(QWaylandQuickSurface);
    d->buffer->invalidateTexture();
    foreach (QWaylandSurfaceView *view, views())
        static_cast<QWaylandSurfaceItem *>(view)->updateTexture(true);
    emit redraw();
}

bool QWaylandQuickSurface::clientRenderingEnabled() const
{
    Q_D(const QWaylandQuickSurface);
    return d->clientRenderingEnabled;
}

void QWaylandQuickSurface::setClientRenderingEnabled(bool enabled)
{
    Q_D(QWaylandQuickSurface);
    if (d->clientRenderingEnabled != enabled) {
        d->clientRenderingEnabled = enabled;

        sendOnScreenVisibilityChange(enabled);

        emit clientRenderingEnabledChanged();
    }
}

QT_END_NAMESPACE
