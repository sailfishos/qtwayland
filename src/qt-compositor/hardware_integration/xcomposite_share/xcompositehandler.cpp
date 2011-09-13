#include "xcompositehandler.h"

#include "wayland-xcomposite-server-protocol.h"

#include "xcompositebuffer.h"
#include <X11/extensions/Xcomposite.h>

XCompositeHandler::XCompositeHandler(Wayland::Compositor *compositor, Display *display, QWindow *window)
    : mCompositor(compositor)
    , mwindow(window)
    , mDisplay(display)
{
    mFakeRootWidget = new QWindow(mCompositor->window());
    mFakeRootWidget->setGeometry(QRect(-1,-1,1,1));
    int composite_event_base, composite_error_base;
    if (XCompositeQueryExtension(mDisplay, &composite_event_base, &composite_error_base)) {

    } else {
        qFatal("XComposite required");
    }
}

void XCompositeHandler::createBuffer(struct wl_client *client, uint32_t id, Window window, const QSize &size, struct wl_visual *visual)
{
    XCompositeBuffer *buffer = new XCompositeBuffer(mCompositor, window, size, visual);
    Wayland::addClientResource(client,&buffer->base()->resource,
                               id,&wl_buffer_interface,
                               &XCompositeBuffer::buffer_interface,
                               XCompositeBuffer::delete_resource);
}

void XCompositeHandler::send_root_information(struct wl_client *client, struct wl_object *global, uint32_t version)
{
    Q_UNUSED(version);
    XCompositeHandler *handler = Wayland::wayland_cast<XCompositeHandler *>(global);
    const char *displayString = XDisplayString(handler->mDisplay);
    wl_client_post_event(client, global, WL_XCOMPOSITE_ROOT, displayString, handler->mFakeRootWidget->winId());
}

void XCompositeHandler::create_buffer(struct wl_client *client,
                      struct wl_xcomposite *xcomposite,
                      uint32_t id,
                      uint32_t x_window,
                      int width,
                      int height,
                      struct wl_visual *visual)
{
    Window window = (Window)x_window;
    XCompositeHandler *that = reinterpret_cast<XCompositeHandler *>(xcomposite);
    that->createBuffer(client, id, window, QSize(width,height),visual);
}
