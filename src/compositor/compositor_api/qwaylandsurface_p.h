/****************************************************************************
**
** Copyright (C) 2014 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
** Copyright (C) 2014 Jolla Ltd, author: <giulio.camuffo@jollamobile.com>
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

#ifndef QWAYLANDSURFACE_P_H
#define QWAYLANDSURFACE_P_H

#include <QtCompositor/qwaylandexport.h>
#include <private/qobject_p.h>

#include <QtCompositor/private/qwlsurface_p.h>

QT_BEGIN_NAMESPACE

class QWaylandCompositor;
class QWaylandSurface;
class QWaylandSurfaceView;
class QWaylandSurfaceInterface;

class Q_COMPOSITOR_EXPORT QWaylandSurfacePrivate : public QObjectPrivate, public QtWayland::Surface
{
    Q_DECLARE_PUBLIC(QWaylandSurface)
public:
    QWaylandSurfacePrivate(wl_client *wlClient, quint32 id, int version, QWaylandCompositor *compositor, QWaylandSurface *surface);
    void setType(QWaylandSurface::WindowType type);
    void setTitle(const QString &title);
    void setClassName(const QString &className);

    bool closing;
    int refCount;

    QWaylandClient *client;

    QWaylandSurface::WindowType windowType;
    QList<QWaylandSurfaceView *> views;
    QList<QWaylandSurfaceInterface *> interfaces;
};

QT_END_NAMESPACE

#endif
