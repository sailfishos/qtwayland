/****************************************************************************
**
** Copyright (C) 2013 Klarälvdalens Datakonsult AB (KDAB).
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

#include "qwlinputmethod_p.h"

#include "qwlcompositor_p.h"
#include "qwlinputdevice_p.h"
#include "qwlinputmethodcontext_p.h"
#include "qwlinputpanel_p.h"
#include "qwlkeyboard_p.h"
#include "qwltextinput_p.h"

QT_BEGIN_NAMESPACE

namespace QtWayland {

InputMethod::InputMethod(Compositor *compositor, InputDevice *seat)
    : QtWaylandServer::wl_input_method(seat->compositor()->wl_display(), 1)
    , m_compositor(compositor)
    , m_seat(seat)
    , m_resource(0)
    , m_textInput()
    , m_context()
{
    connect(seat->keyboardDevice(), SIGNAL(focusChanged(Surface*)), this, SLOT(focusChanged(Surface*)));
}

InputMethod::~InputMethod()
{
}

void InputMethod::activate(TextInput *textInput)
{
    if (!m_resource) {
        qDebug("Cannot activate (no input method running).");
        return;
    }

    if (m_textInput) {
        Q_ASSERT(m_textInput != textInput);
        m_textInput->deactivate(this);
    }
    m_textInput = textInput;
    m_context = new InputMethodContext(m_resource->client(), textInput);

    send_activate(m_resource->handle, m_context->resource()->handle);

    m_compositor->inputPanel()->setFocus(textInput->focus());
    m_compositor->inputPanel()->setCursorRectangle(textInput->cursorRectangle());
    m_compositor->inputPanel()->setInputPanelVisible(textInput->inputPanelVisible());
}

void InputMethod::deactivate()
{
    if (!m_resource) {
        qDebug("Cannot deactivate (no input method running).");
        return;
    }

    send_deactivate(m_resource->handle, m_context->resource()->handle);
    m_textInput = 0;
    m_context = 0;

    m_compositor->inputPanel()->setFocus(0);
    m_compositor->inputPanel()->setCursorRectangle(QRect());
    m_compositor->inputPanel()->setInputPanelVisible(false);
}

void InputMethod::focusChanged(Surface *surface)
{
    if (!m_textInput)
        return;

    if (!surface || m_textInput->focus() != surface) {
        m_textInput->deactivate(this);
    }
}

bool InputMethod::isBound() const
{
    return m_resource != 0;
}

InputMethodContext *InputMethod::context() const
{
    return m_context;
}

TextInput *InputMethod::textInput() const
{
    return m_textInput;
}

void InputMethod::input_method_bind_resource(Resource *resource)
{
    if (m_resource) {
        wl_resource_post_error(resource->handle, WL_DISPLAY_ERROR_INVALID_OBJECT, "interface object already bound");
        wl_resource_destroy(resource->handle);
        return;
    }

    m_resource = resource;
}

void InputMethod::input_method_destroy_resource(Resource *resource)
{
    Q_ASSERT(resource == m_resource);
    m_resource = 0;
}

} // namespace QtWayland

QT_END_NAMESPACE
