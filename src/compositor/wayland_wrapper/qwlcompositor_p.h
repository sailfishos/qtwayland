/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Compositor.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef WL_COMPOSITOR_H
#define WL_COMPOSITOR_H

#include <QtCompositor/qwaylandexport.h>
#include <QtCompositor/qwaylandcompositor.h>

#include <QtCore/QSet>
#include <QtGui/QWindow>

#include <private/qwldisplay_p.h>

#include <wayland-server.h>

QT_BEGIN_NAMESPACE

class QWaylandCompositor;
class QWaylandInputDevice;
class QWaylandGraphicsHardwareIntegration;
class WindowManagerServerIntegration;
class QMimeData;
class QPlatformScreenPageFlipper;
class QPlatformScreenBuffer;

namespace QtWayland {

class Surface;
class SurfaceBuffer;
class InputDevice;
class DataDeviceManager;
class OutputGlobal;
class OutputExtensionGlobal;
class SurfaceExtensionGlobal;
class SubSurfaceExtensionGlobal;
class Shell;
class TouchExtensionGlobal;
class QtKeyExtensionGlobal;
class TextInputManager;
class InputPanel;

class Q_COMPOSITOR_EXPORT Compositor : public QObject
{
    Q_OBJECT

public:
    Compositor(QWaylandCompositor *qt_compositor, QWaylandCompositor::ExtensionFlag extensions);
    ~Compositor();

    void frameFinished(Surface *surface = 0);

    InputDevice *defaultInputDevice(); //we just have 1 default device for now (since QPA doesn't give us anything else)

    void createSurface(struct wl_client *client, uint32_t id);
    void surfaceDestroyed(Surface *surface);
    void markSurfaceAsDirty(Surface *surface);

    void destroyClient(WaylandClient *client);

    static uint currentTimeMsecs();

    QWindow *window() const;

    QWaylandGraphicsHardwareIntegration *graphicsHWIntegration() const;
    void initializeHardwareIntegration();
    void initializeDefaultInputDevice();
    void initializeWindowManagerProtocol();
    bool setDirectRenderSurface(Surface *surface, QOpenGLContext *context);
    Surface *directRenderSurface() const {return m_directRenderSurface;}
    QOpenGLContext *directRenderContext() const {return m_directRenderContext;}
    QPlatformScreenPageFlipper *pageFlipper() const { return m_pageFlipper; }
    void setDirectRenderingActive(bool active);

    QList<Surface*> surfaces() const { return m_surfaces; }
    QList<Surface*> surfacesForClient(wl_client* client);
    QWaylandCompositor *waylandCompositor() const { return m_qt_compositor; }

    struct wl_display *wl_display() const { return m_display->handle(); }

    static Compositor *instance();

    QList<struct wl_client *> clients() const;

    WindowManagerServerIntegration *windowManagerIntegration() const { return m_windowManagerIntegration; }

    void setScreenOrientation(Qt::ScreenOrientation orientation);
    Qt::ScreenOrientation screenOrientation() const;
    void setOutputGeometry(const QRect &geometry);
    QRect outputGeometry() const;
    void setOutputRefreshRate(int rate);
    int outputRefreshRate() const;

    Qt::ScreenOrientations orientationUpdateMaskForClient(wl_client *client);

    void setClientFullScreenHint(bool value);

    QWaylandCompositor::ExtensionFlag extensions() const;

    TouchExtensionGlobal *touchExtension() { return m_touchExtension; }
    void configureTouchExtension(int flags);

    QtKeyExtensionGlobal *qtkeyExtension() { return m_qtkeyExtension; }

    InputPanel *inputPanel() const;

    bool isDragging() const;
    void sendDragMoveEvent(const QPoint &global, const QPoint &local, Surface *surface);
    void sendDragEndEvent();

    typedef void (*RetainedSelectionFunc)(QMimeData *, void *);
    void setRetainedSelectionWatcher(RetainedSelectionFunc func, void *param);
    void overrideSelection(QMimeData *data);

    bool wantsRetainedSelection() const;
    void feedRetainedSelectionData(QMimeData *data);

    void scheduleReleaseBuffer(SurfaceBuffer *screenBuffer);

private slots:

    void releaseBuffer(QPlatformScreenBuffer *screenBuffer);
    void processWaylandEvents();

private:
    QWaylandCompositor::ExtensionFlag m_extensions;

    Display *m_display;

    /* Input */
    QWaylandInputDevice *m_default_wayland_input_device;
    InputDevice *m_default_input_device;

    /* Output */
    //make this a list of the available screens
    OutputGlobal *m_output_global;
    //This one should be part of the outputs
    QPlatformScreenPageFlipper *m_pageFlipper;

    DataDeviceManager *m_data_device_manager;

    QList<Surface *> m_surfaces;
    QSet<Surface *> m_dirty_surfaces;

    /* Render state */
    uint32_t m_current_frame;
    int m_last_queued_buf;

    wl_event_loop *m_loop;

    QWaylandCompositor *m_qt_compositor;
    Qt::ScreenOrientation m_orientation;

    Surface *m_directRenderSurface;
    QOpenGLContext *m_directRenderContext;
    bool m_directRenderActive;

#ifdef QT_COMPOSITOR_WAYLAND_GL
    QWaylandGraphicsHardwareIntegration *m_graphics_hw_integration;
#endif

    //extensions
    WindowManagerServerIntegration *m_windowManagerIntegration;

    Shell *m_shell;
    OutputExtensionGlobal *m_outputExtension;
    SurfaceExtensionGlobal *m_surfaceExtension;
    SubSurfaceExtensionGlobal *m_subSurfaceExtension;
    TouchExtensionGlobal *m_touchExtension;
    QtKeyExtensionGlobal *m_qtkeyExtension;
    QScopedPointer<TextInputManager> m_textInputManager;
    QScopedPointer<InputPanel> m_inputPanel;

    static void bind_func(struct wl_client *client, void *data,
                          uint32_t version, uint32_t id);

    RetainedSelectionFunc m_retainNotify;
    void *m_retainNotifyParam;
};

}

QT_END_NAMESPACE

#endif //WL_COMPOSITOR_H