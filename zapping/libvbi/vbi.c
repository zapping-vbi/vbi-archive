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
#include "misc.h"
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
vbi_add_handler(struct vbi *vbi, void *handler, void *data)
{
    struct vbi_client *cl;

    if (not(cl = malloc(sizeof(*cl))))
	return -1;
    cl->handler = handler;
    cl->data = data;
    dl_insert_last(vbi->clients, cl->node);
    return 0;
}

void
vbi_del_handler(struct vbi *vbi, void *handler, void *data)
{
    struct vbi_client *cl;

    for (cl = $ vbi->clients->first; cl->node->next; cl = $ cl->node->next)
	if (cl->handler == handler && cl->data == data)
	{
	    dl_remove(cl->node);
	    break;
	}
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
				vbi_packet(vbi, s->data);
/*
			if (s->id & SLICED_CAPTION)
				vbi_caption(vbi, s->line, s->data);
			if (s->id & SLICED_VPS)
				vbi_vps(vbi, s->data);
*/

			s++;
			items--;
		}

		send_empty_buffer(vbi->fifo, b);
	}

	return NULL;
}



/* Quick Hack(tm) to read from a sample stream */

//static char *sample_file = "libvbi/samples/t2-br";
static char *sample_file = NULL; // disabled
static FILE *sample_fd;

void
sample_beta(struct vbi *vbi)
{
	unsigned char wst[42];
	char buf[256];
//	double dt;
	int index, line;
	int items;
	int i;

	if (feof(sample_fd)) {
		rewind(sample_fd);
		printf("Rewind sample stream\n");
	}

	{
		fgets(buf, 255, sample_fd);

		/* usually 0.04 (1/25) */
//		dt = strtod(buf, NULL);

		items = fgetc(sample_fd);

//		printf("%8.6f %d:\n", dt, items);

		for (i = 0; i < items; i++) {
			index = fgetc(sample_fd);
			line = fgetc(sample_fd);
			line += 256 * fgetc(sample_fd);

			if (index != 0) {
				printf("Oops! Confusion in vbi.c/sample_beta()\n");
				// index: 0 == Teletext
				exit(EXIT_FAILURE);
			}

			fread(wst, 1, 42, sample_fd);

			vbi_packet(vbi, wst);
		}
	}
}


struct vbi *
vbi_open(char *vbi_name, struct cache *ca, int fine_tune)
{
    struct vbi *vbi;
    extern void open_vbi(void);
    
    

    if (not(vbi = calloc(1, sizeof(*vbi)))) // must clear for reset_magazines
    {
	error("out of memory");
	goto fail1;
    }

    vbi->fifo = open_vbi_v4lx(vbi_name);

    if (!vbi->fifo)
    {
//	ioerror(vbi_name);
	goto fail2;
    }

    vbi->max_level = VBI_LEVEL_2p5;

    vbi->cache = ca;

    dl_init(vbi->clients);
    out_of_sync(vbi);
    reset_magazines(vbi);

    if (sample_file)
	if (!(sample_fd = fopen(sample_file, "r")))
	    printf("Cannot open %s: %s\n", sample_file, strerror(errno));

    return vbi;

fail2:
    free(vbi);
fail1:
    return 0;
}

void
vbi_close(struct vbi *vbi)
{
    reset_magazines(vbi);

    if (vbi->cache)
	vbi->cache->op->close(vbi->cache);

    close_vbi_v4lx(vbi->fifo);

    if (sample_fd)
	fclose(sample_fd);
    sample_fd = NULL;

    free(vbi);
}


