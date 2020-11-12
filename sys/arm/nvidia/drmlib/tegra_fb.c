/*-
 * Copyright (c) 2016 Michal Meloun
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/vmem.h>

#include <vm/vm.h>

#include <machine/bus.h>

#include <dev/extres/clk/clk.h>
#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_gem.h>

#include <arm/nvidia/drmlib/tegra_drm.h>

static void
fb_destroy(struct drm_framebuffer *drm_fb)
{
	struct tegra_fb *fb;
	struct tegra_bo *bo;
	unsigned int i;

	fb = container_of(drm_fb, struct tegra_fb, drm_fb);
	for (i = 0; i < fb->nplanes; i++) {
		bo = fb->planes[i];
		if (bo != NULL) {
			pmap_qremove(fb->planes_vbase[i], bo->npages);
			vmem_free(kmem_arena, fb->planes_vbase[i], bo->size);
			drm_gem_object_unreference_unlocked(&bo->gem_obj);
		}
	}

	drm_framebuffer_cleanup(drm_fb);
	free(fb->planes, DRM_MEM_DRIVER);
	free(fb->planes_vbase, DRM_MEM_DRIVER);
}

static int
fb_create_handle(struct drm_framebuffer *drm_fb, struct drm_file *file,
 unsigned int *handle)
{
	struct tegra_fb *fb;
	int rv;

	fb = container_of(drm_fb, struct tegra_fb, drm_fb);
	rv = drm_gem_handle_create(file, &fb->planes[0]->gem_obj, handle);
	return (rv);
}

/* XXX Probably not needed */
static int
fb_dirty(struct drm_framebuffer *fb, struct drm_file *file_priv,
unsigned flags, unsigned color, struct drm_clip_rect *clips, unsigned num_clips)
{

	return (0);
}

static const struct drm_framebuffer_funcs fb_funcs = {
	.destroy = fb_destroy,
	.create_handle = fb_create_handle,
	.dirty = fb_dirty,
};

static int
fb_alloc(struct drm_device *drm, const struct drm_mode_fb_cmd2 *mode_cmd,
    struct tegra_bo **planes, int num_planes, struct tegra_fb **res_fb)
{
	struct tegra_fb *fb;
	int i;
	int rv;

	fb = malloc(sizeof(*fb), DRM_MEM_DRIVER, M_WAITOK | M_ZERO);
	fb->planes = malloc(num_planes * sizeof(*fb->planes), DRM_MEM_DRIVER,
	    M_WAITOK | M_ZERO);
	fb->planes_vbase = malloc(num_planes * sizeof(*fb->planes_vbase),
	    DRM_MEM_DRIVER,  M_WAITOK | M_ZERO);
	fb->nplanes = num_planes;

	drm_helper_mode_fill_fb_struct(drm, &fb->drm_fb, mode_cmd);
	for (i = 0; i < fb->nplanes; i++) {
		fb->planes[i] = planes[i];
		rv = vmem_alloc(kmem_arena, planes[i]->size,
		    M_WAITOK | M_BESTFIT, &fb->planes_vbase[i]);
		if (rv != 0)
			return (ENOMEM);
		pmap_qenter(fb->planes_vbase[i], planes[i]->m,
		    planes[i]->npages);
	}
	rv = drm_framebuffer_init(drm, &fb->drm_fb, &fb_funcs);
	if (rv < 0) {
		device_printf(drm->dev,
		    "Cannot initialize frame buffer %d\n", rv);
		free(fb->planes, DRM_MEM_DRIVER);
		return (rv);
	}
	*res_fb = fb;
	return (0);
}


static int
tegra_fb_probe(struct drm_fb_helper *helper,
    struct drm_fb_helper_surface_size *sizes)
{
	u_int bpp, size; //, offs;
	struct tegra_drm *drm;
	struct tegra_fb *fb;
	struct fb_info *info;
	struct tegra_bo *bo;
	struct drm_mode_fb_cmd2 mode_cmd;
	struct drm_device *drm_dev;
	int rv;

	if (helper->fb != NULL)
		return (0);

	DRM_DEBUG_KMS("surface: %d x %d (bpp: %d)\n", sizes->surface_width,
	    sizes->surface_height, sizes->surface_bpp);

	drm_dev = helper->dev;
	fb = container_of(helper, struct tegra_fb, fb_helper);
	drm = container_of(drm_dev, struct tegra_drm, drm_dev);
	bpp = (sizes->surface_bpp + 7) / 8;

	/* Create mode_cmd */
	memset(&mode_cmd, 0, sizeof(mode_cmd));
	mode_cmd.width = sizes->surface_width;
	mode_cmd.height = sizes->surface_height;
	mode_cmd.pitches[0] = roundup(sizes->surface_width * bpp,
	    drm->pitch_align);
	mode_cmd.pixel_format = drm_mode_legacy_fb_format(sizes->surface_bpp,
	    sizes->surface_depth);
	size = mode_cmd.pitches[0] * mode_cmd.height;

	rv = tegra_bo_create(drm_dev, size, &bo);
	if (rv != 0)
		return (rv);

	info = drm_fb_helper_alloc_fbi(helper);
	if (IS_ERR(info)) {
		device_printf(drm_dev->dev,
		    "Cannot allocate DRM framebuffer info.\n");
		rv =  PTR_ERR(info);
		goto err_object;
	}

	rv = fb_alloc(drm_dev, &mode_cmd,  &bo, 1, &fb);
	if (rv != 0) {
		device_printf(drm_dev->dev,
		     "Cannot allocate DRM framebuffer.\n");
		goto err_fb;
	}

	helper->fb = &fb->drm_fb;
	helper->fbdev = info;

	/* Fill FB info */
	info->fb_vbase = fb->planes_vbase[0];
	info->fb_pbase = fb->planes[0]->pbase;
	info->fb_size = size;
	info->fb_bpp = sizes->surface_bpp;
	drm_fb_helper_fill_fix(info, fb->drm_fb.pitches[0],
	    fb->drm_fb.format->depth);
	drm_fb_helper_fill_var(info, helper, fb->drm_fb.width,
	    fb->drm_fb.height);

	drm_dev->mode_config.fb_base =  bo->pbase;

	DRM_DEBUG_KMS("allocated %dx%d (s %dbits) fb size: %d, bo %p\n",
		      fb->drm_fb.width, fb->drm_fb.height,
		      fb->drm_fb.format->depth, size, bo);
	DRM_DEBUG_KMS(" vbase: 0x%08X, pbase: 0x%08X\n",
		      info->fb_vbase, info->fb_pbase);
	return (0);
err_fb:
	drm_gem_object_unreference_unlocked(&bo->gem_obj);
	framebuffer_release(info);
err_object:
	drm_gem_object_release(&bo->gem_obj);
	return (rv);
}

static struct drm_fb_helper_funcs fb_helper_funcs = {
	.fb_probe = tegra_fb_probe,
};

/*
 *	Exported functions
 */
struct fb_info *
tegra_drm_fb_getinfo(struct drm_device *drm_dev)
{
	struct tegra_fb *fb;
	struct tegra_drm *drm;

	drm = container_of(drm_dev, struct tegra_drm, drm_dev);
	fb = drm->fb;
	if (fb == NULL)
		return (NULL);
	return (fb->fb_helper.fbdev);
}

struct tegra_bo *
tegra_fb_get_plane(struct tegra_fb *fb, int idx)
{

	if (idx >= drm_format_num_planes(fb->drm_fb.format->format))
		return (NULL);
	if (idx >= fb->nplanes)
		return (NULL);
	return (fb->planes[idx]);
}

int
tegra_drm_fb_preinit(struct drm_device *drm_dev)
{
	struct tegra_fb *fb;
	struct tegra_drm *drm;

	drm = container_of(drm_dev, struct tegra_drm, drm_dev);

	fb = malloc(sizeof(*fb), DRM_MEM_DRIVER, M_WAITOK | M_ZERO);
	drm_fb_helper_prepare(drm_dev, &fb->fb_helper, &fb_helper_funcs);
	drm->fb = fb;

	return (0);
}

int
tegra_drm_fb_init(struct drm_device *drm_dev)
{

	struct tegra_drm *drm;
	int rv;

	drm = container_of(drm_dev, struct tegra_drm, drm_dev);

	rv = drm_fb_helper_init(drm_dev, &drm->fb->fb_helper,
	     drm_dev->mode_config.num_connector);
	if (rv != 0) {
		device_printf(drm_dev->dev,
		    "Cannot initialize frame buffer %d\n", rv);
		return (rv);
	}

	rv = drm_fb_helper_single_add_all_connectors(&drm->fb->fb_helper);
	if (rv != 0) {
		device_printf(drm_dev->dev, "Cannot add all connectors: %d\n",
		    rv);
		goto err_fini;
	}

	rv = drm_fb_helper_initial_config(&drm->fb->fb_helper, 32);
	if (rv != 0) {
		device_printf(drm_dev->dev,
		    "Cannot set initial config: %d\n", rv);
		goto err_fini;
	}

	return 0;

err_fini:
	drm_fb_helper_fini(&drm->fb->fb_helper);
	return (rv);
}

struct drm_framebuffer *
tegra_drm_fb_create(struct drm_device *drm, struct drm_file *file,
    const struct drm_mode_fb_cmd2 *cmd)
{
	int hsub, vsub, i;
	int width, height, size, bpp;
	struct tegra_bo *planes[4];
	struct drm_gem_object *gem_obj;
	struct tegra_fb *fb;
	int rv, nplanes;

	hsub = drm_format_horz_chroma_subsampling(cmd->pixel_format);
	vsub = drm_format_vert_chroma_subsampling(cmd->pixel_format);

	nplanes = drm_format_num_planes(cmd->pixel_format);
	for (i = 0; i < nplanes; i++) {
		width = cmd->width;
		height = cmd->height;
		if (i != 0) {
			width /= hsub;
			height /= vsub;
		}
		gem_obj = drm_gem_object_lookup(file, cmd->handles[i]);
		if (gem_obj == NULL)
			goto fail;

		bpp = drm_format_plane_cpp(cmd->pixel_format, i);
		size = (height - 1) * cmd->pitches[i] +
		    width * bpp + cmd->offsets[i];
		if (gem_obj->size < size)
			goto fail;
		planes[i] = container_of(gem_obj, struct tegra_bo, gem_obj);
	}

	rv = fb_alloc(drm, cmd, planes, nplanes, &fb);
	if (rv != 0)
		goto fail;

	return (&fb->drm_fb);

fail:
	while (i--)
		drm_gem_object_unreference_unlocked(&planes[i]->gem_obj);
	return (NULL);
}

void
tegra_drm_fb_destroy(struct drm_device *drm_dev)
{
	struct fb_info *info;
	struct tegra_fb *fb;
	struct tegra_drm *drm;

	drm = container_of(drm_dev, struct tegra_drm, drm_dev);
	fb = drm->fb;
	if (fb == NULL)
		return;
	info = fb->fb_helper.fbdev;

	drm_framebuffer_remove(&fb->drm_fb);
	framebuffer_release(info);
	drm_fb_helper_fini(&fb->fb_helper);
	drm_framebuffer_cleanup(&fb->drm_fb);

	free(fb, DRM_MEM_DRIVER);
	drm->fb = NULL;
}
