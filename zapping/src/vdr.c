/* Zapping (TV viewer for the Gnome Desktop)
 * VDR Interface
 * Copyright (C) 2003 Slobodan Tomic, based on XawTV patch.
 * Python command modifications by mhs.
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
 * Interface to VideoDiskRecorder
 */

#include "../config.h"

#include <gnome.h>

#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "vdr.h"
#include "remote.h"
#include "zmisc.h"

static int vdr_sock = -1;
static struct sockaddr_in vdr_sockaddr;

static void
vdr_close			(int			signum);

static gboolean
vdr_open			(void)
{
    int opt=1;
    struct protoent *proto_ent;
    char c;

    signal(SIGPIPE, vdr_close);
    
    if (!(proto_ent=getprotobyname("TCP"))) {
         fprintf(stderr, "vdr: getprotobyname failed\n");
         return FALSE;
    }
    if ((vdr_sock=socket(PF_INET, SOCK_STREAM, proto_ent->p_proto)) < 0) {
         fprintf(stderr, "vdr: socket failed\n");
         return FALSE;
    }
    vdr_sockaddr.sin_family = PF_INET;
    /* FIXME do not hardwire the port */
    vdr_sockaddr.sin_port = htons( 2001 );
    /* FIXME handle both loopback and normal ip */
    vdr_sockaddr.sin_addr.s_addr = htonl( INADDR_LOOPBACK );
    setsockopt(vdr_sock,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
    if (connect(vdr_sock, (struct sockaddr *)&vdr_sockaddr, sizeof(vdr_sockaddr))) {
         fprintf(stderr, "vdr: connect failed\n");        
         return FALSE;
    }
    fcntl(vdr_sock,F_SETFL,O_NONBLOCK);
    fcntl(vdr_sock,F_SETFD,FD_CLOEXEC);
                    
    /* skip the greeting message */
    do {
      c = '\0';
      if (read(vdr_sock, &c, 1) < 0) {
        if( errno == EAGAIN )
	  continue;
	fprintf(stderr, "vdr: initial read failed");
        close(vdr_sock);
	vdr_sock = -1;
	break;
      }
    } while (c != '\n');

    return TRUE;
}

static void
vdr_close			(int			signum)
{
  if (vdr_sock != -1) {
    write(vdr_sock, "QUIT\r\n", 6);
    close(vdr_sock);
    vdr_sock = -1;
  }
}

static PyObject *
py_vdr				(PyObject *		self,
				 PyObject *		args)
{
  char *s;
  gchar *t;

  t = NULL;

  if (!PyArg_ParseTuple (args, "s", &s))
    g_error ("zapping.vdr(s)");

  if (strlen (s) > 0)
    {
      guint l;
      ssize_t r;
      char c;

      if (vdr_sock < 0)
	vdr_open();
      if (vdr_sock < 0)
	goto failure;

      t = g_strconcat (s, "\r\n", NULL);
      l = strlen (t);

      do r = write (vdr_sock, t, l);
      while (-1 == r && EINTR == errno);

      if (r != l)
	{
	  fprintf (stderr, "vdr write failed: %u %s (%d/%u)",
		   errno, strerror (errno), r, l);
	  close(vdr_sock);
	  vdr_sock = -1;
	  goto failure;
	}

      /* skip the answer */
      do {
	c = '\0';
	if (read(vdr_sock, &c, 1) < 0) {
	  if (EAGAIN == errno || EINTR == errno)
	    continue;
	  fprintf (stderr, "vdr read failed: %u %s",
		   errno, strerror (errno));
	  close(vdr_sock);
	  vdr_sock = -1;
	  break;
	}
      } while (c != '\n');
    }

  Py_INCREF(Py_None);
  return Py_None;

 failure:
  g_free (t);

  py_return_false;
}

gboolean
startup_vdr			(void)
{
  if (vdr_open ())
    {
      cmd_register ("vdr", py_vdr, METH_VARARGS,
		    ("Send commands to VDR daemon at localhost:2001"),
		    "zapping.vdr('command')");
      return FALSE;
    }
  else
    {
      return TRUE;
    }
}

void
shutdown_vdr			(void)
{
  vdr_close (0);
}
