/*****************************************************************************
  | File        :   LVGL_Driver.c
  
  | help        : 
    The provided LVGL library file must be installed first
******************************************************************************/
#include "LVGL_Driver.h"

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[ LVGL_BUF_LEN ];
static lv_color_t buf2[ LVGL_BUF_LEN ];
// static lv_color_t* buf1 = (lv_color_t*) heap_caps_malloc(LVGL_BUF_LEN, MALLOC_CAP_SPIRAM);
// static lv_color_t* buf2 = (lv_color_t*) heap_caps_malloc(LVGL_BUF_LEN, MALLOC_CAP_SPIRAM);
    


/* Serial debugging */
void Lvgl_print(const char * buf)
{
    // Serial.printf(buf);
    // Serial.flush();
}
void Lvgl_port_rounder_callback(struct _lv_disp_drv_t * disp_drv, lv_area_t * area)
{
  uint16_t x1 = area->x1;
  uint16_t x2 = area->x2;

  // round the start of coordinate down to the nearest 4M number
  area->x1 = (x1 >> 2) << 2;

  // round the end of coordinate up to the nearest 4N+3 number
  area->x2 = ((x2 >> 2) << 2) + 3;
}
/*  Display flushing 
    Displays LVGL content on the LCD
    This function implements associating LVGL data to the LCD screen
*/
// Copy of the last frame (RGB565, as drawn) for the USB screenshot command.
// LCD_addWindow byte-swaps the buffer in place, so the copy is taken first.
uint16_t* Lvgl_ScreenCopy = nullptr;
void Lvgl_EnableScreenCopy(void)
{
  if (!Lvgl_ScreenCopy)
    Lvgl_ScreenCopy = (uint16_t*)heap_caps_calloc(LCD_WIDTH * LCD_HEIGHT, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}
void Lvgl_Display_LCD( lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p )
{
  if (Lvgl_ScreenCopy && area->x1 >= 0 && area->y1 >= 0 && area->x2 < LCD_WIDTH && area->y2 < LCD_HEIGHT) {
    const int width = area->x2 - area->x1 + 1;
    const uint16_t* source = (const uint16_t*)&color_p->full;
    for (int y = area->y1; y <= area->y2; ++y)
      memcpy(&Lvgl_ScreenCopy[y * LCD_WIDTH + area->x1], source + (size_t)(y - area->y1) * width, (size_t)width * 2);
  }
  LCD_addWindow(area->x1, area->y1, area->x2, area->y2, ( uint16_t *)&color_p->full);
  lv_disp_flush_ready( disp_drv );
}
/*Read the touchpad*/
void Lvgl_Touchpad_Read( lv_indev_drv_t * indev_drv, lv_indev_data_t * data )
{
  bool tp_pressed = false;
  uint16_t tp_x = 0;
  uint16_t tp_y = 0;
  uint8_t tp_cnt = 0;
  // LVGL's pointer input and these scalar coordinates accept one touch point.
  tp_pressed = Touch_Get_xy(&tp_x, &tp_y, NULL, &tp_cnt, 1);
  if (tp_pressed && (tp_cnt > 0)) {
    data->point.x = tp_x;
    data->point.y = tp_y;
    data->state = LV_INDEV_STATE_PR;
    // printf("LVGL : X=%u Y=%u points=%d\r\n",  tp_x , tp_y,tp_cnt);
  }else {
    data->state = LV_INDEV_STATE_REL;
  }

}
void example_increase_lvgl_tick(void *arg)
{
    /* Tell LVGL how many milliseconds has elapsed */
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}
void Lvgl_Init(void)
{
  lv_init();
  lv_disp_draw_buf_init( &draw_buf, buf1, buf2, LVGL_BUF_LEN);

  /*Initialize the display*/
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init( &disp_drv );
  /*Change the following line to your display resolution*/
  disp_drv.hor_res = LCD_WIDTH;
  disp_drv.ver_res = LCD_HEIGHT;
  disp_drv.flush_cb = Lvgl_Display_LCD;
  disp_drv.rounder_cb = Lvgl_port_rounder_callback;
  disp_drv.full_refresh = 1;                    /**< 1: Always make the whole screen redrawn*/
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register( &disp_drv );

  /*Initialize the (dummy) input device driver*/
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init( &indev_drv );
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = Lvgl_Touchpad_Read;
  lv_indev_drv_register( &indev_drv );

  /* Create simple label */
  lv_obj_t *label = lv_label_create( lv_scr_act() );
  lv_label_set_text( label, "Hello Ardino and LVGL!");
  lv_obj_align( label, LV_ALIGN_CENTER, 0, 0 );

  const esp_timer_create_args_t lvgl_tick_timer_args = {
    .callback = &example_increase_lvgl_tick,
    .name = "lvgl_tick"
  };
  esp_timer_handle_t lvgl_tick_timer = NULL;
  esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer);
  esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000);

}
void Lvgl_Loop(void)
{
  lv_timer_handler(); /* let the GUI do its work */
  // delay( 5 );
}
