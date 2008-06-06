#ifndef _PSMCOMM_H_
#define _PSMCOMM_H_

#if defined(__FreeBSD) || defined(__NetBSD__) || defined(__OpenBSD)

#include <unistd.h>
#include <sys/ioctl.h>
#include <freebsd/mouse.h>

#endif

#endif /* _PSMCOMM_H_ */
