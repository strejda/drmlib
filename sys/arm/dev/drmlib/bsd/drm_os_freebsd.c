
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/priv.h>

#include <drm/drmP.h>
#include <linux/cdev.h>
#undef cdev

devclass_t drm_devclass;
const char *fb_mode_option = NULL;

MALLOC_DEFINE(DRM_MEM_DMA, "drm_dma", "DRM DMA Data Structures");
MALLOC_DEFINE(DRM_MEM_DRIVER, "drm_driver", "DRM DRIVER Data Structures");
MALLOC_DEFINE(DRM_MEM_KMS, "drm_kms", "DRM KMS Data Structures");

SYSCTL_NODE(_dev, OID_AUTO, drm, CTLFLAG_RW, 0, "DRM args");
SYSCTL_INT(_dev_drm, OID_AUTO, drm_debug, CTLFLAG_RWTUN, &drm_debug, 0, "drm debug flags");

int
drm_dev_alias(struct device *ldev, struct drm_minor *minor, const char *minor_str)
{
	struct sysctl_oid_list *oid_list, *child;
	struct sysctl_ctx_list *ctx_list;
	struct linux_cdev *cdevp;
	struct sysctl_oid *node;
	device_t dev = ldev->parent->bsddev;
	char buf[20];
	char *devbuf;

	MPASS(dev != NULL);
	ctx_list = device_get_sysctl_ctx(dev);
	child = SYSCTL_CHILDREN(device_get_sysctl_tree(dev));
	sprintf(buf, "%d", minor->index);
	node = SYSCTL_ADD_NODE(ctx_list, SYSCTL_STATIC_CHILDREN(_dev_drm), OID_AUTO, buf,
			       CTLFLAG_RD, NULL, "drm properties");
	oid_list = SYSCTL_CHILDREN(node);
	sprintf(buf, "%x:%x", pci_get_vendor(dev),
		pci_get_device(dev));
	/* XXX leak - fix me */
	devbuf = strndup(buf, 20, DRM_MEM_DRIVER);
	SYSCTL_ADD_STRING(ctx_list, oid_list, OID_AUTO, "PCI_ID",
			  CTLFLAG_RD, devbuf, 0, "vendor and device ids");

	/*
	 * FreeBSD won't automaticaly create the corresponding device
	 * node as linux must so we find the corresponding one created by
	 * register_chrdev in drm_drv.c and alias it.
	 */
	sprintf(buf, "dri/%s", minor_str);
	cdevp = linux_find_cdev("drm", DRM_MAJOR, minor->index);
	MPASS(cdevp != NULL);
	if (cdevp == NULL)
		return (-ENXIO);
	/* XXXX Fix type of bsd_device */
	minor->bsd_device = (struct linux_cdev *)cdevp->cdev;
	make_dev_alias(cdevp->cdev, buf, minor->index);
	return (0);
}

bool
capable(enum linux_capabilities cap)
{

	switch (cap) {
	case CAP_SYS_ADMIN:
		return (priv_check(curthread, PRIV_DRIVER) == 0);
		break;
	case CAP_SYS_NICE:
		// BSDFIXME: What to do for CAP_SYS_NICE?
		return (priv_check(curthread, PRIV_DRIVER) == 0);
		break;
	default:
		panic("%s: unhandled capability: %0x", __func__, cap);
		return (false);
	}
}

static int
drm_modevent(module_t mod, int type, void *data)
{
	switch (type) {
	case MOD_LOAD:
		TUNABLE_INT_FETCH("drm.debug", &drm_debug);
		break;
	}
	return (0);
}

static moduledata_t drm_mod = {
	"drmn",
	drm_modevent,
	0
};

DECLARE_MODULE(drmn, drm_mod, SI_SUB_DRIVERS, SI_ORDER_FIRST);
MODULE_VERSION(drmn, 2);
MODULE_DEPEND(drmn, pci, 1, 1, 1);
MODULE_DEPEND(drmn, linuxkpi, 1, 1, 1);
