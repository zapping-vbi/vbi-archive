/* Generated file, do not edit! */

#include <stdio.h>
#include "io.h"

#ifndef __GNUC__
#undef __attribute__
#define __attribute__(x)
#endif

static size_t
snprint_struct_v4l2_capability (char *str, size_t size, int rw __attribute__ ((unused)), const struct v4l2_capability *t)
{
size_t n = 0;
n += snprintf (str + n, size - n, "name=\"%.*s\" "
"type=",
32, (const char *) t->name);
n = MIN (n, size);
n += snprint_symbolic (str + n, size - n, 0, t->type,
(void *) 0);
n += snprintf (str + n, size - n, " inputs=%ld "
"outputs=%ld "
"audios=%ld "
"maxwidth=%ld "
"maxheight=%ld "
"minwidth=%ld "
"minheight=%ld "
"maxframerate=%ld "
"flags=",
(long) t->inputs, 
(long) t->outputs, 
(long) t->audios, 
(long) t->maxwidth, 
(long) t->maxheight, 
(long) t->minwidth, 
(long) t->minheight, 
(long) t->maxframerate);
n = MIN (n, size);
n += snprint_symbolic (str + n, size - n, 2, t->flags,
(void *) 0);
n += strlcpy (str + n, " reserved[] ", size - n);
n = MIN (n, size);
return n;
}

static size_t
snprint_ioctl_arg (char *str, size_t size, unsigned int cmd, int rw, void *arg)
{
size_t n;
assert (size > 0);
switch (cmd) {
case VIDIOC_QUERYCAP:
if (!arg) { return strlcpy (str, "VIDIOC_QUERYCAP", size); }
 return snprint_struct_v4l2_capability (str, size, rw, arg);
default:
if (!arg) { return snprint_unknown_ioctl (str, size, cmd, arg); }
str[0] = 0; return 0;
}
return 0; }

static __inline__ void IOCTL_ARG_TYPE_CHECK_VIDIOC_QUERYCAP (struct v4l2_capability *arg __attribute__ ((unused))) {}

