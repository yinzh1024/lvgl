/**
 * @file lv_linux_fbdev.h
 *
 */

#ifndef LV_LINUX_FBDEV_H
#define LV_LINUX_FBDEV_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include "../../../display/lv_display.h"

#if LV_USE_LINUX_FBDEV

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
 uint32_t fb_width;
 uint32_t fb_height;
 uint32_t screen_width;
 uint32_t screen_height;
 lv_point_t start_pos;
} lv_disp_fb_info_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/
lv_display_t * lv_linux_fbdev_create(lv_disp_fb_info_t *fb_info);

void lv_linux_fbdev_destroy(lv_display_t *disp);

void lv_linux_fbdev_set_file(lv_display_t * disp, const char * file);

/**
 * Force the display to be refreshed on every change.
 * Expected to be used with LV_DISPLAY_RENDER_MODE_DIRECT or LV_DISPLAY_RENDER_MODE_FULL.
 */
void lv_linux_fbdev_set_force_refresh(lv_display_t * disp, bool enabled);

/**********************
 *      MACROS
 **********************/

#endif /* LV_USE_LINUX_FBDEV */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LV_LINUX_FBDEV_H */
