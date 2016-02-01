/*
 * libwebsockets - small server side websockets and web server implementation
 *
 * Copyright (C) 2010-2014 Andy Green <andy@warmcat.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation:
 *  version 2.1 of the License.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA  02110-1301  USA
 */

#include "private-libwebsockets.h"

void lws_feature_status_libev(struct lws_context_creation_info *info)
{
	if (info->options & LWS_SERVER_OPTION_LIBEV)
		lwsl_notice("libev support compiled in and enabled\n");
	else
		lwsl_notice("libev support compiled in but disabled\n");
}

static void
lws_accept_cb(uv_poll_t *watcher, int status, int revents)
{
	struct lws_io_watcher *lws_io = container_of(watcher,
						struct lws_io_watcher, watcher);
	struct lws_context *context = lws_io->context;
	struct lws_pollfd eventfd;

    if (status < 0/*revents & EV_ERROR*/)
        return;

    eventfd.fd = watcher->io_watcher.fd;
	eventfd.events = 0;
    eventfd.revents = 0;//EV_NONE;
    if (revents & UV_READABLE) {
		eventfd.events |= LWS_POLLIN;
		eventfd.revents |= LWS_POLLIN;
	}
    if (revents & UV_WRITABLE) {
		eventfd.events |= LWS_POLLOUT;
		eventfd.revents |= LWS_POLLOUT;
	}
	lws_service_fd(context, &eventfd);
}

LWS_VISIBLE void
lws_sigint_cb(uv_loop_t *loop, uv_signal_t *watcher, int revents)
{
    //ev_break(loop, EVBREAK_ALL);
}

LWS_VISIBLE int
lws_sigint_cfg(struct lws_context *context, int use_ev_sigint,
	       lws_ev_signal_cb* cb)
{
	context->use_ev_sigint = use_ev_sigint;
	if (cb)
		context->lws_ev_sigint_cb = cb;
	else
		context->lws_ev_sigint_cb = &lws_sigint_cb;

	return 0;
}

LWS_VISIBLE int
lws_initloop(struct lws_context *context, uv_loop_t *loop)
{
    //uv_signal_t *w_sigint = &context->w_sigint.watcher;
    uv_poll_t *w_accept = &context->w_accept.watcher;
    //const char * backend_name;
	int status = 0;
    //int backend;
    int m = 0; /* !!! TODO add pt support */

	if (!loop)
        loop = uv_default_loop();

	context->io_loop = loop;

	/*
	 * Initialize the accept w_accept with the listening socket
	 * and register a callback for read operations
	 */
    uv_poll_init(context->io_loop, w_accept, context->pt[m].lserv_fd);
    uv_poll_start(w_accept, UV_READABLE, lws_accept_cb);
    //ev_io_init(w_accept, lws_accept_cb, context->pt[m].lserv_fd, UV_READABLE);
    //ev_io_start(context->io_loop,w_accept);

	/* Register the signal watcher unless the user says not to */
	if (context->use_ev_sigint) {
        //ev_signal_init(w_sigint, context->lws_ev_sigint_cb, SIGINT);
        //ev_signal_start(context->io_loop,w_sigint);
	}
    /*backend = ev_backend(loop);

	switch (backend) {
	case EVBACKEND_SELECT:
		backend_name = "select";
		break;
	case EVBACKEND_POLL:
		backend_name = "poll";
		break;
	case EVBACKEND_EPOLL:
		backend_name = "epoll";
		break;
	case EVBACKEND_KQUEUE:
		backend_name = "kqueue";
		break;
	case EVBACKEND_DEVPOLL:
		backend_name = "/dev/poll";
		break;
	case EVBACKEND_PORT:
		backend_name = "Solaris 10 \"port\"";
		break;
	default:
		backend_name = "Unknown libev backend";
		break;
	}

    lwsl_notice(" libev backend: %s\n", backend_name);*/

	return status;
}

LWS_VISIBLE void
lws_libev_accept(struct lws *new_wsi, int accept_fd)
{
	struct lws_context *context = lws_get_context(new_wsi);
    uv_poll_t *r = &new_wsi->w_read.watcher;
    //uv_poll_t *w = &new_wsi->w_write.watcher;

	if (!LWS_LIBEV_ENABLED(context))
		return;

	new_wsi->w_read.context = context;
	new_wsi->w_write.context = context;
    uv_poll_init(context->io_loop, r, accept_fd);
    //ev_io_init(r, lws_accept_cb, accept_fd, UV_READABLE);
    //ev_io_init(w, lws_accept_cb, accept_fd, UV_WRITABLE);
}

LWS_VISIBLE void
lws_libev_io(struct lws *wsi, int flags)
{
	struct lws_context *context = lws_get_context(wsi);
    int current_events = wsi->w_read.watcher.io_watcher.pevents & (UV_READABLE | UV_WRITABLE);

	if (!LWS_LIBEV_ENABLED(context))
		return;

	if (!context->io_loop)
		return;

	assert((flags & (LWS_EV_START | LWS_EV_STOP)) &&
	       (flags & (LWS_EV_READ | LWS_EV_WRITE)));

	if (flags & LWS_EV_START) {
        if (flags & LWS_EV_WRITE)
            current_events |= UV_WRITABLE;
            //ev_io_start(context->io_loop, &wsi->w_write.watcher);
        if (flags & LWS_EV_READ)
            current_events |= UV_READABLE;
            //ev_io_start(context->io_loop, &wsi->w_read.watcher);

        uv_poll_start(&wsi->w_read.watcher, current_events, lws_accept_cb);
	} else {
        if (flags & LWS_EV_WRITE)
            current_events &= ~UV_WRITABLE;
            //ev_io_stop(context->io_loop, &wsi->w_write.watcher);
        if (flags & LWS_EV_READ)
            current_events &= ~UV_READABLE;
            //ev_io_stop(context->io_loop, &wsi->w_read.watcher);

        if (!(current_events & (UV_READABLE | UV_WRITABLE)))
            uv_poll_stop(&wsi->w_read.watcher);
        else
            uv_poll_start(&wsi->w_read.watcher, current_events, lws_accept_cb);
	}
}

LWS_VISIBLE int
lws_libev_init_fd_table(struct lws_context *context)
{
	if (!LWS_LIBEV_ENABLED(context))
		return 0;

	context->w_accept.context = context;
	context->w_sigint.context = context;

	return 1;
}

LWS_VISIBLE void
lws_libev_run(const struct lws_context *context)
{
	if (context->io_loop && LWS_LIBEV_ENABLED(context))
        uv_run(context->io_loop, 0);
}
