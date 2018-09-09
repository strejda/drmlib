#ifndef __LINUX_FB_H_
#define __LINUX_FB_H_
#include <sys/fbio.h>
#include <uapi/linux/fb.h>

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/workqueue.h>
#include <linux/list.h>

/* screen: unblanked, hsync: on,  vsync: on */
#define	FB_BLANK_UNBLANK	0

/* screen: blanked,   hsync: on,  vsync: on */
#define	FB_BLANK_NORMAL		1

/* screen: blanked,   hsync: on,  vsync: off */
#define	FB_BLANK_VSYNC_SUSPEND	2

/* screen: blanked,   hsync: off, vsync: on */
#define	FB_BLANK_HSYNC_SUSPEND 3

/* screen: blanked,   hsync: off, vsync: off */
#define	FB_BLANK_POWERDOWN	4
	
struct fb_cmap {
	__u32 start;			/* First entry	*/
	__u32 len;			/* Number of entries */
	__u16 *red;			/* Red values	*/
	__u16 *green;
	__u16 *blue;
	__u16 *transp;			/* transparency, can be NULL */
};


struct fb_bitfield {
	__u32 offset;			/* beginning of bitfield	*/
	__u32 length;			/* length of bitfield		*/
	__u32 msb_right;		/* != 0 : Most significant bit is */ 
					/* right */ 
};


#define FB_NONSTD_HAM		1	/* Hold-And-Modify (HAM)        */
#define FB_NONSTD_REV_PIX_IN_B	2	/* order of pixels in each byte is reversed */

#define FB_ACTIVATE_NOW		0	/* set values immediately (or vbl)*/
#define FB_ACTIVATE_NXTOPEN	1	/* activate on next open	*/
#define FB_ACTIVATE_TEST	2	/* don't set, round up impossible */
#define FB_ACTIVATE_MASK       15
					/* values			*/
#define FB_ACTIVATE_VBL	       16	/* activate values on next vbl  */
#define FB_CHANGE_CMAP_VBL     32	/* change colormap on vbl	*/
#define FB_ACTIVATE_ALL	       64	/* change all VCs on this fb	*/
#define FB_ACTIVATE_FORCE     128	/* force apply even when no change*/
#define FB_ACTIVATE_INV_MODE  256       /* invalidate videomode */


struct fb_var_screeninfo {
	__u32 xres;			/* visible resolution		*/
	__u32 yres;
	__u32 xres_virtual;		/* virtual resolution		*/
	__u32 yres_virtual;
	__u32 xoffset;			/* offset from virtual to visible */
	__u32 yoffset;			/* resolution			*/

	__u32 bits_per_pixel;		/* guess what			*/
	__u32 grayscale;		/* 0 = color, 1 = grayscale,	*/
					/* >1 = FOURCC			*/
	struct fb_bitfield red;		/* bitfield in fb mem if true color, */
	struct fb_bitfield green;	/* else only length is significant */
	struct fb_bitfield blue;
	struct fb_bitfield transp;	/* transparency			*/	

	__u32 nonstd;			/* != 0 Non standard pixel format */

	__u32 activate;			/* see FB_ACTIVATE_*		*/

	__u32 height;			/* height of picture in mm    */
	__u32 width;			/* width of picture in mm     */

	__u32 accel_flags;		/* (OBSOLETE) see fb_info.flags */

	/* Timing: All values in pixclocks, except pixclock (of course) */
	__u32 pixclock;			/* pixel clock in ps (pico seconds) */
	__u32 left_margin;		/* time from sync to picture	*/
	__u32 right_margin;		/* time from picture to sync	*/
	__u32 upper_margin;		/* time from sync to picture	*/
	__u32 lower_margin;
	__u32 hsync_len;		/* length of horizontal sync	*/
	__u32 vsync_len;		/* length of vertical sync	*/
	__u32 sync;			/* see FB_SYNC_*		*/
	__u32 vmode;			/* see FB_VMODE_*		*/
	__u32 rotate;			/* angle we rotate counter clockwise */
	__u32 colorspace;		/* colorspace for FOURCC-based modes */
	__u32 reserved[4];		/* Reserved for future compatibility */
};

struct apertures_struct;

int remove_conflicting_framebuffers(struct apertures_struct *a,
    const char *name, bool primary);
int fb_get_options(const char *name, char **option);
int fb_alloc_cmap(struct fb_cmap *cmap, int len, int transp);

struct fb_info *framebuffer_alloc(size_t size, struct device *dev);
void framebuffer_release(struct fb_info *info);
void fb_set_suspend(struct fb_info *info, int state);
int unlink_framebuffer(struct fb_info *fb_info);

#endif /* __LINUX_FB_H_ */
