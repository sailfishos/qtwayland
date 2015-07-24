/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtWaylandCompositor module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
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
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef SURFACEBUFFER_H
#define SURFACEBUFFER_H

#include <QtCore/QRect>
#include <QtGui/qopengl.h>
#include <QImage>
#include <QAtomicInt>

#include <wayland-server.h>

QT_BEGIN_NAMESPACE

class QWaylandClientBufferIntegration;
class QWaylandBufferRef;

namespace QtWayland {

class Surface;
class Compositor;

struct surface_buffer_destroy_listener
{
    struct wl_listener listener;
    class SurfaceBuffer *surfaceBuffer;
};

class SurfaceBuffer
{
public:
    SurfaceBuffer(Surface *surface);

    ~SurfaceBuffer();

    void initialize(struct ::wl_resource *bufferResource);
    void destructBufferState();

    QSize size() const;

    bool isShmBuffer() const;
    bool isYInverted() const;

    inline bool isRegisteredWithBuffer() const { return m_is_registered_for_buffer; }

    void sendRelease();
    void disown();

    void setDisplayed();

    inline bool isComitted() const { return m_committed; }
    inline void setCommitted() { m_committed = true; }
    inline bool isDisplayed() const { return m_is_displayed; }

    inline bool textureCreated() const { return m_texture; }

    bool isDestroyed() { return m_destroyed; }

    void createTexture();
#ifdef QT_COMPOSITOR_WAYLAND_GL
    inline GLuint texture() const;
#else
    inline uint texture() const;
#endif

    void destroyTexture();

    inline struct ::wl_resource *waylandBufferHandle() const { return m_buffer; }

    void handleAboutToBeDisplayed();
    void handleDisplayed();

    void bufferWasDestroyed();
    void setDestroyIfUnused(bool destroy);

    void *handle() const;
    QImage image();
private:
    void ref();
    void deref();
    void destroyIfUnused();

    Surface *m_surface;
    Compositor *m_compositor;
    struct ::wl_resource *m_buffer;
    struct surface_buffer_destroy_listener m_destroy_listener;
    bool m_committed;
    bool m_is_registered_for_buffer;
    bool m_surface_has_buffer;
    bool m_destroyed;

    bool m_is_displayed;
#ifdef QT_COMPOSITOR_WAYLAND_GL
    GLuint m_texture;
#else
    uint m_texture;
#endif
    void *m_handle;
    mutable bool m_is_shm_resolved;

#if (WAYLAND_VERSION_MAJOR >= 1) && (WAYLAND_VERSION_MINOR >= 2)
    mutable struct ::wl_shm_buffer *m_shmBuffer;
#else
    mutable struct ::wl_buffer *m_shmBuffer;
#endif

    mutable bool m_isSizeResolved;
    mutable QSize m_size;
    QAtomicInt m_refCount;
    bool m_used;
    bool m_destroyIfUnused;

    QImage m_image;

    static void destroy_listener_callback(wl_listener *listener, void *data);

    friend class ::QWaylandBufferRef;
};

#ifdef QT_COMPOSITOR_WAYLAND_GL
GLuint SurfaceBuffer::texture() const
{
    if (m_buffer)
        return m_texture;
    return 0;
}
#else
uint SurfaceBuffer::texture() const
{
    return 0;
}
#endif

}

QT_END_NAMESPACE

#endif // SURFACEBUFFER_H
