/*
 *  Copyright (C) 1999-2000 Michael H. Schimek
 *  Unmodified non-profit redistribution permitted.
 *
 *  gcc -O2 test.c tables.c decode.c
 *  ./a.out [/dev/vbi*]
 */

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <linux/types.h>
#include <linux/videodev.h>
#include "../common/mmx.h"
#include "vbi.h"

static struct decode_rec	vpsd, ttxd;

/*
 *  ETS 300 706 8.1 Odd parity
 */
unsigned char
odd_parity[256] __attribute__ ((aligned(CACHE_LINE))) =
{
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0
};

static unsigned char buf[42];

static const char *pcs_names[] = {
	"unknown", "mono", "stereo", "bilingual"
};

static const char *month_names[] = {
	"0", "Jan", "Feb", "Mar", "Apr",
	"May", "Jun", "Jul", "Aug",
	"Sep", "Oct", "Nov", "Dec",
	"13", "14", "15"
};

#define printable(c) (((c) < 0x20 || (c) > 0x7E) ? '.' : (c))

#define PIL(day, mon, hour, min) \
	(((day) << 15) + ((mon) << 11) + ((hour) << 6) + ((min) << 0))

static void
dump_pil(int pil)
{
	int day, mon, hour, min;

	day = pil >> 15;
	mon = (pil >> 11) & 0xF;
	hour = (pil >> 6) & 0x1F;
	min = pil & 0x3F;

	if (pil == PIL(0, 15, 31, 63))
		printf(" PDC: Timer-control (no PDC)\n");
	else if (pil == PIL(0, 15, 30, 63))
		printf(" PDC: Recording inhibit/terminate\n");
	else if (pil == PIL(0, 15, 29, 63))
		printf(" PDC: Interruption\n");
	else if (pil == PIL(0, 15, 28, 63))
		printf(" PDC: Continue\n");
	else if (pil == PIL(31, 15, 31, 63))
		printf(" PDC: No time\n");
	else
		printf(" PDC: %05x, %2d %s %02d:%02d\n",
			pil, day, month_names[mon], hour, min);
}

static void
dump_pty(int pty)
{
	if (pty == 0xFF)
		printf(" Prog. type: %02x unused", pty);
	else
		printf(" Prog. type: %02x class %s", pty, program_class[pty >> 4]);

	if (pty < 0x80) {
		if (program_type[pty >> 4][pty & 0xF])
			printf(", type %s", program_type[pty >> 4][pty & 0xF]);
		else
			printf(", type undefined");
	}

	putchar('\n');
}

static void
dump_status(unsigned char *buf)
{
	int j;

	printf(" Status: \"");

	for (j = 0; j < 20; j++) {
		char c = odd_parity[buf[j]] ? buf[j] & 0x7F : '?';

		c = printable(c);
		putchar(c);
	}

	printf("\"\n");
}

static void
_decode_vps(unsigned char *buf)
{
	static char pr_label[20];
	static char label[20];
	static int l = 0;
	int cni, pcs, pty, pil;
	int c, j;

	for (j = 0; j < 13; j++) {
		c = unbip(buf + j * 2);

		if (c < 0)
			return; /* tx error */

		buf[j] = c;
	}

	printf("\nVPS:\n");

	c = bit_reverse[buf[1]];

	if ((char) c < 0) {
		label[l] = 0;
		memcpy(pr_label, label, sizeof(pr_label));
		l = 0;
	}

	c &= 0x7F;

	label[l] = printable(c);

	l = (l + 1) % 16;

	printf(" 3-10: %02x %02x %02x %02x %02x %02x %02x %02x (\"%s\")\n",
		buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], pr_label);

	pcs = buf[2] >> 6;

	cni = + ((buf[10] & 3) << 10)
	      + ((buf[11] & 0xC0) << 2)
	      + ((buf[8] & 0xC0) << 0)
	      + (buf[11] & 0x3F);

	pil = ((buf[8] & 0x3F) << 14) + (buf[9] << 6) + (buf[10] >> 2);

	pty = buf[12];

	if (cni)
		for (j = 0; VPS_CNI[j].short_name; j++)
			if (VPS_CNI[j].cni == cni) {
				printf(" Country: %s\n Station: %s%s\n",
					country_names_en[VPS_CNI[j].country],
					VPS_CNI[j].long_name,
					(cni == 0x0DC3) ? ((buf[2] & 0x10) ? " (ZDF)" : " (ARD)") : "");
				break;
			}

//	if (!cni || !VPS_CNI[j].short_name)
		printf(" CNI: %04x\n", cni);

	printf(" Analog audio: %s\n", pcs_names[pcs]);

	dump_pil(pil);
	dump_pty(pty);
}

static void
_decode_pdc(unsigned char *buf)
{
	int lci, luf, prf, pcs, mi, pty;
	int cni, cni_vps, pil;
	int j;

	lci = (buf[0] >> 2) & 3;
	luf = !!(buf[0] & 2);
	prf = buf[0] & 1;
	pcs = buf[1] >> 6;
	mi = !!(buf[1] & 0x20);

	cni_vps =
	      + ((buf[4] & 0x03) << 10)
	      + ((buf[5] & 0xC0) << 2)
	      + (buf[2] & 0xC0)
	      + (buf[5] & 0x3F);

	cni = cni_vps + ((buf[1] & 0x0F) << 12);

	pil = ((buf[2] & 0x3F) << 14) + (buf[3] << 6) + (buf[4] >> 2);

	pty = buf[6];

	printf(" Label channel %d: label update %d,"
	       " prepare to record %d, mode %d\n",
		lci, luf, prf, mi);

	if (cni) {
		for (j = 0; VPS_CNI[j].short_name; j++)
			if (VPS_CNI[j].cni == cni_vps) {
				printf(" Country: %s\n Station: %s%s\n",
					country_names_en[VPS_CNI[j].country],
					VPS_CNI[j].long_name,
					(cni_vps == 0x0DC3) ? ((buf[2] & 0x10) ? " (ZDF)" : " (ARD)") : "");
				break;
			}

		if (!VPS_CNI[j].short_name)
			for (j = 0; PDC_CNI[j].short_name; j++)
				if (PDC_CNI[j].cni2 == cni) {
					printf(" Country: %s\n Station: %s\n",
						country_names_en[PDC_CNI[j].country],
						PDC_CNI[j].long_name);
					break;
				}
	}

//	if (!cni || !PDC_CNI[j].short_name)
		printf(" CNI: %04x\n", cni);

	printf(" Analog audio: %s\n", pcs_names[pcs]);

	dump_pil(pil);
	dump_pty(pty);
}

static void
decode_8301(unsigned char *buf)
{
	int j, cni, cni_vps, lto;
	int mjd, utc_h, utc_m, utc_s;
	struct tm tm;
	time_t ti;

	cni = bit_reverse[buf[9]] * 256 + bit_reverse[buf[10]];
	cni_vps = cni & 0x0FFF;

	lto = (buf[11] & 0x7F) >> 1;

	mjd = + ((buf[12] & 0xF) - 1) * 10000
	      + ((buf[13] >> 4) - 1) * 1000
	      + ((buf[13] & 0xF) - 1) * 100
	      + ((buf[14] >> 4) - 1) * 10
	      + ((buf[14] & 0xF) - 1);

	utc_h = ((buf[15] >> 4) - 1) * 10 + ((buf[15] & 0xF) - 1);
	utc_m = ((buf[16] >> 4) - 1) * 10 + ((buf[16] & 0xF) - 1);
	utc_s = ((buf[17] >> 4) - 1) * 10 + ((buf[17] & 0xF) - 1);

	if (cni) {
		for (j = 0; VPS_CNI[j].short_name; j++)
			if (VPS_CNI[j].cni == cni_vps) {
				printf(" Country: %s\n Station: %s%s\n",
					country_names_en[VPS_CNI[j].country],
					VPS_CNI[j].long_name,
					(cni_vps == 0x0DC3) ? ((buf[2] & 0x10) ? " (ZDF)" : " (ARD)") : "");
				break;
			}

		if (!VPS_CNI[j].short_name)
			for (j = 0; PDC_CNI[j].short_name; j++)
				if (PDC_CNI[j].cni1 == cni) {
					printf(" Country: %s\n Station: %s\n",
					    	country_names_en[PDC_CNI[j].country],
						PDC_CNI[j].long_name);
					break;
				}
	}

	if (!cni || !PDC_CNI[j].short_name)
		printf(" CNI: %04x\n", cni);

	ti = (mjd - 40587) * 86400 + 43200;
	localtime_r(&ti, &tm);

	printf(" Local time: MJD %d %02d %s %04d, UTC %02d:%02d:%02d %c%02d%02d\n",
		mjd, tm.tm_mday, month_names[tm.tm_mon + 1], tm.tm_year + 1900,
		utc_h, utc_m, utc_s,
		(buf[11] & 0x80) ? '-' : '+', lto >> 1, (lto & 1) * 30);
}

static void
decode_ttx(unsigned char *buf)
{
	int packet_address;
	int magazine, packet;
	int designation;
	int c, j;

	packet_address = unham84(buf + 0);

	if (packet_address < 0)
		return; /* hamming error */

	magazine = packet_address & 7;
	packet = packet_address >> 3;

	if (magazine != 0 /* 0 -> 8 */
	    || packet != 30)
		return;

	designation = hamming84[buf[2]]; 

	if (designation < 0) {
		return; /* hamming error */
	} else if (designation <= 1) {
		printf("\nPacket 8/30/1:\n");

		decode_8301(buf);
		dump_status(buf + 22);
	} else if (designation <= 3) {
		printf("\nPacket 8/30/2:\n");

		for (j = 0; j < 7; j++) {
			c = unham84(buf + j * 2 + 8);

			if (c < 0)
				return; /* hamming error */

			buf[j] = bit_reverse[c];
		}

		_decode_pdc(buf);
		dump_status(buf + 22);
	}
}

static int fd;

static int start;
static int lines;
static int samples;
static int rate;

static unsigned char *raw;
static int rawsize;

#ifdef V4L2_MAJOR_VERSION

#ifndef V4L2_BUF_TYPE_VBI // pending extension
#define V4L2_BUF_TYPE_VBI V4L2_BUF_TYPE_CAPTURE;
#endif

static struct v4l2_capability cap;
static struct v4l2_format format;

#endif

static int
init_vbi(char *dev_name)
{
	if ((fd = open(dev_name, O_RDONLY)) < 0) {
		perror("open");
		exit(EXIT_FAILURE);
	}

#ifdef V4L2_MAJOR_VERSION

	if (ioctl(fd, VIDIOC_QUERYCAP, &cap) >= 0) {

		printf("Opened %s (\'%s\')\n", dev_name, cap.name);

		if (cap.type != V4L2_TYPE_VBI) {
			fprintf(stderr, "Not a VBI device: %s\n", dev_name);
			exit(EXIT_FAILURE);
		}

		/* Should request PAL scanning here */

		memset(&format, 0, sizeof(format));

		format.type = V4L2_BUF_TYPE_VBI;
		format.fmt.vbi.sampling_rate = 27000000;
		format.fmt.vbi.sample_format = V4L2_VBI_SF_UBYTE;
		format.fmt.vbi.start[0] = 6;
		format.fmt.vbi.count[0] = 16;
		format.fmt.vbi.start[1] = 318;
		format.fmt.vbi.count[1] = 16;

		if (ioctl(fd, VIDIOC_S_FMT, &format) < 0) {
			perror("VIDIOC_S_FMT");
			exit(EXIT_FAILURE);
		}

		if (format.fmt.vbi.sampling_rate < 6937500 * 2 ||
		    format.fmt.vbi.start[0] > 6 ||
		    format.fmt.vbi.start[1] > 318 || 
		    format.fmt.vbi.start[0] + format.fmt.vbi.count[0] < 22 ||
		    format.fmt.vbi.start[1] + format.fmt.vbi.count[1] < 334) {
			fprintf(stderr, "Cannot capture Teletext with this device\n");
			exit(EXIT_FAILURE);
		}

    		if (format.fmt.vbi.sample_format != V4L2_VBI_SF_UBYTE ||
		    format.fmt.vbi.flags & V4L2_VBI_INTERLACED) {
			fprintf(stderr, "Sorry, VBI format not supported by this tool\n");
			exit(EXIT_FAILURE);
		}

		fprintf(stderr, "Using V4L2 VBI interface...\n");

		start = format.fmt.vbi.start[0];
		lines = format.fmt.vbi.count[0] + format.fmt.vbi.count[1];
		samples = format.fmt.vbi.samples_per_line;
		rate = format.fmt.vbi.sampling_rate;
	} else 

#endif

	{
		fprintf(stderr, "Using BTTV VBI interface...\n");

		start = 6; /* yes? */
		lines = 16 * 2;
		samples = 2048;
		rate = 35468950;
	}

	rawsize = lines * samples;

	if (!(raw = malloc(rawsize))) {
		fprintf(stderr, "No memory\n");
		exit(EXIT_FAILURE);
	}

	return 1;
}

int
main(int ac, char **av)
{
	char *dev_name = "/dev/vbi0";

	if (ac > 1) dev_name = av[1];

	init_vbi(dev_name);

	init_decoder(&vpsd, samples, rate, 5000000, 0x99515555, 0xFFFFFF00, 26);
	init_decoder(&ttxd, samples, rate, 6937500, 0x27555500, 0xFFFF0000, 42);

	for (;;) {
		int i;

		if (rawsize != read(fd, raw, rawsize)) {
			perror("read");
			exit(EXIT_FAILURE);
		}

		if (decode_nrz(&vpsd, raw + (16 - (start + 1)) * samples, buf))
			_decode_vps(buf);

		for (i = 0; i < lines; i++)
			if (decode_nrz(&ttxd, raw + i * samples, buf))
				decode_ttx(buf);
	}

	return EXIT_SUCCESS;
}
