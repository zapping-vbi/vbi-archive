#ifndef VBI_H
#define VBI_H

#include <pthread.h>

#include "../common/types.h"
#include "libvbi.h"

#include "vt.h"
#include "cc.h"

#include "cache.h"

struct event_handler {
	struct event_handler *	next;
	int			event_mask;
	void 			(* handler)(vbi_event *ev, void *user_data);
	void *			user_data;
};

struct vbi
{
    struct cache *cache;

	int			quit; // stoopid


	vbi_network		network;
	vbi_weblink		link[2];

	int			brightness;
	int			contrast;

	struct teletext		vt;
	struct caption		cc;

	pthread_mutex_t		event_mutex;
	int			event_mask;
	struct event_handler *	handlers;
	struct event_handler *	next_handler;

    // sliced data source
    void *fifo;
};

static inline void
vbi_send_event(struct vbi *vbi, vbi_event *ev)
{
	struct event_handler *eh;

	pthread_mutex_lock(&vbi->event_mutex);

	for (eh = vbi->handlers; eh; eh = vbi->next_handler) {
		vbi->next_handler = eh->next;

		if (eh->event_mask & ev->type)
			eh->handler(ev, eh->user_data);
	}

	pthread_mutex_unlock(&vbi->event_mutex);
}

extern void		vbi_transp_colourmap(struct vbi *vbi, attr_rgba *d, attr_rgba *s, int entries);

#endif /* VBI_H */
