
#include "lvgl_port.h"
#include <XPT2046_Touchscreen.h>
#include <SPI.h>

#include "wifi_manager.h"

#include <nvs_flash.h>
#include <nvs.h>

#include <time.h>

#include "qr_webserver.h"

/* Screen config */
#define TFT_HOR_RES   240
#define TFT_VER_RES   320
#define TFT_ROTATION  LV_DISPLAY_ROTATION_0

#define DRAW_BUF_SIZE (TFT_HOR_RES * TFT_VER_RES / 10 * (LV_COLOR_DEPTH / 8))
static uint32_t draw_buf[DRAW_BUF_SIZE / 4];
/* Touch config */
#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

static SPIClass tsSpi = SPIClass(VSPI);
static XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);

static uint16_t touchScreenMinimumX = 200;
static uint16_t touchScreenMaximumX = 3700;
static uint16_t touchScreenMinimumY = 240;
static uint16_t touchScreenMaximumY = 3800;

/*================ Display Flush ================*/
static void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    if (lv_color_swap_16) {
        size_t len = lv_area_get_size(area);
        lv_draw_sw_rgb565_swap(px_map, len);
    }

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t*)px_map, w * h, true);
    tft.endWrite();

    lv_disp_flush_ready(disp);
}

/*================ Touch Read ================*/
static void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    lv_indev_touch_data_t touches[1];
    int32_t touch_cnt = 0;
    
    if (ts.touched())
    {
        TS_Point p = ts.getPoint();

        if (p.x < touchScreenMinimumX) touchScreenMinimumX = p.x;
        if (p.x > touchScreenMaximumX) touchScreenMaximumX = p.x;
        if (p.y < touchScreenMinimumY) touchScreenMinimumY = p.y;
        if (p.y > touchScreenMaximumY) touchScreenMaximumY = p.y;

        int x = map(p.x, touchScreenMinimumX, touchScreenMaximumX, 0, TFT_HOR_RES);
        int y = map(p.y, touchScreenMinimumY, touchScreenMaximumY, 0, TFT_VER_RES);
        
        touches[0].point.x = x;
        touches[0].point.y = y;
        touches[0].state = LV_INDEV_STATE_PRESSED;
        touch_cnt = 1;
        
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PR;
    }
    else
    {
        touches[0].state = LV_INDEV_STATE_RELEASED;
        touch_cnt = 0;
        data->state = LV_INDEV_STATE_REL;
    }
    
    lv_indev_gesture_recognizers_update(indev, touches, touch_cnt);
    
}

/*================ Tick ================*/
static uint32_t my_tick(void)
{
    return millis();
}

/*================ Public API ================*/

/** Screen brightness dimmed */
void TFT_SET_BL(uint8_t Value) {
  if (Value > 100) {
    printf("TFT_SET_BL Error \r\n");
    return;
  }
  uint8_t minValue = 5;  
  if (Value < minValue) Value = minValue;

  uint8_t pwmValue = (uint8_t)(Value * 2.55);  
  analogWrite(TFT_BL, pwmValue);
}

/** Initialize LVGL */
void lvgl_begin()
    {
        Serial.println("Init LVGL...");

        lv_init();
        lv_tick_set_cb(my_tick);

        lv_display_t *disp = lv_tft_espi_create(
            TFT_HOR_RES,
            TFT_VER_RES,
            draw_buf,
            sizeof(draw_buf)
        );

        lv_display_set_rotation(disp, TFT_ROTATION);

        /* Touch init */
        tsSpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
        ts.begin(tsSpi);
        ts.setRotation(0);

        lv_indev_t *indev = lv_indev_create();
        lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(indev, my_touchpad_read);

        TFT_SET_BL(5);
        nvs_flash_init();

        char ssid[64], pwd[64];
        memset(ssid, 0, sizeof(ssid)); // ← Clear buffer 
        memset(pwd, 0, sizeof(pwd));
        if (load_wifi_credentials(ssid, pwd)) {
            start_wifi_connect(ssid, pwd);  // Auto-connect
        } else {
            // Show WiFi list UI
        }
        lv_async_call(qr_task, NULL);

        Serial.println("LVGL Ready");
    }

/** Update LVGL */
void lvgl_handler()
    {
        lv_timer_handler();
        delay(5); 
    }
