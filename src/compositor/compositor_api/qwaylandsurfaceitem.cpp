/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
** Copyright (c) 2014 - 2019 Jolla Ltd.
** Copyright (c) 2020 Open Mobile Platform LLC.
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

#include "qwaylandsurfaceitem.h"
#include "qwaylandquicksurface.h"
#include <QtCompositor/qwaylandcompositor.h>
#include <QtCompositor/qwaylandinput.h>

#include "wayland_wrapper/qwlsurface_p.h"

#include <QtGui/QKeyEvent>
#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>

#include <QtQuick/QSGSimpleTextureNode>
#include <QtQuick/QQuickWindow>

#include <QtCore/QMutexLocker>
#include <QtCore/QMutex>

QT_BEGIN_NAMESPACE

QMutex *QWaylandSurfaceItem::mutex = 0;

class QWaylandSurfaceTextureNode : public QSGGeometryNode
{
public:
    QWaylandSurfaceTextureNode()
        : m_geometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 0)
    {
        QSGGeometryNode::setGeometry(&m_geometry);
        setMaterial(&m_material);
    }

    virtual void setTexture(QSGTexture *texture)
    {
        m_material.setTexture(texture);
    }

    void setGeometry(const QSizeF &itemSize, const QSize &surfaceSize, const QRegion &rectangles, bool yInverted)
    {
        m_geometry.allocate(rectangles.rectCount() * 6);
        QSGGeometry::TexturedPoint2D *vertices = static_cast<QSGGeometry::TexturedPoint2D *>(m_geometry.vertexData());

        for (const QRect &rectangle : rectangles.rects()) {
            struct Rect { qreal left, top, right, bottom; };
            const Rect texture =  {
                qreal(rectangle.left()) / surfaceSize.width(),
                qreal(rectangle.top()) / surfaceSize.height(),
                qreal(rectangle.right() + 1) / surfaceSize.width(),
                qreal(rectangle.bottom() + 1) / surfaceSize.height(),
            };

            const Rect position = {
                texture.left * itemSize.width(),
                (yInverted ? texture.bottom : texture.top) * itemSize.height(),
                texture.right * itemSize.width(),
                (yInverted ? texture.top : texture.bottom) * itemSize.height()
            };

            vertices[1].set(position.left, position.bottom, texture.left, texture.bottom);  // Bottom left.
            vertices[2].set(position.left, position.top, texture.left, texture.top);        // Top left.
            vertices[3].set(position.right, position.bottom, texture.right, texture.bottom);// Bottom right.
            vertices[4].set(position.right, position.top, texture.right, texture.top);      // Top right.

            vertices[0] = vertices[1];
            vertices[5] = vertices[4];

            vertices += 6;
        }

        markDirty(QSGNode::DirtyGeometry);
    }

private:
    QSGTextureMaterial m_material;
    QSGGeometry m_geometry;
};

class QWaylandSurfaceOpaqueTextureNode : public QWaylandSurfaceTextureNode
{
public:
    QWaylandSurfaceOpaqueTextureNode()
    {
        setOpaqueMaterial(&m_opaqueMaterial);
    }

    void setTexture(QSGTexture *texture) override
    {
        QWaylandSurfaceTextureNode::setTexture(texture);

        m_opaqueMaterial.setTexture(texture);
        m_opaqueMaterial.setFlag(QSGMaterial::Blending, false);
    }

private:
    QSGOpaqueTextureMaterial m_opaqueMaterial;
};

class QWaylandSurfaceNode : public QSGNode
{
public:
    QWaylandSurfaceNode()
    {
    }

    void setTexture(QSGTexture *texture)
    {
        m_translucentNode.setTexture(texture);
        m_opaqueNode.setTexture(texture);

        m_translucentNode.markDirty(QSGNode::DirtyMaterial);
        m_opaqueNode.markDirty(QSGNode::DirtyMaterial);
    }

    void setGeometry(
            const QSizeF &itemSize,
            const QSize &surfaceSize,
            const QRegion &opaqueRectangles,
            bool yInverted)
    {
        if (m_itemSize == itemSize && m_surfaceSize == surfaceSize && m_opaqueRectangles == opaqueRectangles) {
            return;
        }

        m_itemSize = itemSize;
        m_surfaceSize = surfaceSize;
        m_opaqueRectangles = opaqueRectangles;

        QRegion translucentRectangles(0, 0, surfaceSize.width(), surfaceSize.height());
        translucentRectangles -= opaqueRectangles;

        removeAllChildNodes();

        if (!translucentRectangles.isEmpty()) {
            appendChildNode(&m_translucentNode);

            m_translucentNode.setGeometry(itemSize, surfaceSize, translucentRectangles, yInverted);
        }

        if (!opaqueRectangles.isEmpty()) {
            appendChildNode(&m_opaqueNode);

            m_opaqueNode.setGeometry(itemSize, surfaceSize, opaqueRectangles, yInverted);
        }
    }

private:
    QWaylandSurfaceTextureNode m_translucentNode;
    QWaylandSurfaceOpaqueTextureNode m_opaqueNode;
    QRegion m_opaqueRectangles;
    QSizeF m_itemSize;
    QSize m_surfaceSize;

};

class QWaylandSurfaceTextureProvider : public QSGTextureProvider
{
public:
    QWaylandSurfaceTextureProvider() : t(0) { }

    QSGTexture *texture() const Q_DECL_OVERRIDE
    {
        if (t)
            t->setFiltering(smooth ? QSGTexture::Linear : QSGTexture::Nearest);
        return t;
    }

    bool smooth;
    QSGTexture *t;
};

QWaylandSurfaceItem::QWaylandSurfaceItem(QWaylandQuickSurface *surface, QQuickItem *parent)
    : QQuickItem(parent)
    , QWaylandSurfaceView(surface)
    , m_provider(0)
    , m_paintEnabled(true)
    , m_touchEventsEnabled(false)
    , m_resizeSurfaceToItem(false)
    , m_newTexture(false)

{
    if (!mutex)
        mutex = new QMutex;

    setFlag(ItemHasContents);

    update();

    if (surface) {
        setWidth(surface->size().width());
        setHeight(surface->size().height());
    }

    setSmooth(true);

    setAcceptedMouseButtons(Qt::LeftButton | Qt::MiddleButton | Qt::RightButton |
        Qt::ExtraButton1 | Qt::ExtraButton2 | Qt::ExtraButton3 | Qt::ExtraButton4 |
        Qt::ExtraButton5 | Qt::ExtraButton6 | Qt::ExtraButton7 | Qt::ExtraButton8 |
        Qt::ExtraButton9 | Qt::ExtraButton10 | Qt::ExtraButton11 |
        Qt::ExtraButton12 | Qt::ExtraButton13);
    setAcceptHoverEvents(true);
    if (surface) {
        connect(surface, &QWaylandSurface::mapped, this, &QWaylandSurfaceItem::surfaceMapped);
        connect(surface, &QWaylandSurface::unmapped, this, &QWaylandSurfaceItem::surfaceUnmapped);
        connect(surface, &QWaylandSurface::surfaceDestroyed, this, &QWaylandSurfaceItem::surfaceDestroyed);
        connect(surface, &QWaylandSurface::parentChanged, this, &QWaylandSurfaceItem::parentChanged);
        connect(surface, &QWaylandSurface::sizeChanged, this, &QWaylandSurfaceItem::updateSize);
        connect(surface, &QWaylandSurface::configure, this, &QWaylandSurfaceItem::updateBuffer);
        connect(surface, &QWaylandSurface::redraw, this, &QQuickItem::update);
    }
    connect(this, &QWaylandSurfaceItem::widthChanged, this, &QWaylandSurfaceItem::updateSurfaceSize);
    connect(this, &QWaylandSurfaceItem::heightChanged, this, &QWaylandSurfaceItem::updateSurfaceSize);


    m_yInverted = surface ? surface->isYInverted() : true;
    emit yInvertedChanged();
}

QWaylandSurfaceItem::~QWaylandSurfaceItem()
{
    QMutexLocker locker(mutex);
    if (m_provider)
        m_provider->deleteLater();
}

bool QWaylandSurfaceItem::isYInverted() const
{
    return m_yInverted;
}

QSGTextureProvider *QWaylandSurfaceItem::textureProvider() const
{
    if (!m_provider)
        m_provider = new QWaylandSurfaceTextureProvider();
    return m_provider;
}

void QWaylandSurfaceItem::mousePressEvent(QMouseEvent *event)
{
    if (!surface())
        return;

    if (!surface()->inputRegionContains(event->pos())) {
        event->ignore();
        return;
    }

    QWaylandInputDevice *inputDevice = compositor()->defaultInputDevice();
    if (inputDevice->mouseFocus() != this)
        inputDevice->setMouseFocus(this, event->localPos(), event->windowPos());
    inputDevice->sendMousePressEvent(event->button(), event->localPos(), event->windowPos());
}

void QWaylandSurfaceItem::mouseMoveEvent(QMouseEvent *event)
{
    if (surface()) {
        QWaylandInputDevice *inputDevice = compositor()->defaultInputDevice();
        inputDevice->sendMouseMoveEvent(this, event->localPos(), event->windowPos());
    }
}

void QWaylandSurfaceItem::mouseReleaseEvent(QMouseEvent *event)
{
    if (surface()) {
        QWaylandInputDevice *inputDevice = compositor()->defaultInputDevice();
        inputDevice->sendMouseReleaseEvent(event->button(), event->localPos(), event->windowPos());
    }
}

void QWaylandSurfaceItem::hoverEnterEvent(QHoverEvent *event)
{
    if (surface()) {
        QWaylandInputDevice *inputDevice = compositor()->defaultInputDevice();
        inputDevice->sendMouseMoveEvent(this, event->pos());
    }
}

void QWaylandSurfaceItem::hoverMoveEvent(QHoverEvent *event)
{
    if (surface()) {
        QWaylandInputDevice *inputDevice = compositor()->defaultInputDevice();
        inputDevice->sendMouseMoveEvent(this, event->pos());
    }
}

void QWaylandSurfaceItem::hoverLeaveEvent(QHoverEvent *event)
{
    if (surface()) {
        QWaylandInputDevice *inputDevice = compositor()->defaultInputDevice();
        inputDevice->sendMouseMoveEvent(this, event->pos());
    }
}

void QWaylandSurfaceItem::wheelEvent(QWheelEvent *event)
{
    if (surface()) {
        if (!surface()->inputRegionContains(event->pos())) {
            event->ignore();
            return;
        }

        QWaylandInputDevice *inputDevice = compositor()->defaultInputDevice();
        inputDevice->sendMouseWheelEvent(event->orientation(), event->delta());
    }
}

void QWaylandSurfaceItem::keyPressEvent(QKeyEvent *event)
{
    if (surface()) {
        QWaylandInputDevice *inputDevice = compositor()->defaultInputDevice();
        inputDevice->sendFullKeyEvent(event);
    }
}

void QWaylandSurfaceItem::keyReleaseEvent(QKeyEvent *event)
{
    if (surface() && hasFocus()) {
        QWaylandInputDevice *inputDevice = compositor()->defaultInputDevice();
        inputDevice->sendFullKeyEvent(event);
    }
}

void QWaylandSurfaceItem::touchEvent(QTouchEvent *event)
{
    if (m_touchEventsEnabled) {
        QWaylandInputDevice *inputDevice = compositor()->defaultInputDevice();

        QPoint pointPos;
        const QList<QTouchEvent::TouchPoint> &points = event->touchPoints();
        if (!points.isEmpty())
            pointPos = points.at(0).pos().toPoint();

        if (event->type() == QEvent::TouchBegin && !surface()->inputRegionContains(pointPos)) {
            event->ignore();
            return;
        }

        event->accept();
        if (inputDevice->mouseFocus() != this) {
            inputDevice->setMouseFocus(this, pointPos, pointPos);
        }
        inputDevice->sendFullTouchEvent(event);
    } else {
        event->ignore();
    }
}

void QWaylandSurfaceItem::takeFocus()
{
    setFocus(true);

    if (!surface())
        return;

    QWaylandInputDevice *inputDevice = compositor()->defaultInputDevice();
    inputDevice->setKeyboardFocus(surface());
}

void QWaylandSurfaceItem::surfaceMapped()
{
    update();
}

void QWaylandSurfaceItem::surfaceUnmapped()
{
    update();
}

void QWaylandSurfaceItem::parentChanged(QWaylandSurface *newParent, QWaylandSurface *oldParent)
{
    Q_UNUSED(oldParent);

    if (newParent) {
        setPaintEnabled(true);
        setVisible(true);
        setOpacity(1);
        setEnabled(true);
    }
}

void QWaylandSurfaceItem::updateSize()
{
    if (surface()) {
        setSize(surface()->size());
    }
}

void QWaylandSurfaceItem::updateSurfaceSize()
{
    if (surface() && m_resizeSurfaceToItem) {
        surface()->requestSize(QSize(width(), height()));
    }
}

void QWaylandSurfaceItem::setPos(const QPointF &pos)
{
    setPosition(pos);
}

QPointF QWaylandSurfaceItem::pos() const
{
    return position();
}

/*!
    \qmlproperty bool QtWayland::QWaylandSurfaceItem::paintEnabled

    If this property is true, the \l item is hidden, though the texture
    will still be updated. As opposed to hiding the \l item by
    setting \l{Item::visible}{visible} to false, setting this property to true
    will not prevent mouse or keyboard input from reaching \l item.
*/
bool QWaylandSurfaceItem::paintEnabled() const
{
    return m_paintEnabled;
}

void QWaylandSurfaceItem::setPaintEnabled(bool enabled)
{
    m_paintEnabled = enabled;
    update();
}

void QWaylandSurfaceItem::updateBuffer(bool hasBuffer)
{
    Q_UNUSED(hasBuffer)

    bool inv = m_yInverted;
    m_yInverted = surface()->isYInverted();
    if (inv != m_yInverted)
        emit yInvertedChanged();

    m_newTexture = true;
}

void QWaylandSurfaceItem::updateTexture()
{
    updateTexture(false);
}

void QWaylandSurfaceItem::updateTexture(bool changed)
{
    if (!m_provider)
        m_provider = new QWaylandSurfaceTextureProvider();

    m_provider->t = static_cast<QWaylandQuickSurface *>(surface())->texture();
    m_provider->smooth = smooth();
    if (m_newTexture || changed)
        emit m_provider->textureChanged();
    m_newTexture = false;
}

QSGNode *QWaylandSurfaceItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    QWaylandSurface * const surface = this->surface();
    QtWayland::Surface * const handle = surface && surface->isMapped() ? surface->handle() : nullptr;

    if (!handle || !m_provider->t || !m_paintEnabled) {
        delete oldNode;
        return 0;
    }

    QWaylandSurfaceNode *node = static_cast<QWaylandSurfaceNode *>(oldNode);

    if (!node)
        node = new QWaylandSurfaceNode();

    // Surface textures come by default with the OpenGL coordinate system, which is inverted relative
    // to the QtQuick one. So we're dealing with a double invertion here, and if isYInverted() returns
    // true it means it is NOT inverted relative to QtQuick, while if it returns false it means it IS.
    node->setGeometry(QSize(width(), height()), handle->size(), handle->opaqueRegion(), !surface->isYInverted());
    node->setTexture(m_provider->t);

    return node;
}

void QWaylandSurfaceItem::setTouchEventsEnabled(bool enabled)
{
    if (m_touchEventsEnabled != enabled) {
        m_touchEventsEnabled = enabled;
        emit touchEventsEnabledChanged();
    }
}

void QWaylandSurfaceItem::setResizeSurfaceToItem(bool enabled)
{
    if (m_resizeSurfaceToItem != enabled) {
        m_resizeSurfaceToItem = enabled;
        emit resizeSurfaceToItemChanged();
    }
}

QT_END_NAMESPACE
