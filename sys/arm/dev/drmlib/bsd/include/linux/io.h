#ifndef __DRM_LINUX_IO_H__
#define	__DRM_LINUX_IO_H__

#include_next<linux/io.h>

#ifndef arch_phys_wc_add
static inline int
arch_phys_wc_add(unsigned long base, unsigned long size)
{
	return (0);
}
#define arch_phys_wc_add arch_phys_wc_add

static inline void
arch_phys_wc_del(int handle)
{
}

#ifndef arch_phys_wc_index
static inline int
arch_phys_wc_index(int handle)
{
	return (-1);
}
#define arch_phys_wc_index arch_phys_wc_index
#endif
#endif

#endif /* __DRM_LINUX_IO_H__ */
