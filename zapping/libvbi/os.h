#ifndef OS_H
#define OS_H

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBsd__) \
							|| defined(__bsdi__)
#define BSD
#define DEFAULT_SMALL_BUF
#endif

#if defined(__linux__)
/* people with the 2.2 kernel's bttv driver may want to define this... */
//#define DEFAULT_SMALL_BUF
#endif

#endif /* OS_H */
