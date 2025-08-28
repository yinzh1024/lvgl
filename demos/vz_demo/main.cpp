#include "lvgl/lvgl.h"
#include "lvgl/demos/lv_demos.h"
#include "src/display/lv_display_private.h"

#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>

#define TEXT_FONT_SIZE 196

lv_obj_t *s_label_list[4];

static const char *getenv_default(const char *name, const char *dflt)
{
    return getenv(name) ? : dflt;
}

static void anim_x_cb(void * var, int32_t v)
{
    char text[128];
    snprintf(text, sizeof(text) - 1, "你好，臻识：%d", rand());
    for (int i = 0; i < 4; i++) {
        lv_label_set_text(s_label_list[i], text);
        lv_obj_set_x(s_label_list[i], v);
    }
}

static void anim_y_cb(void * var, int32_t v)
{
    static int32_t p = 0;
    // p += 4;
    p = v;
    char text[128];
    snprintf(text, sizeof(text) - 1, "你好，臻识：%d", rand());
    lv_label_set_text((lv_obj_t *)var, text);
    lv_obj_set_y((lv_obj_t*)var, p);
}

static void anim_rot_cb(void * var, int32_t v)
{
    lv_obj_set_style_transform_rotation((lv_obj_t*)var, v * 10, LV_PART_MAIN);
    // lv_obj_set_x((lv_obj_t *)var, -v);
}

void test_rotate_label(lv_obj_t *screen, lv_style_t *comm_style, int angle_val)
{
    lv_obj_t * label = lv_label_create(screen);
    lv_label_set_text(label, "2022年05月24日!");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFF0000), LV_PART_MAIN);
    lv_obj_set_style_transform_rotation(label, angle_val * 10, LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_y(label, 0, LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_x(label, 0, LV_PART_MAIN);
}

void create_rotate_label(lv_obj_t *screen, lv_style_t *comm_style)
{
    lv_obj_t * label = lv_label_create(screen);
    lv_label_set_text(label, "2022年05月24日!");
    lv_obj_align(label, LV_ALIGN_BOTTOM_LEFT, 0, TEXT_FONT_SIZE);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFF0000), LV_PART_MAIN);
    lv_obj_set_style_transform_rotation(label, -90 * 10, LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_x(label, 0, LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_y(label, 0, LV_PART_MAIN);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, label);
#if 0
    lv_anim_set_values(&a, 0, -900);
    lv_anim_set_duration(&a, 900 * 1000);
    lv_anim_set_exec_cb(&a, anim_rot_cb);
    lv_anim_set_path_cb(&a, lv_anim_path_linear);
#elif 1
    lv_anim_set_values(&a, -1080, 1080);
    lv_anim_set_duration(&a, 10000);
    lv_anim_set_exec_cb(&a, anim_y_cb);
    lv_anim_set_path_cb(&a, lv_anim_path_linear);
#endif
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

/**
 * Start animation on an event
 */
void lv_example_anim_1(void)
{
    lv_obj_t *img = lv_image_create(lv_screen_active());
    // lv_image_set_src(img, "A:/nfsroot/test.bmp");
#ifdef CHIP_PLATFORM_HISI_V6_0
    lv_image_set_src(img, "A:/nfsroot/rgb-compose.jpg");
#elif CHIP_PLATFORM_SSTAR_V2_0
    lv_image_set_src(img, "A:/tmp/nfs/rgb-compose.jpg");
#endif
    // lv_obj_center(img);

    lv_font_t * font = lv_tiny_ttf_create_file("A:/tmp/app/exec/font/DroidSans.ttf", TEXT_FONT_SIZE);
    static lv_style_t ft_style;
    lv_style_init(&ft_style);
    lv_style_set_text_font(&ft_style, font);
    lv_style_set_text_color(&ft_style, lv_color_make(0xFF, 0, 0));
    lv_obj_t * screen = lv_screen_active();
    lv_obj_add_style(screen, &ft_style, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    // for (int i = 0; i < 10; i++) {
    //     test_rotate_label(screen, &ft_style, i * 10);
    // }
    // create_rotate_label(screen,  &ft_style);

    for (int i = 0; i < sizeof(s_label_list) / sizeof(s_label_list[0]); i++) {
        s_label_list[i] = lv_label_create(lv_scr_act());
        lv_obj_add_style(s_label_list[i], &ft_style, LV_STATE_DEFAULT);
        lv_label_set_text(s_label_list[i], "2022年05月24日!");
        lv_obj_set_size(s_label_list[i], 3000, 600);
        lv_obj_set_pos(s_label_list[i], 0, i * 256 + 600);
        lv_obj_remove_flag(s_label_list[i], LV_OBJ_FLAG_SCROLLABLE);
    }

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, nullptr);
    lv_anim_set_values(&a, -1080, 1080);
    lv_anim_set_duration(&a, 10 * 1000);
    lv_anim_set_exec_cb(&a, anim_x_cb);
    lv_anim_set_path_cb(&a, lv_anim_path_linear);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

#if LV_USE_LINUX_FBDEV
static void lv_linux_disp_init(void)
{
    const char *device = getenv_default("LV_LINUX_FBDEV_DEVICE", "/dev/fb0");
    lv_display_t * disp = lv_linux_fbdev_create();
    lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_90);

    lv_linux_fbdev_set_file(disp, device);
}
#elif LV_USE_LINUX_DRM
static void lv_linux_disp_init(void)
{
    const char *device = getenv_default("LV_LINUX_DRM_CARD", "/dev/dri/card0");
    lv_display_t * disp = lv_linux_drm_create();

    lv_linux_drm_set_file(disp, device, -1);
}
#elif LV_USE_SDL
static void lv_linux_disp_init(void)
{
    const int width = atoi(getenv("LV_SDL_VIDEO_WIDTH") ? : "800");
    const int height = atoi(getenv("LV_SDL_VIDEO_HEIGHT") ? : "480");

    lv_sdl_window_create(width, height);
}
#else
#error Unsupported configuration
#endif

int main(void)
{
    lv_init();

    /*Linux display device init*/
    lv_linux_disp_init();

    // lv_obj_set_style_bg_color(lv_scr_act(), lv_color_make(0xFF, 0xFF, 0xFF), 0);

    /*Create a Demo*/
    // lv_demo_widgets();
    // lv_demo_widgets_start_slideshow();
    // lv_demo_music();
    lv_example_anim_1();

    prctl(PR_SET_NAME, "LVGLMain");
    /*Handle LVGL tasks*/
    while(1) {
        lv_timer_handler();
        usleep(5000);
    }

    return 0;
}
