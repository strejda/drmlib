#ifndef __DRM_LINUX_STRING_H__
#define	__DRM_LINUX_STRING_H__

#include_next<linux/string.h>

#define strscpy(a, b, c) strlcpy(a, b, c)

#endif /* __DRM_LINUX_STRING_H__ */
