#include "dev_sdk/chwinfo.h"
#include "eventservice/util/time_util.h"
#include "lvgl.h"
#include "src/extra/libs/freetype/lv_freetype.h"
#include "src/hwdisplay/hisi_fbdev.h"
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sstream>
#include <stdio.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#ifdef ENABLE_FB_DOUBLE_BUFFER
#define DISP_BUF_SIZE (16200 * 1024)
#else
#define DISP_BUF_SIZE (128 * 1024)
#endif

#if LV_BUILD_EXAMPLES && LV_USE_SWITCH

static char time_str[16];
static void anim_x_cb(void * var, int32_t v)
{
  static int32_t p = 0;
  // p += 4;
  p = v;
  lv_label_set_text((lv_obj_t*)var, vzes::XTimeUtil::TimeLocalToString().c_str());
  lv_obj_set_x((lv_obj_t*)var, p);
}

/**
 * Start animation on an event
 */
void lv_example_anim_1(void)
{
#if 1
    static lv_style_t ft_style;
    static lv_ft_info_t ft_info;
    ft_info.name = "/tmp/app/exec/font/DroidSans.ttf";
    ft_info.weight = 128;
    ft_info.style = FT_FONT_STYLE_NORMAL;
    ft_info.mem = NULL;

    lv_freetype_init(128, 1, 0);
    if(!lv_ft_font_init(&ft_info)) {
      printf("get font_info failed, lv_ft_font_init error!\n");
      return;
    }

    lv_obj_t * label = lv_label_create(lv_scr_act());
    lv_style_set_text_font(&ft_style, ft_info.font);
    lv_obj_add_style(label, &ft_style, LV_STATE_DEFAULT);
    lv_label_set_text(label, "2022年05月24日!");
    lv_obj_set_size(label, 1200, 600);
    lv_obj_set_pos(label, 0, 0);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, label);
    lv_anim_set_values(&a, lv_obj_get_x(label), 1920);
    lv_anim_set_time(&a, 19200);
    lv_anim_set_exec_cb(&a, anim_x_cb);
    lv_anim_set_path_cb(&a, lv_anim_path_linear);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
#else
  lv_obj_t *image = lv_img_create(lv_scr_act());
  if (image) {
    lv_obj_set_width(image, LV_SIZE_CONTENT);
    lv_obj_set_height(image, LV_SIZE_CONTENT);
    lv_img_set_src(image, "A:/nfsroot/text_pic.png");
    lv_obj_set_pos(image, 40, 0);
    static lv_style_t style_image;
    lv_style_init(&style_image);
    lv_style_set_bg_opa(&style_image, LV_OPA_TRANSP); // 不透明
    lv_style_set_border_width(&style_image, 2);
    lv_style_set_border_color(&style_image, lv_color_make(0xff, 0x00, 0x00));
    lv_obj_add_style(image, &style_image, LV_STATE_DEFAULT);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, image);
    lv_anim_set_values(&a, lv_obj_get_x(image), 1920);
    lv_anim_set_time(&a, 19200);
    lv_anim_set_exec_cb(&a, anim_x_cb);
    lv_anim_set_path_cb(&a, lv_anim_path_linear);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
  }
#endif

}

#endif

/**
 * Open a PNG image from a file and a variable
 */
void lv_example_png_1(void)
{
  LV_IMG_DECLARE(img_wink_png);
  lv_obj_t * img;

  img = lv_img_create(lv_scr_act());
  // lv_img_set_src(img, &img_wink_png);
  // lv_obj_align(img, LV_ALIGN_LEFT_MID, 20, 0);
  //
  // img = lv_img_create(lv_scr_act());
  /* Assuming a File system is attached to letter 'A'
   * E.g. set LV_USE_FS_STDIO 'A' in lv_conf.h */
  lv_img_set_src(img, "A:/nfsroot/text_pic.png");
  lv_obj_align(img, LV_ALIGN_RIGHT_MID, -20, 0);
}

int main(int argc, char *argv[])
{
  /*LittlevGL init*/
  lv_init();

  /*Linux frame buffer device init*/
  fbdev_init();
  // fbdev_set_colorkey(true, lv_color_make(0xff, 0xc3, 0xc0));

  /*A small buffer for LittlevGL to draw the screen's content*/
  static lv_color_t buf1[DISP_BUF_SIZE];

  /*Initialize a descriptor for the buffer*/
  static lv_disp_draw_buf_t disp_buf;
  lv_disp_draw_buf_init(&disp_buf, buf1, NULL, DISP_BUF_SIZE);

  /*Initialize and register a display driver*/
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.draw_buf   = &disp_buf;
  disp_drv.flush_cb   = fbdev_flush;
  disp_drv.hor_res    = fbdev_get_width();
  disp_drv.ver_res    = fbdev_get_height();
#ifdef ENABLE_FB_DOUBLE_BUFFER
  // NOTE：如果使用framebuffer的双buffer模式，必须使用完全刷新
  disp_drv.full_refresh = 1;
#endif
  lv_disp_drv_register(&disp_drv);

  lv_example_anim_1();
  // lv_example_png_1();

  /*Handle LitlevGL tasks (tickless mode)*/
  while(1) {
    lv_timer_handler();
#if LV_TICK_CUSTOM == 0
    lv_tick_inc(5);
#endif
    usleep(5000);
  }
  return 0;
}

