#include <lvgl.h>
#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device {
  lgfx::Bus_SPI _bus_instance;
  lgfx::Panel_ILI9341 _panel_instance;
  lgfx::Touch_XPT2046 _touch_instance;

public:
  LGFX(void) {
    // 1. SPI Bus configuration (shared by display and touch)[cite: 5, 9]
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

static const uint16_t SCREEN_W = 320;
static const uint16_t SCREEN_H = 240;
static const int TOTAL_PAGES   = 4;

// 10-line partial draw buffer in RGB565 (320 * 10 * 2 bytes = 6.4 KB)[cite: 10]
static uint8_t draw_buf[SCREEN_W * 10 * 2];

LGFX lcd;
lv_obj_t* pages[TOTAL_PAGES];
int currentPage = 0;

static uint32_t lvgl_tick_cb(void) {
  return millis();
}

// Push LVGL rendered pixels to LovyanGFX
static void lvgl_flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;

  lcd.startWrite();
  lcd.pushImage(area->x1, area->y1, w, h, (lgfx::rgb565_t*)px_map);
  lcd.endWrite();

  lv_display_flush_ready(disp);
}

// Read touch coordinates from LovyanGFX for LVGL
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

static void slider_event_cb(lv_event_t* e) {
  lv_obj_t* slider = (lv_obj_t*)lv_event_get_target(e);
  lv_obj_t* val_label = (lv_obj_t*)lv_event_get_user_data(e);
  lv_label_set_text_fmt(val_label, "Value: %d%%", (int)lv_slider_get_value(slider));
}

static void create_nav_bar(lv_obj_t* parent, int pageIdx) {
  // Top header
  lv_obj_t* header = lv_obj_create(parent);
  lv_obj_set_size(header, SCREEN_W, 34);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_set_style_bg_color(header, lv_color_hex(0x1565C0), 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_radius(header, 0, 0);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* title = lv_label_create(header);
  lv_label_set_text_fmt(title, "LVGL Page %d of %d", pageIdx + 1, TOTAL_PAGES);
  lv_obj_set_style_text_color(title, lv_color_white(), 0);
  lv_obj_align(title, LV_ALIGN_LEFT_MID, 6, 0);

  // PREV button
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

  // NEXT button
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
    lv_obj_set_style_bg_color(pages[i], lv_color_hex(0x101418), 0);
    lv_obj_clear_flag(pages[i], LV_OBJ_FLAG_SCROLLABLE);
    create_nav_bar(pages[i], i);

    lv_obj_set_style_bg_color(pages[i], lv_color_black(), 0);

    if (i == 0) {
      lv_obj_t* lbl = lv_label_create(pages[i]);
      lv_label_set_text(lbl, "Page 1: Overview\nTap NEXT to browse pages.");
      lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
      lv_obj_set_pos(lbl, 18, 56);
    } else if (i == 1) {
      lv_obj_t* sw = lv_switch_create(pages[i]);
      lv_obj_set_pos(sw, 18, 52);

      lv_obj_t* sw_lbl = lv_label_create(pages[i]);
      lv_label_set_text(sw_lbl, "Toggle Switch");
      lv_obj_set_style_text_color(sw_lbl, lv_color_white(), 0);
      lv_obj_set_pos(sw_lbl, 84, 58);

      lv_obj_t* val_lbl = lv_label_create(pages[i]);
      lv_label_set_text(val_lbl, "Value: 50%");
      lv_obj_set_style_text_color(val_lbl, lv_color_hex(0x80CBC4), 0);
      lv_obj_set_pos(val_lbl, 18, 96);

      lv_obj_t* slider = lv_slider_create(pages[i]);
      lv_obj_set_size(slider, 260, 14);
      lv_obj_set_pos(slider, 18, 126);
      lv_slider_set_value(slider, 50, LV_ANIM_OFF);
      lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, val_lbl);
    } else if (i == 2) {
      lv_obj_t* lbl = lv_label_create(pages[i]);
      lv_label_set_text(lbl, "Page 3: Status / Info\nReady for custom LVGL widgets.");
      lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFD54F), 0);
      lv_obj_set_pos(lbl, 18, 56);
    } else if (i == 3) {
      lv_obj_t* lbl = lv_label_create(pages[i]);
      lv_label_set_text(lbl, "Page 4: Final Screen\nTap PREV to go back.");
      lv_obj_set_style_text_color(lbl, lv_color_hex(0x81C784), 0);
      lv_obj_set_pos(lbl, 18, 56);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  lcd.init();
  lcd.setRotation(3); // 320x240 landscape[cite: 5, 9]

  // Optional: run lcd.calibrateTouch(calData, ...) once if needed
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
  lv_timer_handler();
  delay(5);
}