#define LV_CONF_SKIP
#include <lvgl.h>
#include <LovyanGFX.hpp>
#include <Wire.h>
#include <RTClib.h>
#include <HX711.h>

// ==========================================
// 1. Peripherals Pins (Conflict-Free with Diagram)
// ==========================================
static const int HX711_DT_PIN  = 32;
static const int HX711_SCK_PIN = 33;
static const int I2C_SDA_PIN   = 22; // GPIO 21 is used by T_CS in diagram
static const int I2C_SCL_PIN   = 25;

// Adjust after testing with a known weight
static float hx711_scale_factor = 420.0f;

// ==========================================
// 2. LovyanGFX Setup (Matches Image Wiring)
// ==========================================
class LGFX : public lgfx::LGFX_Device {
  lgfx::Bus_SPI _bus_instance;
  lgfx::Panel_ILI9341 _panel_instance;
  lgfx::Touch_XPT2046 _touch_instance;

public:
  LGFX(void) {
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host    = VSPI_HOST;
      cfg.spi_mode    = 0;
      cfg.freq_write  = 24000000;
      cfg.freq_read   = 16000000;
      cfg.spi_3wire   = false;
      cfg.use_lock    = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO; // Offloads SPI transfers via DMA[cite: 10]
      cfg.pin_sclk    = 18;
      cfg.pin_mosi    = 23;
      cfg.pin_miso    = 19;
      cfg.pin_dc      = 15; // Slide 9 ICST breakout: GPIO 15 (Slide 5 table: GPIO 2)[cite: 5, 9]
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }

    // 2. Display Panel configuration (320x240 ILI9341)[cite: 5, 9]
    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs           = 5;  // Slide 9 ICST breakout: GPIO 5 (Slide 5 table: GPIO 15)[cite: 5, 9]
      cfg.pin_rst          = 4;
      cfg.pin_busy         = -1;
      cfg.panel_width      = 240;
      cfg.panel_height     = 320;
      cfg.offset_x         = 0;
      cfg.offset_y         = 0;
      cfg.offset_rotation  = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits  = 1;
      cfg.readable         = true;
      cfg.invert           = true;
      cfg.rgb_order        = false;
      cfg.dlen_16bit       = false;
      cfg.bus_shared       = true;
      _panel_instance.config(cfg);
    }

    // 3. Touch Controller configuration (XPT2046 over SPI)[cite: 5, 9]
    {
      auto cfg = _touch_instance.config();
      cfg.x_min      = 300;
      cfg.x_max      = 3900;
      cfg.y_min      = 3700;
      cfg.y_max      = 200;
      cfg.pin_int    = -1;
      cfg.bus_shared = true;
      cfg.spi_host   = VSPI_HOST;
      cfg.pin_sclk   = 18;
      cfg.pin_mosi   = 23;
      cfg.pin_miso   = 19;
      cfg.pin_cs     = 21; // Slide 9 ICST breakout: GPIO 21[cite: 9]
      cfg.freq       = 2500000;
      _touch_instance.config(cfg);
      _panel_instance.setTouch(&_touch_instance);
    }
    setPanel(&_panel_instance);
  }
};

// ==========================================
// 3. Globals & LVGL Objects
// ==========================================
static const uint16_t SCREEN_W = 320;
static const uint16_t SCREEN_H = 240;
static const int TOTAL_PAGES   = 4;
static uint8_t draw_buf[SCREEN_W * 10 * 2];

LGFX lcd;
RTC_DS3231 rtc;
HX711 scale;

bool rtc_ok = false;
int currentPage = 0;
lv_obj_t* pages[TOTAL_PAGES];

lv_obj_t* lbl_page1_clock  = nullptr;
lv_obj_t* lbl_page1_weight = nullptr;
lv_obj_t* lbl_page2_weight = nullptr;
lv_obj_t* lbl_page3_clock  = nullptr;

float latest_weight_g = 0.0f;
char latest_time_str[16] = "00:00:00";
uint32_t last_serial_ms = 0;

static uint32_t lvgl_tick_cb(void) { return millis(); }

static void lvgl_flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;

  lcd.startWrite();
  lcd.pushImage(area->x1, area->y1, w, h, (lgfx::rgb565_t*)px_map);
  lcd.endWrite();

  lv_display_flush_ready(disp);
}

static void lvgl_touch_read_cb(lv_indev_t* indev, lv_indev_data_t* data) {
  uint16_t x = 0, y = 0;
  if (lcd.getTouch(&x, &y)) {
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = x;
    data->point.y = y;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

static void go_to_page(int targetPage) {
  if (targetPage < 0 || targetPage >= TOTAL_PAGES) return;
  currentPage = targetPage;
  lv_screen_load_anim(pages[currentPage], LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
}

static void prev_btn_cb(lv_event_t* e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    go_to_page(currentPage - 1);
  }
}

static void next_btn_cb(lv_event_t* e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    go_to_page(currentPage + 1);
  }
}

static void tare_btn_cb(lv_event_t* e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    scale.tare();
    Serial.println("[HX711] Tared scale to 0.0 g");
  }
}

static void create_nav_bar(lv_obj_t* parent, int pageIdx) {
  lv_obj_t* header = lv_obj_create(parent);
  lv_obj_set_size(header, SCREEN_W, 34);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_set_style_bg_color(header, lv_color_hex(0x1565C0), 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_radius(header, 0, 0);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* title = lv_label_create(header);
  lv_label_set_text_fmt(title, "Page %d of %d", pageIdx + 1, TOTAL_PAGES);
  lv_obj_set_style_text_color(title, lv_color_white(), 0);
  lv_obj_align(title, LV_ALIGN_LEFT_MID, 6, 0);

  lv_obj_t* btn_prev = lv_button_create(parent);
  lv_obj_set_size(btn_prev, 136, 40);
  lv_obj_set_pos(btn_prev, 14, 188);
  lv_obj_t* lbl_prev = lv_label_create(btn_prev);
  lv_label_set_text(lbl_prev, "PREV");
  lv_obj_center(lbl_prev);
  if (pageIdx == 0) {
    lv_obj_add_state(btn_prev, LV_STATE_DISABLED);
  } else {
    lv_obj_add_event_cb(btn_prev, prev_btn_cb, LV_EVENT_CLICKED, NULL);
  }

  lv_obj_t* btn_next = lv_button_create(parent);
  lv_obj_set_size(btn_next, 136, 40);
  lv_obj_set_pos(btn_next, 170, 188);
  lv_obj_t* lbl_next = lv_label_create(btn_next);
  lv_label_set_text(lbl_next, "NEXT");
  lv_obj_center(lbl_next);
  if (pageIdx == TOTAL_PAGES - 1) {
    lv_obj_add_state(btn_next, LV_STATE_DISABLED);
  } else {
    lv_obj_add_event_cb(btn_next, next_btn_cb, LV_EVENT_CLICKED, NULL);
  }
}

static void build_pages(void) {
  for (int i = 0; i < TOTAL_PAGES; i++) {
    pages[i] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(pages[i], lv_color_black(), 0);
    lv_obj_clear_flag(pages[i], LV_OBJ_FLAG_SCROLLABLE);
    create_nav_bar(pages[i], i);

    if (i == 0) {
      lbl_page1_clock = lv_label_create(pages[i]);
      lv_label_set_text(lbl_page1_clock, "Time: 00:00:00");
      lv_obj_set_style_text_color(lbl_page1_clock, lv_color_white(), 0);
      lv_obj_set_pos(lbl_page1_clock, 18, 54);

      lbl_page1_weight = lv_label_create(pages[i]);
      lv_label_set_text(lbl_page1_weight, "Weight: 0.0 g");
      lv_obj_set_style_text_color(lbl_page1_weight, lv_color_hex(0x80CBC4), 0);
      lv_obj_set_pos(lbl_page1_weight, 18, 92);

      lv_obj_t* hint = lv_label_create(pages[i]);
      lv_label_set_text(hint, "Tap NEXT for Scale & Tare");
      lv_obj_set_style_text_color(hint, lv_color_hex(0xB0BEC5), 0);
      lv_obj_set_pos(hint, 18, 134);
    } else if (i == 1) {
      lbl_page2_weight = lv_label_create(pages[i]);
      lv_label_set_text(lbl_page2_weight, "Weight: 0.0 g");
      lv_obj_set_style_text_color(lbl_page2_weight, lv_color_white(), 0);
      lv_obj_set_pos(lbl_page2_weight, 18, 56);

      lv_obj_t* btn_tare = lv_button_create(pages[i]);
      lv_obj_set_size(btn_tare, 150, 38);
      lv_obj_set_pos(btn_tare, 18, 96);
      lv_obj_t* lbl_tare = lv_label_create(btn_tare);
      lv_label_set_text(lbl_tare, "TARE (ZERO)");
      lv_obj_center(lbl_tare);
      lv_obj_add_event_cb(btn_tare, tare_btn_cb, LV_EVENT_CLICKED, NULL);
    } else if (i == 2) {
      lbl_page3_clock = lv_label_create(pages[i]);
      lv_label_set_text(lbl_page3_clock, "RTC Time: 00:00:00");
      lv_obj_set_style_text_color(lbl_page3_clock, lv_color_white(), 0);
      lv_obj_set_pos(lbl_page3_clock, 18, 60);
    } else if (i == 3) {
      lv_obj_t* lbl = lv_label_create(pages[i]);
      lv_label_set_text(lbl, "Page 4: Ready\nUse PREV to go back.");
      lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
      lv_obj_set_pos(lbl, 18, 56);
    }
  }
}

static void update_rtc(void) {
  if (!rtc_ok) {
    snprintf(latest_time_str, sizeof(latest_time_str), "NO RTC");
    return;
  }
  DateTime now = rtc.now();
  snprintf(
    latest_time_str,
    sizeof(latest_time_str),
    "%02d:%02d:%02d",
    now.hour(),
    now.minute(),
    now.second()
  );
  Serial.print(now.hour());
  Serial.print(":");
  Serial.print(now.minute());
  Serial.print(":");
  Serial.println(now.second());
}

static void update_hx711(void) {
  if (scale.is_ready()) {
    float units = scale.get_units(3);
    if (fabs(units) < 0.2f) units = 0.0f;
    latest_weight_g = units;
  }
}

static void refresh_ui_labels(void) {
  char w_buf[32];
  snprintf(w_buf, sizeof(w_buf), "Weight: %.1f g", latest_weight_g);

  char t_buf1[32];
  snprintf(t_buf1, sizeof(t_buf1), "Time: %s", latest_time_str);

  char t_buf3[32];
  snprintf(t_buf3, sizeof(t_buf3), "RTC Time: %s", latest_time_str);

  if (lbl_page1_clock)  lv_label_set_text(lbl_page1_clock, t_buf1);
  if (lbl_page1_weight) lv_label_set_text(lbl_page1_weight, w_buf);
  if (lbl_page2_weight) lv_label_set_text(lbl_page2_weight, w_buf);
  if (lbl_page3_clock)  lv_label_set_text(lbl_page3_clock, t_buf3);
}

void setup() {
  Serial.begin(115200);
  delay(200);

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  rtc.begin();
  rtc_ok = true;
  //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
 // if (rtc.begin()) {
   // rtc_ok = true;
   // if (rtc.lostPower()) {
   //  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    //}
  //} else {
  //  rtc_ok = false;
  //  Serial.println("[RTC] DS3231 not found on I2C bus!");
  //}

  scale.begin(HX711_DT_PIN, HX711_SCK_PIN);
  scale.set_scale(hx711_scale_factor);
  scale.tare();

  lcd.init();
  lcd.setRotation(3);

  lv_init();
  lv_tick_set_cb(lvgl_tick_cb);

  lv_display_t* disp = lv_display_create(SCREEN_W, SCREEN_H);
  lv_display_set_flush_cb(disp, lvgl_flush_cb);
  lv_display_set_buffers(disp, draw_buf, NULL, sizeof(draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);

  lv_indev_t* indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, lvgl_touch_read_cb);

  build_pages();
  lv_screen_load(pages[0]);
}

void loop() {
  update_rtc();

  update_hx711();
  refresh_ui_labels();

  uint32_t now_ms = millis();
  if (now_ms - last_serial_ms >= 500) {
    last_serial_ms = now_ms;
    //Serial.printf("Time: %s | Weight: %.1f g\n", latest_time_str, latest_weight_g);
  }

  lv_timer_handler();
  delay(5);
}