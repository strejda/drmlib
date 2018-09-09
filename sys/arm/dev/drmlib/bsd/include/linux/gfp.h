#ifndef __DRM_LINUX_GFP_H__
#define	__DRM_LINUX_GFP_H__

#include_next<linux/gfp.h>

#define	__GFP_KSWAPD_RECLAIM	0

#define	GFP_TRANSHUGE_LIGHT	M_WAITOK
#endif /* __DRM_LINUX_GFP_H__ */
