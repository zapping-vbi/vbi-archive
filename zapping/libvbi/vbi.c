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

#include <pthread.h>
#include "../common/fifo.h"
#include "v4lx.h"
#include "sliced.h"

int
vbi_add_handler(struct vbi *vbi, int event_mask, void *handler, void *data)
{
    struct vbi_client *cl;

    if (!(cl = malloc(sizeof(*cl))))
	return -1;

	cl->event_mask = event_mask;
	vbi->event_mask |= event_mask;

    cl->handler = handler;
    cl->data = data;
    dl_insert_last(vbi->clients, cl->node);
    return 0;
}

void
vbi_del_handler(struct vbi *vbi, void *handler, void *data)
{
    struct vbi_client *cl;

    for (cl = (void *) vbi->clients->first; cl->node->next; cl = (void *) cl->node->next)
	if (cl->handler == handler && cl->data == data)
	{
	    dl_remove(cl->node);
	    break;
	}


	vbi->event_mask = 0;

    for (cl = (void *) vbi->clients->first; cl->node->next; cl = (void *) cl->node->next)
	vbi->event_mask |= cl->event_mask;

    return;
}

#define SLICED_TELETEXT_B	(SLICED_TELETEXT_B_L10_625 | SLICED_TELETEXT_B_L25_625)
#define SLICED_CAPTION		(SLICED_CAPTION_625_F1 | SLICED_CAPTION_625 \
				 | SLICED_CAPTION_525_F1 | SLICED_CAPTION_525)

void *
vbi_mainloop(void *p)
{
	struct vbi *vbi = p;
	vbi_sliced *s;
	buffer *b;
	int items;

	while (!vbi->quit) {
		b = wait_full_buffer(vbi->fifo);

		if (!b) {
			fprintf(stderr, "Oops! VBI read error and "
					"I don't know how to handle it.\n");
			exit(EXIT_FAILURE);
		}

		/* call out_of_sync if timestamp delta > 1.5 * frame period */

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
// #define SAMPLE "libvbi/samples/t4-arte"

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

	init_callback_fifo(&filter.fifo,
		wait_full_filter, send_empty_filter, NULL, NULL, 0, 0);

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











struct vbi *
vbi_open(char *vbi_name, struct cache *ca, int fine_tune)
{
    struct vbi *vbi;
    extern void open_vbi(void);
    
    

    if (!(vbi = calloc(1, sizeof(*vbi))))
    {
//	error("out of memory");
	goto fail1;
    }

    vbi->fifo = open_vbi_v4lx(vbi_name);

    if (!vbi->fifo)
    {
//	ioerror(vbi_name);
	goto fail2;
    }


	add_filter(vbi);


    vbi->cache = ca;

    dl_init(vbi->clients);

	vbi_init_teletext(&vbi->vt);
	vbi_init_caption(&vbi->cc);

	vbi->vt.max_level = VBI_LEVEL_2p5;

    out_of_sync(vbi);


    return vbi;

fail2:
    free(vbi);
fail1:
    return 0;
}

void
vbi_close(struct vbi *vbi)
{

    if (vbi->cache)
	vbi->cache->op->close(vbi->cache);

	remove_filter(vbi);

    close_vbi_v4lx(vbi->fifo);

    free(vbi);
}
