/*
 * Zapping (TV viewer for the Gnome Desktop)
 *
 * Copyright (C) 2000 Iñaki García Etxebarria
 * Copyright (C) 2003 Michael H. Schimek
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
  This is an auxiliary suid program to prepare a kernel video capture
  device for DMA overlay onto video memory. When you find any security
  flaws here, please report at zapping-misc@lists.sourceforge.net.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#ifdef HAVE_GETOPT_LONG
#include <getopt.h>
#endif

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <assert.h>

#include "zapping_setup_fb.h"

#define ZSFB_VERSION "zapping_setup_fb 0.10"
#define MAX_VERBOSITY 3

#ifndef HAVE_PROGRAM_INVOCATION_NAME
char *			program_invocation_name;
char *			program_invocation_short_name;
#endif

int			verbosity = 1;

int			uid, euid;

/* Frame buffer parameters */

unsigned long		addr;
unsigned int		bpl;
unsigned int		width;
unsigned int		height;
unsigned int		depth;
unsigned int		bpp;

void
fprintf_symbolic		(FILE *			fp,
				 int			mode,
				 unsigned long		value,
				 ...)
{
	unsigned int i, j = 0;
	unsigned long v;
	const char *s;
	va_list ap;

	if (0 == mode) {
		unsigned int n[2] = { 0, 0 };

		va_start (ap, value);

		for (i = 0; (s = va_arg (ap, const char *)); i++) {
			v = va_arg (ap, unsigned long);
			n[((v & (v - 1)) == 0)]++; /* single bit */
		}

		mode = 1 + (n[1] > n[0]); /* 1-enum, 2-flags */

		va_end (ap); 
	}

	va_start (ap, value);

	for (i = 0; (s = va_arg (ap, const char *)); i++) {
		v = va_arg (ap, unsigned long);
		if (2 == mode || v == value) {
			fprintf (fp, "%s%s%s", j++ ? "|" : "",
				 (2 == mode && 0 == (v & value)) ? "!" : "", s);
			value &= ~v;
		}
	}

	if (value)
		fprintf (fp, "%s0x%lx", j ? "|" : "", value);

	va_end (ap); 
}

#if defined (_IOC_SIZE) /* Linux */

#define IOCTL_ARG_SIZE(cmd)	_IOC_SIZE (cmd)
#define IOCTL_READ(cmd)		((cmd) & _IOC_READ)
#define IOCTL_WRITE(cmd)	((cmd) & _IOC_WRITE)
#define IOCTL_READ_WRITE(cmd)	(_IOC_DIR (cmd) == (_IOC_READ | _IOC_WRITE))
#define IOCTL_NUMBER(cmd)	_IOC_NR (cmd)

#elif defined (IOCPARM_LEN) /* FreeBSD */

#define IOCTL_ARG_SIZE(cmd)	IOCPARM_LEN (cmd)
#define IOCTL_READ(cmd)		((cmd) & IOC_OUT)
#define IOCTL_WRITE(cmd)	((cmd) & IOC_IN)
#define IOCTL_READ_WRITE(cmd)	(((cmd) & IOC_DIRMASK) == (IOC_IN | IOC_OUT))
#define IOCTL_NUMBER(cmd)	((cmd) & 0xFF)

#else

#define IOCTL_ARG_SIZE(cmd)	0
#define IOCTL_READ(cmd)		0
#define IOCTL_WRITE(cmd)	0
#define IOCTL_READ_WRITE(cmd)	0
#define IOCTL_NUMBER(cmd)	0

#endif

int
dev_ioctl			(int			fd,
				 unsigned int		cmd,
				 void *			arg,
				 ioctl_log_fn *		fn)
{
  int buf[256];
  int err;

  if (verbosity >= 3 && IOCTL_WRITE (cmd))
    {
      assert (sizeof (buf) >= IOCTL_ARG_SIZE (cmd));
      memcpy (buf, arg, IOCTL_ARG_SIZE (cmd));
    }

  do err = ioctl (fd, cmd, arg);
  while (-1 == err && EINTR == errno);

  if (3 <= verbosity && NULL != fn)
    {
      int saved_errno;

      saved_errno = errno;

      fprintf (stderr, "%d = ", err);
      fn (stderr, cmd, NULL);
      fputc ('(', stderr);
      
      if (IOCTL_WRITE (cmd))
	fn (stderr, cmd, &buf);

      if (0 == err)
	{
	  if (IOCTL_READ_WRITE (cmd))
	    fputs (") -> (", stderr);
	  
	  if (IOCTL_READ (cmd))
	    fn (stderr, cmd, arg);
	  
	  fputs (")\n", stderr);
	}
      else 
	{
	  fprintf (stderr, "), errno = %d, %s\n",
		   errno, strerror (errno));
	}
      
      errno = saved_errno;
    }
  
  return err;
}

#ifndef major
#  define major(dev)  (((dev) >> 8) & 0xff)
#endif

int
dev_open			(const char *		device_name,
				 int			major_number,
				 int			flags)
{
  struct stat st;
  int fd;

  /* Sanity checks */

  if (strchr (device_name, '.'))
    {
      message (1, "Device name '%s' rejected, has dots.\n", device_name);
      return -1;
    }

  if (strncmp (device_name, "/dev/", 5))
    {
      message (1, "Device name '%s' rejected, must start with '/dev/'.\n",
	       device_name);
      return -1;
    }

  if (-1 == stat (device_name, &st))
    {
      errmsg ("Cannot stat device '%s'", device_name);
      return -1;
    }

  if (!S_ISCHR (st.st_mode))
    {
      message (1, "'%s' is not a character device file.\n", device_name);
      return -1;
    }

  if (major_number != major (st.st_rdev))
    {
      message (1, "'%s' has suspect major number %d, expected %d.\n",
	       device_name, major (st.st_rdev), major_number);
      return -1;
    }

  message (2, "Opening device '%s'.\n", device_name);

  if (-1 == (fd = open (device_name, flags)))
    {
      errmsg ("Cannot open device '%s'", device_name);
    }

  return fd;
}

int
drop_root_privileges		(int			uid,
				 int			euid)
{
  if (ROOT_UID == euid && ROOT_UID != uid)
    {
      message (2, "Dropping root privileges\n");

      if (-1 == seteuid (uid))
        {
	  errmsg ("Cannot drop root privileges "
		  "despite uid=%d, euid=%d", uid, euid);
	  return FALSE;
        }
    }

  return TRUE;
}

int
restore_root_privileges		(int			uid,
				 int			euid)
{
  if (ROOT_UID == euid && ROOT_UID != uid)
    {
      message (2, "Restoring root privileges\n");

      if (-1 == seteuid (euid))
        {
	  errmsg ("Cannot restore root privileges "
		  "despite uid=%d, euid=%d", uid, euid);
	  return FALSE;
        }
    }

  return TRUE;
}

static const char short_options[] = "d:D:b:vqh?V";

#ifdef HAVE_GETOPT_LONG
static const struct option
long_options [] =
{
  { "device",		required_argument,	0, 'd' },
  { "display",		required_argument,	0, 'D' },
  { "bpp",		required_argument,	0, 'b' },
  { "verbose",		no_argument,		0, 'v' },
  { "quiet",		no_argument,		0, 'q' },
  { "help",		no_argument,		0, 'h' },
  { "usage",		no_argument,		0, 'h' },
  { "version",		no_argument,		0, 'V' },
};
#else
#define getopt_long(ac, av, s, l, i) getopt (ac, av, s)
#endif

static void
print_usage			(void)
{
  printf ("Usage:\n"
	  " %s [OPTIONS], where OPTIONS can be\n"
	  " -d, --device name     - The video device to open, default /dev/video0\n"
	  " -D, --display name    - The X display to use\n"
	  " -b, --bpp x           - Color depth, bits per pixel on said display\n"
	  " -v, --verbose         - Increment verbosity level\n"
	  " -q, --quiet           - Decrement verbosity level\n"
	  " -h, --help, --usage   - Show this message\n"
	  " -V, --version         - Print the program version and exit\n"
	  "", program_invocation_name);
}

int
main				(int			argc,
				 char **		argv)
{
  char *device_name;
  char *display_name;
  int bpp_arg;
  int err;

#ifndef HAVE_PROGRAM_INVOCATION_NAME
  program_invocation_name =
  program_invocation_short_name = argv[0];
#endif

  /*
   *  Make sure fd's 0 1 2 are open, otherwise
   *  we might end up sending error messages to
   *  the device file.
   */
  {
    int i, n;

    for (i = 0; i < 3; i++)
      if (-1 == fcntl (i, F_GETFL, &n))
	exit (EXIT_FAILURE);
  }

  /* Drop root privileges until we need them */

  uid = getuid ();
  euid = geteuid ();

  if (!drop_root_privileges (uid, euid))
    goto failure;

  /* Parse arguments */

  device_name = "/dev/video0";
  display_name = getenv ("DISPLAY");

  bpp_arg = -1;

  for (;;)
    {
      int c = getopt_long (argc, argv, short_options, long_options, NULL);

      if (c == -1)
        break;

      switch (c)
        {
	case 'd':
	  device_name = strdup (optarg);
	  break;
	    
	case 'D':
	  display_name = strdup (optarg);
	  break;

	case 'b':
	  bpp_arg = strtol (optarg, NULL, 0);

	  switch (bpp_arg)
	    {
	    case 8:
	    case 15:
	    case 16:
	    case 24:
	    case 32:
	      break;

	    default:
	      message (1, "Invalid bpp argument %d. Expected "
		       "color depth 8, 15, 16, 24 or 32.\n", bpp_arg);
	      goto failure;
	    }
	  
	  break;

	case 'v':
	  if (verbosity < MAX_VERBOSITY)
	    verbosity++;
	  break;

	case 'q':
	  if (verbosity > 0)
	    verbosity--;
	  break;

	case 'V':
	  message (0, "%s\n", ZSFB_VERSION);
	  exit (EXIT_SUCCESS);

	case 'h':
	  print_usage ();
	  exit (EXIT_SUCCESS);

	default:
	  /* getopt_long prints option name when unknown or arg missing */
	  print_usage ();
	  goto failure;
	}
    }

  message (1, "(C) 2000-2003 Iñaki G. Etxebarria, Michael H. Schimek.\n"
	   "This program is freely redistributable under the terms\n"
	   "of the GNU General Public License.\n\n");

  message (1, "Using video device '%s', display '%s'.\n",
	   device_name, display_name);

  if (1 != query_dga (display_name, bpp_arg))
    goto failure;
 
  /* OK, the DGA is working and we have its info,
     set up the overlay */

  err = setup_v4l25 (device_name);

  if (err == -1)
    {
      err = setup_v4l2 (device_name);

      if (err == -1)
	{
	  err = setup_v4l (device_name);
	}
    }

  if (err != 1)
    goto failure;

  message (1, "Setup completed.\n");

  return EXIT_SUCCESS;

 failure:
  message (1, "Setup failed.\n");

  return EXIT_FAILURE;
}
