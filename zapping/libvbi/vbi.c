#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <sys/ioctl.h>
#include "os.h"
#include "vt.h"
#include "vbi.h"
#include "hamm.h"
#include "lang.h"
#include "export.h"
#include "tables.h"
#include "libvbi.h"
#include "trigger.h"

#include "../common/fifo.h"
#include "../common/math.h"
#include "v4lx.h"
#include "sliced.h"

/*
 *  When event_mask == 0, remove previously added handler,
 *  otherwise add handler or change its event_mask.
 *
 *  The event_mask can be a combination of VBI_EVENT_*. When no handler
 *  exists for an event, decoding the respective data (eg.
 *  VBI_EVENT_PAGE, loading the cache with Teletext pages) is disabled.
 *  Mind the vbi fifo producer which isn't controlled from here.
 *
 *  Safe to call from a handler or thread other than vbi_mainloop.
 *  Returns boolean success.
 */
int
vbi_event_handler(struct vbi *vbi, int event_mask,
	void (* handler)(vbi_event *, void *), void *user_data) 
{
	struct event_handler *eh, **ehp;
	int found = 0, mask = 0, was_locked;
	int activate;

	/* If was_locked we're a handler, no recursion. */
	was_locked = pthread_mutex_trylock(&vbi->event_mutex);

	ehp = &vbi->handlers;

	while ((eh = *ehp)) {
		if (eh->handler == handler) {
			found = 1;

			if (!event_mask) {
				*ehp = eh->next;

				if (vbi->next_handler == eh)
					vbi->next_handler = eh->next;
						/* in event send loop */
				free(eh);

				continue;
			} else
				eh->event_mask = event_mask;
		}

		mask |= eh->event_mask;	
		ehp = &eh->next;
	}

	if (!found && event_mask) {
		if (!(eh = calloc(1, sizeof(*eh))))
			return 0;

		eh->event_mask = event_mask;
		mask |= event_mask;

		eh->handler = handler;
		eh->user_data = user_data;

		*ehp = eh;
	}

	activate = mask & ~vbi->event_mask;

	if (activate & VBI_EVENT_PAGE)
		vbi_init_teletext(&vbi->vt);
	if (activate & VBI_EVENT_CAPTION)
		vbi_init_caption(vbi);
	if (activate & VBI_EVENT_NETWORK)
		memset(&vbi->network, 0, sizeof(vbi->network));

	vbi->event_mask = mask;

	if (!was_locked)
		pthread_mutex_unlock(&vbi->event_mutex);

	return 1;
}

#define SLICED_TELETEXT_B	(SLICED_TELETEXT_B_L10_625 | SLICED_TELETEXT_B_L25_625)
#define SLICED_CAPTION		(SLICED_CAPTION_625_F1 | SLICED_CAPTION_625 \
				 | SLICED_CAPTION_525_F1 | SLICED_CAPTION_525)

#define FIFO_DEPTH 30

void *
vbi_mainloop(void *p)
{
	struct vbi *vbi = p;
	vbi_sliced *s;
	int items;

	while (!vbi->quit) {
		buffer *b = wait_full_buffer(vbi->fifo);

		if (!b) {
			vbi_event ev;

			ev.type = VBI_EVENT_IO_ERROR;
			vbi_send_event(vbi, &ev);

			break;
		}

		if (vbi->time > 0 && (b->time - vbi->time) > 0.055) {
// fprintf(stderr, "vbi frame/s dropped at %f, D=%f\n", b->time, b->time - vbi->time);
			if (vbi->event_mask & (VBI_EVENT_PAGE | VBI_EVENT_NETWORK))
				vbi_teletext_desync(vbi);
			if (vbi->event_mask & (VBI_EVENT_CAPTION | VBI_EVENT_NETWORK))
				vbi_caption_desync(vbi);
		}

		vbi->time = b->time;

		s = (vbi_sliced *) b->data;
		items = b->used / sizeof(vbi_sliced);

		while (items) {
			if (s->id & SLICED_TELETEXT_B)
				vbi_teletext_packet(vbi, s->data);
			else if (s->id & SLICED_CAPTION)
				vbi_caption_dispatcher(vbi, s->line, s->data);
			else if (s->id & SLICED_VPS)
				vbi_vps(vbi, s->data);

			s++;
			items--;
		}

		send_empty_buffer(vbi->fifo, b);

		if (vbi->event_mask & VBI_EVENT_TRIGGER)
			vbi_deferred_trigger(vbi);

		if (1 && (rand() % 511) == 0)
			vbi_eacem_trigger(vbi,
				"<http://zapping.sourceforge.net>[n:Zapping][5450]");
	}

	return NULL;
}

/* TEST ONLY */

/* tinker here */
#define FILTER_REM 0
#define FILTER_ADD 0
#define SAMPLE "libvbi/samples/"

/* examples */
// #define FILTER_REM 0
// #define FILTER_ADD SLICED_CAPTION
// #define SAMPLE "libvbi/samples/s4"

// #define FILTER_REM SLICED_TELETEXT_B
// #define FILTER_ADD SLICED_TELETEXT_B
// #define SAMPLE "libvbi/samples/t1-swr"

// #define FILTER_* (SLICED_TELETEXT_B | SLICED_CAPTION | SLICED_VPS)

struct {
	fifo			fifo;
	buffer 			buf;

	fifo *			old_fifo;

	vbi_sliced		sliced[60];
	int			pass, add;

	FILE *			fp;
} filter;

static buffer *
wait_full_filter(fifo *f)
{
	int items, index, line;
	vbi_sliced *s, *d;
	char buf[256];
	buffer *b;
	int i;

	b = wait_full_buffer(filter.old_fifo);

	s = (vbi_sliced *) b->data;
	d = filter.sliced;
	items = b->used / sizeof(vbi_sliced);

	for (; items > 0; s++, items--)
		if (s->id & filter.pass) {
			memcpy(d, s, sizeof(vbi_sliced));
			d++;
		}

	if (feof(filter.fp)) {
		rewind(filter.fp);
		printf("Rewind sample stream\n");
	}

	fgets(buf, 255, filter.fp);

//	dt = strtod(buf, NULL);

	items = fgetc(filter.fp);

//	printf("%8.6f %d:\n", dt, items);

	for (i = 0; i < items; i++) {
		index = fgetc(filter.fp);
		line = fgetc(filter.fp);
		line += 256 * fgetc(filter.fp);

		if (index == 0) {
			d->id = SLICED_TELETEXT_B;
			d->line = line & 0xFFF;
			fread(d->data, 1, 42, filter.fp);
		} else if (index == 7) {
			d->id = SLICED_CAPTION;
			d->line = line & 0xFFF;
			fread(d->data, 1, 2, filter.fp);
		} else {
			printf("Oops! Unknown data in sample file %s\n", SAMPLE);
			exit(EXIT_FAILURE);
		}

		if (d->id & filter.add)
			d++;
	}

	filter.buf.data = (void *) filter.sliced;
	filter.buf.used = (d - filter.sliced) * sizeof(vbi_sliced);
	filter.buf.time = b->time;

	send_empty_buffer(filter.old_fifo, b);

	return &filter.buf;
}

static void
send_empty_filter(fifo *f, buffer *b)
{
}

static void
add_filter(struct vbi *vbi)
{
	if (!(FILTER_REM) && !(FILTER_ADD))
		return;

	if (!(filter.fp = fopen(SAMPLE, "r"))) {
		printf("Cannot open %s: %s\n", SAMPLE, strerror(errno));
		return;
	}

	filter.pass = FILTER_REM ^ -1;
	filter.add = FILTER_ADD;

	filter.old_fifo = vbi->fifo;

	init_callback_fifo(&filter.fifo, "vbi-filter",
		wait_full_filter, send_empty_filter, NULL, 0, 0);

	vbi->fifo = &filter.fifo;
}

static void
remove_filter(struct vbi *vbi)
{
	if (!filter.old_fifo)
		return;

	vbi->fifo = filter.old_fifo;

	filter.old_fifo = NULL;

	fclose(filter.fp);
}

static inline int
transp(int val, int brig, int cont)
{
	int r = (((val - 128) * cont) / 64) + brig;

	return saturate(r, 0, 255);
}

void
vbi_transp_colourmap(struct vbi *vbi, attr_rgba *d, attr_rgba *s, int entries)
{
	int brig, cont;
	attr_rgba colour;

	brig = saturate(vbi->brightness, 0, 255);
	cont = saturate(vbi->contrast, -128, +127);

	while (entries--) {
		colour  = transp(((*s >> 0) & 0xFF), brig, cont) << 0;
		colour |= transp(((*s >> 8) & 0xFF), brig, cont) << 8;
		colour |= transp(((*s >> 16) & 0xFF), brig, cont) << 16;
		colour |= *s & (0xFFUL << 24);
		*d++ = colour;
		s++;
	}
}

/*
 *  Brightness: 0 ... 255, default 128
 *  Contrast: -128 ... +128, default 64
 *  (maximum contrast at +/-128, zero at 0, inversion below 0)
 */
void
vbi_set_colour_level(struct vbi *vbi, int brightness, int contrast)
{
	vbi->brightness = brightness;
	vbi->contrast = contrast;

	vbi_caption_colour_level(vbi);
}

int
vbi_classify_page(struct vbi *vbi, int pgno, int *subpage, char **language)
{
	struct page_info *pi;
	int code, subc;
	char *lang;

	if (!subpage)
		subpage = &subc;
	if (!language)
		language = &lang;

	*subpage = 0;
	*language = NULL;

	if (pgno < 0x100 || pgno > 0x8FF) {
		return VBI_UNKNOWN_PAGE;
	}

	pi = vbi->vt.page_info + pgno - 0x100;
	code = pi->code;

	if (code != VBI_UNKNOWN_PAGE) {
		if (code == VBI_SUBTITLE_PAGE) {
			if (pi->language != 0xFF)
				*language = font_descriptors[pi->language].label;
		} else if (code == VBI_TOP_BLOCK || code == VBI_TOP_GROUP)
			code = VBI_NORMAL_PAGE;
		else if (code == VBI_NOT_PUBLIC || code > 0xE0)
			return VBI_UNKNOWN_PAGE;

		*subpage = pi->subcode;

		return code;
	}

	if ((pgno & 0xFF) <= 0x99) {
		*subpage = 0xFFFF;
		return VBI_NORMAL_PAGE; /* wild guess */
	}

	return VBI_UNKNOWN_PAGE;
}

struct vbi *
vbi_open(char *vbi_name, struct cache *ca, int given_fd)
{
    struct vbi *vbi;

    if (!(vbi = calloc(1, sizeof(*vbi))))
    {
//	error("out of memory");
	goto fail1;
    }

    vbi->fifo = vbi_open_v4lx(vbi_name, given_fd, 1, FIFO_DEPTH);

    if (!vbi->fifo)
    {
//	ioerror(vbi_name);
	goto fail2;
    }


	add_filter(vbi);


    vbi->cache = ca;

	pthread_mutex_init(&vbi->event_mutex, NULL);

	vbi->time = 0.0;

	vbi_init_teletext(&vbi->vt);
	vbi_init_caption(vbi);

	vbi->vt.max_level = VBI_LEVEL_2p5;

	vbi->brightness	= 128;
	vbi->contrast	= 64;

	return vbi;

fail2:
    free(vbi);
fail1:
    return 0;
}

void
vbi_close(struct vbi *vbi)
{
	vbi_event ev;

	ev.type = VBI_EVENT_CLOSE;
	vbi_send_event(vbi, &ev);

    if (vbi->cache)
	vbi->cache->op->close(vbi->cache);

	remove_filter(vbi);

    vbi_close_v4lx(vbi->fifo);

	vbi_trigger_flush(vbi);

	free(vbi);
}
