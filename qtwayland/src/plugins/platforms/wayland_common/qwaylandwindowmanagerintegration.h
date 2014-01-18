/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
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

#ifndef QWAYLANDWINDOWMANAGERINTEGRATION_H
#define QWAYLANDWINDOWMANAGERINTEGRATION_H

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include "wayland-client.h"
#include "qwaylanddisplay.h"
#include <private/qgenericunixservices_p.h>

#include "qwayland-windowmanager.h"

QT_BEGIN_NAMESPACE

class QWaylandWindow;

class QWaylandWindowManagerIntegrationPrivate;

class QWaylandWindowManagerIntegration : public QObject, public QGenericUnixServices, public QtWayland::qt_windowmanager
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QWaylandWindowManagerIntegration)
public:
    explicit QWaylandWindowManagerIntegration(QWaylandDisplay *waylandDisplay);
    virtual ~QWaylandWindowManagerIntegration();

    QByteArray desktopEnvironment() const;

    bool showIsFullScreen() const;

private:
    static void wlHandleListenerGlobal(void *data, wl_registry *registry, uint32_t id,
                                       const QString &interface, uint32_t version);

    QScopedPointer<QWaylandWindowManagerIntegrationPrivate> d_ptr;

    void windowmanager_hints(int32_t showIsFullScreen) Q_DECL_OVERRIDE;
    void windowmanager_quit() Q_DECL_OVERRIDE;

    void openUrl_helper(const QUrl &url);
};

QT_END_NAMESPACE

#endif // QWAYLANDWINDOWMANAGERINTEGRATION_H