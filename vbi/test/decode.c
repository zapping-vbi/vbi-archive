/*
 *  libzvbi test
 *
 *  Copyright (C) 2004 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* $Id: decode.c,v 1.1.2.1 2004-02-25 17:27:23 mschimek Exp $ */

#undef NDEBUG

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>
#ifdef HAVE_GETOPT_LONG
#include <getopt.h>
#endif

#include <libzvbi.h>

#define printable(c) \
	((((c) & 0x7F) < 0x20 || ((c) & 0x7F) > 0x7E) ? '.' : ((c) & 0x7F))

static vbi_bool			dump_ttx	= FALSE;
static vbi_bool			dump_8301	= FALSE;
static vbi_bool			dump_8302	= FALSE;
static vbi_bool			dump_idl	= FALSE;
static vbi_bool			dump_vps	= FALSE;
static vbi_bool			dump_vps_r	= FALSE;
static vbi_bool			dump_network	= FALSE;
static vbi_bool			dump_hex	= FALSE;
static vbi_bool			dump_bin	= FALSE;

static vbi_pgno			pfc_pgno	= 0;
static unsigned int		pfc_stream	= 0;

static vbi_pfc_demux *		pc;

static void
dump_nuid			(vbi_nuid		nuid)
{
	vbi_network *n;

	if (!dump_network)
		return;

	n = vbi_network_new (nuid);

	assert (NULL != n);

	printf ("%s call_sign=%s cni=%x/%x/%x/%x/%x country=%s\n",
		n->name ? n->name : "Unknown",
		n->call_sign ? n->call_sign : "unknown",
		n->cni_vps, n->cni_8301, n->cni_8302,
		n->cni_pdc_a, n->cni_pdc_b,
		n->country_code);

	vbi_network_delete (n);
}

static void
dump_sliced_teletext_b		(const uint8_t		buffer[42])
{
	unsigned int j;

	if (dump_bin) {
		fwrite (buffer, 1, 42, stdout);
		return;
	}

	if (dump_hex) {
		for (j = 0; j < 42; ++j)
			printf ("%02x ", buffer[j]);
	}

	putchar ('>');

	for (j = 0; j < 42; ++j) {
		char c = printable (buffer[j]);

		putchar (c);
	}

	putchar ('<');
	putchar ('\n');
}

extern void
vbi_program_id_dump		(const vbi_program_id *	pi,
				 FILE *			fp);

static void
packet_8301			(const uint8_t		buffer[42],
				 unsigned int		designation)
{
	unsigned int cni;
	time_t time;
	int gmtoff;
	struct tm tm;

	if (!dump_8301)
		return;

	if (!vbi_decode_teletext_8301_cni (&cni, buffer)) {
		printf ("Hamming error in 8/30 format 1 cni\n");
		return;
	}

	if (!vbi_decode_teletext_8301_local_time (&time, &gmtoff, buffer)) {
		printf ("Hamming error in 8/30 format 1 local time\n");
		return;
	}

	printf ("Packet 8/30/%u cni=%x time=%u gmtoff=%d ",
		designation, cni, (unsigned int) time, gmtoff);

	gmtime_r (&time, &tm);

	printf ("(%4u-%02u-%02u %02u:%02u:%02u UTC)\n",
		tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec);

	if (dump_network && 0 != cni) {
		vbi_nuid nuid;

		nuid = vbi_nuid_from_cni (VBI_CNI_TYPE_8301, cni);
		dump_nuid (nuid);
	}
}

static void
packet_8302			(const uint8_t		buffer[42],
				 unsigned int		designation)
{
	unsigned int cni;
	vbi_program_id pi;

	if (!dump_8302)
		return;

	if (!vbi_decode_teletext_8302_cni (&cni, buffer)) {
		printf ("Hamming error in 8/30 format 2 cni\n");
		return;
	}

	if (!vbi_decode_teletext_8302_pdc (&pi, buffer)) {
		printf ("Hamming error in 8/30 format 2 pdc data\n");
		return;
	}

	printf ("Packet 8/30/%u cni=%x ", designation, cni);

	vbi_program_id_dump (&pi, stdout);

	dump_nuid (pi.nuid);
}

static void
page_function_clear		(vbi_pfc_demux *	pc,
				 void *			user_data,
		                 const vbi_pfc_block *	block)
{
	unsigned int i;

	printf ("PFC pgno=%x stream=%u id=%u size=%u\n",
		block->pgno, block->stream,
		block->application_id,
		block->block_size);

	if (dump_bin) {
		fwrite (block->block, sizeof (block->block[0]),
			block->block_size, stdout);
	} else {
		for (i = 0; i < block->block_size; ++i) {
			putchar (printable (block->block[i]));

			if ((i % 75) == 75)
				putchar ('\n');
		}

		putchar ('\n');
	}
}

static void
teletext			(const uint8_t		buffer[42],
				 unsigned int		line)
{
	int pmag;
	unsigned int magazine;
	unsigned int packet;

	if (pc) {
		if (!vbi_pfc_demux_demux (pc, buffer)) {
			printf ("Hamming error in packet\n");
			return;
		}
	}

	if (!(dump_ttx | dump_8301 | dump_8302 | dump_idl))
		return;

	if ((pmag = vbi_iham16p (buffer)) < 0) {
		printf ("Hamming error in pmag\n");
		return;
	}

	magazine = pmag & 7;
	if (0 == magazine)
		magazine = 8;

	packet = pmag >> 3;

	if (8 == magazine && 30 == packet) {
		int designation;

		if ((designation = vbi_iham8 (buffer[2])) < 0) {
			printf ("Hamming error in 8/30 designation\n");
			return;
		}

		if (designation >= 0 && designation <= 1) {
			packet_8301 (buffer, designation);
			return;
		}

		if (designation >= 2 && designation <= 3) {
			packet_8302 (buffer, designation);
			return;
		}
	}

	if (30 == packet || 31 == packet) {
		if (dump_idl) {
			printf ("IDL packet %u/%2u ", magazine, packet);
			dump_sliced_teletext_b (buffer);
			return;
		}
	}

	if (dump_ttx) {
		printf ("TTX L%3u %x/%2u ", line, magazine, packet);
		dump_sliced_teletext_b (buffer);
		return;
	}
}

static void
vps				(uint8_t		buffer[13],
				 unsigned int		line)
{
	if (dump_vps) {
		unsigned int cni;
		vbi_program_id pi;

		if (dump_bin) {
			printf ("VPS L%3u ", line);
			fwrite (buffer, 1, 13, stdout);
			return;
		}

		if (!vbi_decode_vps_cni (&cni, buffer)) {
			printf ("Error in vps cni\n");
			return;
		}
		
		if (!vbi_decode_vps_pdc (&pi, buffer)) {
			printf ("Error in vps pdc data\n");
			return;
		}
		
		printf ("VPS L%3u cni=%x ", line, cni);

		vbi_program_id_dump (&pi, stdout);

		dump_nuid (pi.nuid);
	}

	if (dump_vps_r) {
		static char pr_label[2][20];
		static char label[2][20];
		static int l[2] = { 0, 0 };
		unsigned int i;
		int c;

		i = (line != 16);

		c = vbi_rev8 (buffer[1]);

		if (c & 0x80) {
			label[i][l[i]] = 0;
			strcpy (pr_label[i], label[i]);
			l[i] = 0;
		}

		label[i][l[i]] = printable (c);

		l[i] = (l[i] + 1) % 16;
		
		printf ("VPS L%3u 3-10: %02x %02x (%02x='%c') %02x %02x "
			"%02x %02x %02x %02x (\"%s\")\n",
			line,
			buffer[0], buffer[1],
			c, printable (c),
			buffer[2], buffer[3],
			buffer[4], buffer[5], buffer[6], buffer[7],
			pr_label[i]);
	}
}

static void
mainloop			(void)
{
  	char buf[256];
	double time = 0.0, dt;
	int index, items, i;
	vbi_sliced *s, sliced[40];

	while (1) {
		if (ferror(stdin) || !fgets(buf, 255, stdin))
			goto abort;

		dt = strtod(buf, NULL);
		items = fgetc(stdin);

		assert(items < 40);

		for (s = sliced, i = 0; i < items; s++, i++) {
			index = fgetc(stdin);
			s->line = (fgetc(stdin) + 256 * fgetc(stdin)) & 0xFFF;

			if (index < 0)
				goto abort;

			switch (index) {
			case 0:
				s->id = VBI_SLICED_TELETEXT_B;
				fread(s->data, 1, 42, stdin);
				teletext (s->data, s->line);
				break;
			case 1:
				s->id = VBI_SLICED_CAPTION_625; 
				fread(s->data, 1, 2, stdin);
				break; 
			case 2:
				s->id = VBI_SLICED_VPS; 
				fread(s->data, 1, 13, stdin);
				vps (s->data, s->line);
				break;
			case 3:
				s->id = VBI_SLICED_WSS_625; 
				fread(s->data, 1, 2, stdin);
				break;
			case 4:
				s->id = VBI_SLICED_WSS_CPR1204; 
				fread(s->data, 1, 3, stdin);
				break;
			case 7:
				s->id = VBI_SLICED_CAPTION_525; 
				fread(s->data, 1, 2, stdin);
				break;
			default:
				fprintf(stderr, "\nOops! Unknown data %d "
					"in sample file\n", index);
				exit(EXIT_FAILURE);
			}
		}

		if (feof(stdin) || ferror(stdin))
			goto abort;

		time += dt;
	}

	return;

abort:
	fprintf(stderr, "\rEnd of stream\n");
}

static const char
short_options [] = "12abeinprstv";

#ifdef HAVE_GETOPT_LONG
static const struct option
long_options [] = {
	{ "8301",	no_argument,		NULL,		'1' },
	{ "8302",	no_argument,		NULL,		'2' },
	{ "all",	no_argument,		NULL,		'a' },
	{ "bin",	no_argument,		NULL,		'b' },
	{ "hex",	no_argument,		NULL,		'e' },
	{ "help",	no_argument,		NULL,		'h' },
	{ "idl",	no_argument,		NULL,		'i' },
	{ "network",	no_argument,		NULL,		'n' },
	{ "pfc-pgno",	required_argument,	NULL,		'p' },
	{ "vps-r",	no_argument,		NULL,		'r' },
	{ "pfc-stream",	required_argument,	NULL,		's' },
	{ "ttx",	no_argument,		NULL,		't' },
	{ "vps",	no_argument,		NULL,		'v' },
	{ 0, 0, 0, 0 }
};
#else
#define getopt_long(ac, av, s, l, i) getopt(ac, av, s)
#endif

static void
usage				(FILE *			fp,
				 char **		argv)
{
	fprintf (fp,
		 "Usage: %s [options] < sliced vbi data\n\n"
		 "Decoding options:\n"
		 "-1 | --8301    Teletext packet 8/30 format 1 (local time)\n"
		 "-2 | --8302    Teletext packet 8/30 format 2 (PDC)\n"
		 "-i | --idl     Teletext IDL packets (M/30, M/31)\n"
		 "-t | --ttx     Decode any Teletext packet\n"
		 "-v | --vps     VPS (PDC)\n"
		 "-a | --all     Everything above, e. g.\n"
		 "               -i     decode IDL packets\n"
		 "               -a     decode everything\n"
		 "               -a -i  everything except IDL\n"
		 "-r | --vps-r   VPS data unrelated to PDC\n"
		 "-p | --pfc-pgno NNN\n"
		 "-s | --pfc-stream NN\n"
		 "               Decode Teletext Page Function Clear data\n"
		 "               from page NNN (for example 1DF), stream NN\n"
		 "               (optional, default is 0)\n"
		 "\n"
		 "Modifying options:\n"
		 "-h | --hex     With -t dump packets in hex and ASCII,\n"
		 "               otherwise only ASCII.\n"
		 "-n | --network With -1, -2, -v decode CNI and print\n"
		 "               available information about the network.\n"
		 "-b | --bin     With -t, -p, -v dump data in binary format,\n"
		 "               otherwise only ASCII.\n"
		 "\n"
		 "(--long options are only available on GNU systems.)\n",
		 argv[0]);
}

int
main				(int			argc,
				 char **		argv)
{
	int index;
	int c;

	while (-1 != (c = getopt_long (argc, argv, short_options,
				       long_options, &index))) {
		switch (c) {
		case 0:
			break;

		case '1':
			dump_8301 ^= TRUE;
			break;

		case '2':
			dump_8302 ^= TRUE;
			break;

		case 'b':
			dump_bin ^= TRUE;
			break;

		case 'a':
			dump_ttx = TRUE;
			dump_8301 = TRUE;
			dump_8302 = TRUE;
			dump_idl = TRUE;
			dump_vps = TRUE;
			pfc_pgno = 0x1DF;
			break;

		case 'e':
			dump_hex ^= TRUE;
			break;

		case 'h':
			usage (stdout, argv);
			exit (EXIT_SUCCESS);

		case 'i':
			dump_idl ^= TRUE;
			break;

		case 'n':
			dump_network ^= TRUE;
			break;

		case 'p':
			pfc_pgno = strtol (optarg, NULL, 16);
			break;

		case 'r':
			dump_vps_r ^= TRUE;
			break;

		case 's':
			pfc_stream = strtol (optarg, NULL, 10);
			break;

		case 't':
			dump_ttx ^= TRUE;
			break;

		case 'v':
			dump_vps ^= TRUE;
			break;

		default:
			usage (stderr, argv);
			exit (EXIT_FAILURE);
		}
	}

	if (0 != pfc_pgno) {
		pc = vbi_pfc_demux_new (pfc_pgno, pfc_stream,
					page_function_clear, NULL);
		assert (NULL != pc);
	} else {
		pc = NULL;
	}

	mainloop ();

	vbi_pfc_demux_delete (pc);

	exit (EXIT_SUCCESS);
}
