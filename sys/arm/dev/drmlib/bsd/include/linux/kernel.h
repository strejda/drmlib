#ifndef __DRM_LINUX_KERNEL_H__
#define __DRM_LINUX_KERNEL_H__

#include_next <linux/kernel.h>

#include <asm/processor.h>
#include <linux/capability.h>
#include <linux/irqflags.h>
#include <linux/kconfig.h>
#include <linux/kgdb.h>
#include <linux/ktime.h>
#include <linux/delay.h>

/* XXXKIB what is the right code for the FreeBSD ? */
/* kib@ used ENXIO here -- dumbbell@ */
#define	EREMOTEIO	EIO

#define	lower_32_bits(n)	((u32)(n))
#define	upper_32_bits(n)	((u32)(((n) >> 16) >> 16))

struct device_node;

#endif /* __DRM_LINUX_KERNEL_H__ */
