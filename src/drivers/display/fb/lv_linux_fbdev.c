/**
 * @file lv_linux_fbdev.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "lv_linux_fbdev.h"
#include "src/misc/lv_utils.h"
#if LV_USE_LINUX_FBDEV

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stddef.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <time.h>

#include "gfbg.h"

#if LV_LINUX_FBDEV_BSD
    #include <sys/fcntl.h>
    #include <sys/consio.h>
    #include <sys/fbio.h>
#else
    #include <linux/fb.h>
#endif /* LV_LINUX_FBDEV_BSD */

#include "../../../display/lv_display_private.h"
#include "../../../draw/sw/lv_draw_sw.h"

/*********************
 *      DEFINES
 *********************/
#define DEBUG_TIME_COST 0

#if (LV_LINUX_FBDEV_RENDER_MODE == 2) // LV_DISPLAY_RENDER_MODE_FULL
#define ENABLE_FB_DOUBLE_BUFFER
#endif

/**********************
 *      TYPEDEFS
 **********************/
struct bsd_fb_var_info {
    uint32_t xoffset;
    uint32_t yoffset;
    uint32_t xres;
    uint32_t yres;
    int bits_per_pixel;
};

struct bsd_fb_fix_info {
    long int line_length;
    long int smem_len;
};

typedef struct {
    const char * devname;
    lv_color_format_t color_format;
#if LV_LINUX_FBDEV_BSD
    struct bsd_fb_var_info vinfo;
    struct bsd_fb_fix_info finfo;
#else
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
#endif /* LV_LINUX_FBDEV_BSD */
#if LV_LINUX_FBDEV_MMAP
    char * fbp;
#endif
    uint8_t * rotated_buf;
    size_t rotated_buf_size;
    long int screensize;
    int fbfd;
    bool fb_sw;
    bool force_refresh;
} lv_linux_fb_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * color_p);
static uint32_t tick_get_cb(void);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

#if LV_LINUX_FBDEV_BSD
    #define FBIOBLANK FBIO_BLANK
#endif /* LV_LINUX_FBDEV_BSD */

#ifndef DIV_ROUND_UP
    #define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))
#endif

#define PHY_SCREEN_WIDTH 1920
#define PHY_SCREEN_HEIGHT 1080

#define LV_SCREEN_WIDTH 1088 // must align 16
#define LV_SCREEN_HEIGHT 1080

#ifdef CHIP_PLATFORM_SSTAR_V2_0

#define FB_IOC_MAGIC 'F'
#define FBIOGET_SCREEN_LOCATION _IOR(FB_IOC_MAGIC, 0x60, MI_FB_Rectangle_t)
#define FBIOSET_SCREEN_LOCATION _IOW(FB_IOC_MAGIC, 0x61, MI_FB_Rectangle_t)

typedef struct MI_FB_Rectangle_s
{
    uint16_t x_pos;
    uint16_t y_pos;
    uint16_t width;
    uint16_t height;
}MI_FB_Rectangle_t;

#endif

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_display_t * lv_linux_fbdev_create(void)
{
    lv_tick_set_cb(tick_get_cb);

    lv_linux_fb_t * dsc = lv_malloc_zeroed(sizeof(lv_linux_fb_t));
    LV_ASSERT_MALLOC(dsc);
    if(dsc == NULL) return NULL;

    lv_display_t * disp = lv_display_create(LV_SCREEN_WIDTH, LV_SCREEN_HEIGHT);
    if(disp == NULL) {
        lv_free(dsc);
        return NULL;
    }
    dsc->fbfd = -1;
    lv_display_set_driver_data(disp, dsc);
    lv_display_set_flush_cb(disp, flush_cb);

    return disp;
}

void lv_linux_fbdev_set_file(lv_display_t * disp, const char * file)
{
    char * devname = lv_strdup(file);
    LV_ASSERT_MALLOC(devname);
    if(devname == NULL) return;

    lv_linux_fb_t * dsc = lv_display_get_driver_data(disp);
    dsc->devname = devname;

    if(dsc->fbfd > 0) close(dsc->fbfd);

    /* Open the file for reading and writing*/
    dsc->fbfd = open(dsc->devname, O_RDWR);
    if(dsc->fbfd == -1) {
        perror("Error: cannot open framebuffer device");
        return;
    }
    LV_LOG_INFO("The framebuffer device was opened successfully");

    /* Make sure that the display is on.*/
    if(ioctl(dsc->fbfd, FBIOBLANK, FB_BLANK_UNBLANK) != 0) {
        perror("ioctl(FBIOBLANK)");
        /* Don't return. Some framebuffer drivers like efifb or simplefb don't implement FBIOBLANK.*/
    }

#if LV_LINUX_FBDEV_BSD
    struct fbtype fb;
    unsigned line_length;

    /*Get fb type*/
    if(ioctl(dsc->fbfd, FBIOGTYPE, &fb) != 0) {
        perror("ioctl(FBIOGTYPE)");
        return;
    }

    /*Get screen width*/
    if(ioctl(dsc->fbfd, FBIO_GETLINEWIDTH, &line_length) != 0) {
        perror("ioctl(FBIO_GETLINEWIDTH)");
        return;
    }

    dsc->vinfo.xres = (unsigned) fb.fb_width;
    dsc->vinfo.yres = (unsigned) fb.fb_height;
    dsc->vinfo.bits_per_pixel = fb.fb_depth;
    dsc->vinfo.xoffset = 0;
    dsc->vinfo.yoffset = 0;
    dsc->finfo.line_length = line_length;
    dsc->finfo.smem_len = dsc->finfo.line_length * dsc->vinfo.yres;
#else /* LV_LINUX_FBDEV_BSD */

    dsc->vinfo.xres = disp->hor_res;
    dsc->vinfo.yres = disp->ver_res;
    dsc->vinfo.bits_per_pixel = LV_COLOR_DEPTH;

#ifdef ENABLE_FB_DOUBLE_BUFFER
    dsc->vinfo.xres_virtual = LV_SCREEN_WIDTH;
    dsc->vinfo.yres_virtual = LV_SCREEN_HEIGHT * 2;
#else
    dsc->vinfo.xres_virtual = LV_SCREEN_WIDTH;
    dsc->vinfo.yres_virtual = LV_SCREEN_HEIGHT;
#endif

#if LV_COLOR_DEPTH == 32
    struct fb_bitfield  s_a32 = {24, 8, 0};
    struct fb_bitfield  s_r32 = {16, 8, 0};
    struct fb_bitfield  s_g32 = {8, 8, 0};
    struct fb_bitfield  s_b32 = {0, 8, 0};
    dsc->vinfo.transp = s_a32;
    dsc->vinfo.red = s_r32;
    dsc->vinfo.green = s_g32;
    dsc->vinfo.blue = s_b32;
#elif LV_COLOR_DEPTH == 24
    struct fb_bitfield  s_a24 = {0, 0, 0};
    struct fb_bitfield  s_r24 = {16, 8, 0};
    struct fb_bitfield  s_g24 = {8, 8, 0};
    struct fb_bitfield  s_b24 = {0, 8, 0};
    dsc->vinfo.transp = s_a24;
    dsc->vinfo.red = s_r24;
    dsc->vinfo.green = s_g24;
    dsc->vinfo.blue = s_b24;
#elif LV_COLOR_DEPTH == 16
#ifdef CHIP_PLATFORM_SSTAR_V2_0
    struct fb_bitfield  s_a16 = {0, 0, 0};
    struct fb_bitfield  s_r16 = {11, 5, 0};
    struct fb_bitfield  s_g16 = {5, 6, 0};
    struct fb_bitfield  s_b16 = {0, 5, 0};
#elif defined(CHIP_PLATFORM_HISI_V6_0)
    struct fb_bitfield  s_a16 = {12, 4, 0};
    struct fb_bitfield  s_r16 = {8, 4, 0};
    struct fb_bitfield  s_g16 = {4, 4, 0};
    struct fb_bitfield  s_b16 = {0, 4, 0};
#endif
    dsc->vinfo.transp = s_a16;
    dsc->vinfo.red = s_r16;
    dsc->vinfo.green = s_g16;
    dsc->vinfo.blue = s_b16;
#endif
    dsc->vinfo.activate = FB_ACTIVATE_NOW;

    if(ioctl(dsc->fbfd, FBIOPUT_VSCREENINFO, &dsc->vinfo) == -1) {
        perror("Error write variable information");
        return;
    }

    /* Get fixed screen information*/
    if(ioctl(dsc->fbfd, FBIOGET_FSCREENINFO, &dsc->finfo) == -1) {
        perror("Error reading fixed information");
        return;
    }

    /* Get variable screen information*/
    if(ioctl(dsc->fbfd, FBIOGET_VSCREENINFO, &dsc->vinfo) == -1) {
        perror("Error reading variable information");
        return;
    }

#ifdef CHIP_PLATFORM_SSTAR_V2_0
    /* Set variable screen pos */
    // MI_FB_Rectangle_t rect = {0, 0, LV_SCREEN_WIDTH, LV_SCREEN_HEIGHT};
    MI_FB_Rectangle_t rect = {PHY_SCREEN_WIDTH - LV_SCREEN_WIDTH, PHY_SCREEN_HEIGHT - LV_SCREEN_HEIGHT, LV_SCREEN_WIDTH, LV_SCREEN_HEIGHT};
    if(ioctl(dsc->fbfd, FBIOSET_SCREEN_LOCATION, &rect) == -1) {
        perror("Error FBIOSET_SCREEN_LOCATION");
        return;
    }
#endif
#endif /* LV_LINUX_FBDEV_BSD */

    LV_LOG_INFO("%dx%d, %dbpp", dsc->vinfo.xres, dsc->vinfo.yres, dsc->vinfo.bits_per_pixel);

    /* Figure out the size of the screen in bytes*/
    // dsc->screensize =  dsc->finfo.smem_len;
    dsc->screensize =  dsc->finfo.line_length * dsc->vinfo.yres_virtual;
    LV_LOG_ERROR("mem size: %d, %d", dsc->screensize, dsc->finfo.smem_len);

#if LV_LINUX_FBDEV_MMAP
    /* Map the device to memory*/
    dsc->fbp = (char *)mmap(0, dsc->screensize, PROT_READ | PROT_WRITE, MAP_SHARED, dsc->fbfd, 0);
    if((intptr_t)dsc->fbp == -1) {
        perror("Error: failed to map framebuffer device to memory");
        return;
    }
#endif

    /* Don't initialise the memory to retain what's currently displayed / avoid clearing the screen.
     * This is important for applications that only draw to a subsection of the full framebuffer.*/

    LV_LOG_INFO("The framebuffer device was mapped to memory successfully");

    switch(dsc->vinfo.bits_per_pixel) {
        case 16:
            lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
            break;
        case 24:
            lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB888);
            break;
        case 32:
            lv_display_set_color_format(disp, LV_COLOR_FORMAT_XRGB8888);
            break;
        default:
            LV_LOG_WARN("Not supported color format (%d bits)", dsc->vinfo.bits_per_pixel);
            return;
    }

    int32_t hor_res = dsc->vinfo.xres;
    int32_t ver_res = dsc->vinfo.yres;
    int32_t width = dsc->vinfo.width;
    uint32_t draw_buf_size = hor_res * (dsc->vinfo.bits_per_pixel >> 3);
    if(LV_LINUX_FBDEV_RENDER_MODE == LV_DISPLAY_RENDER_MODE_PARTIAL) {
        draw_buf_size *= LV_LINUX_FBDEV_BUFFER_SIZE;
    }
    else {
        draw_buf_size *= ver_res;
    }

    uint8_t * draw_buf = NULL;
    uint8_t * draw_buf_2 = NULL;
    draw_buf = malloc(draw_buf_size);

    if(LV_LINUX_FBDEV_BUFFER_COUNT == 2) {
        draw_buf_2 = malloc(draw_buf_size);
    }

    lv_display_set_resolution(disp, hor_res, ver_res);
    lv_display_set_buffers(disp, draw_buf, draw_buf_2, draw_buf_size, LV_LINUX_FBDEV_RENDER_MODE);

    if(width > 0) {
        lv_display_set_dpi(disp, DIV_ROUND_UP(hor_res * 254, width * 10));
    }

    LV_LOG_INFO("Resolution is set to %" LV_PRId32 "x%" LV_PRId32 " at %" LV_PRId32 "dpi",
                hor_res, ver_res, lv_display_get_dpi(disp));
}

void lv_linux_fbdev_set_force_refresh(lv_display_t * disp, bool enabled)
{
    lv_linux_fb_t * dsc = lv_display_get_driver_data(disp);
    dsc->force_refresh = enabled;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void write_to_fb(lv_linux_fb_t * dsc, uint32_t fb_pos, const void * data, size_t sz)
{
#if LV_LINUX_FBDEV_MMAP
    uint8_t * fbp = (uint8_t *)dsc->fbp;
    lv_memcpy(&fbp[fb_pos], data, sz);
#else
    if(pwrite(dsc->fbfd, data, sz, fb_pos) < 0)
        LV_LOG_ERROR("write failed: %d", errno);
#endif
}

static void flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * color_p)
{
    lv_linux_fb_t * dsc = lv_display_get_driver_data(disp);

#if LV_LINUX_FBDEV_MMAP
    if(dsc->fbp == NULL) {
        lv_display_flush_ready(disp);
        return;
    }
#endif

    int32_t w = lv_area_get_width(area);
    int32_t h = lv_area_get_height(area);
    lv_color_format_t cf = lv_display_get_color_format(disp);
    uint32_t px_size = lv_color_format_get_size(cf);

    lv_area_t rotated_area;
    lv_display_rotation_t rotation = lv_display_get_rotation(disp);

#if DEBUG_TIME_COST
    uint64_t s_pts, e_pts;
    lv_get_cur_pts(&s_pts);
#endif
    /* Not all framebuffer kernel drivers support hardware rotation, so we need to handle it in software here */
    if(rotation != LV_DISPLAY_ROTATION_0 && LV_LINUX_FBDEV_RENDER_MODE == LV_DISPLAY_RENDER_MODE_PARTIAL) {
        /* (Re)allocate temporary buffer if needed */
        size_t buf_size = w * h * px_size;
        if(!dsc->rotated_buf || dsc->rotated_buf_size < buf_size) {
            dsc->rotated_buf = realloc(dsc->rotated_buf, buf_size);
            dsc->rotated_buf_size = buf_size;
        }

        /* Rotate the pixel buffer */
        uint32_t w_stride = lv_draw_buf_width_to_stride(w, cf);
        uint32_t h_stride = lv_draw_buf_width_to_stride(h, cf);

        switch(rotation) {
            case LV_DISPLAY_ROTATION_0:
                break;
            case LV_DISPLAY_ROTATION_90:
                lv_draw_sw_rotate(color_p, dsc->rotated_buf, w, h, w_stride, h_stride, rotation, cf);
                break;
            case LV_DISPLAY_ROTATION_180:
                lv_draw_sw_rotate(color_p, dsc->rotated_buf, w, h, w_stride, w_stride, rotation, cf);
                break;
            case LV_DISPLAY_ROTATION_270:
                lv_draw_sw_rotate(color_p, dsc->rotated_buf, w, h, w_stride, h_stride, rotation, cf);
                break;
        }
        color_p = dsc->rotated_buf;

        /* Rotate the area */
        rotated_area = *area;
        lv_display_rotate_area(disp, &rotated_area);
        area = &rotated_area;

        if(rotation != LV_DISPLAY_ROTATION_180) {
            w = lv_area_get_width(area);
            h = lv_area_get_height(area);
        }
    }
#if DEBUG_TIME_COST
    lv_get_cur_pts(&e_pts);
    static int64_t max_rotate_cost = 0;
    if (e_pts - s_pts > max_rotate_cost) {
        max_rotate_cost = e_pts - s_pts;
    }
    printf("rotate cost %lld, max: %lld us\n", e_pts - s_pts, max_rotate_cost);
#endif

#ifdef ENABLE_FB_DOUBLE_BUFFER
    dsc->vinfo.yoffset = dsc->fb_sw ? dsc->vinfo.yres : 0;
    dsc->fb_sw = !dsc->fb_sw;
#endif

    /* Ensure that we're within the framebuffer's bounds */
    if(area->x2 < 0 || area->y2 < 0 || area->x1 > (int32_t)dsc->vinfo.xres - 1 || area->y1 > (int32_t)dsc->vinfo.yres - 1) {
        lv_display_flush_ready(disp);
        return;
    }

    uint32_t fb_pos =
        (area->x1 + dsc->vinfo.xoffset) * px_size +
        (area->y1 + dsc->vinfo.yoffset) * dsc->finfo.line_length;

    int32_t y;
    if(LV_LINUX_FBDEV_RENDER_MODE == LV_DISPLAY_RENDER_MODE_DIRECT) {
        uint32_t color_pos =
            area->x1 * px_size +
            area->y1 * disp->hor_res * px_size;

        for(y = area->y1; y <= area->y2; y++) {
            write_to_fb(dsc, fb_pos, &color_p[color_pos], w * px_size);
            fb_pos += dsc->finfo.line_length;
            color_pos += disp->hor_res * px_size;
        }
    }
    else {
        // NOTE: 这里改为使用硬件dma效果不明显
#if DEBUG_TIME_COST
        uint64_t s_pts, e_pts;
        lv_get_cur_pts(&s_pts);
#endif
        w = lv_area_get_width(area);
        for(y = area->y1; y <= area->y2; y++) {
            write_to_fb(dsc, fb_pos, color_p, w * px_size);
            fb_pos += dsc->finfo.line_length;
            color_p += w * px_size;
        }
#if DEBUG_TIME_COST
        lv_get_cur_pts(&e_pts);
        static int64_t max_rotate_cost = 0;
        if (e_pts - s_pts > max_rotate_cost) {
            max_rotate_cost = e_pts - s_pts;
        }
        printf("write fb cost %lld, max: %lld us\n", e_pts - s_pts, max_rotate_cost);
#endif
    }

    if(dsc->force_refresh) {
        dsc->vinfo.activate |= FB_ACTIVATE_NOW | FB_ACTIVATE_FORCE;
        if(ioctl(dsc->fbfd, FBIOPUT_VSCREENINFO, &(dsc->vinfo)) == -1) {
            perror("Error setting var screen info");
        }
    }

#ifdef ENABLE_FB_DOUBLE_BUFFER
    if (ioctl(dsc->fbfd, FBIOPAN_DISPLAY, &dsc->vinfo) < 0) {
        perror("Error FBIOPAN_DISPLAY");
    }
#endif

    lv_display_flush_ready(disp);
}

static uint32_t tick_get_cb(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    uint64_t time_ms = t.tv_sec * 1000 + (t.tv_nsec / 1000000);
    return time_ms;
}

#endif /*LV_USE_LINUX_FBDEV*/
