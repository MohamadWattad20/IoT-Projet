#define LV_CONF_SKIP
#include <lvgl.h>
#include <LovyanGFX.hpp>
#include <unity.h>

// ==========================================
// 1. Display & Touch Hardware Configuration
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
      cfg.freq_write  = 20000000;
      cfg.freq_read   = 16000000;
      cfg.spi_3wire   = false;
      cfg.use_lock    = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk    = 18;
      cfg.pin_mosi    = 23;
      cfg.pin_miso    = 19;
      cfg.pin_dc      = 15; // Slide 9 ICST breakout
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }
    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs           = 5;  // Slide 9 ICST breakout
      cfg.pin_rst          = 4;  // Slide 9 ICST breakout[cite: 9]
      cfg.pin_busy         = -1;
      cfg.panel_width      = 240;
      cfg.panel_height     = 320;
      cfg.readable         = true;
      cfg.invert           = true;
      cfg.rgb_order        = false;
      cfg.bus_shared       = true;
      _panel_instance.config(cfg);
    }
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
      cfg.pin_cs     = 21; // Slide 9 ICST breakout[cite: 9]
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
static uint8_t draw_buf[SCREEN_W * 10 * 2];

LGFX lcd;
lv_obj_t* pages[TOTAL_PAGES];
lv_obj_t* prev_btns[TOTAL_PAGES];
lv_obj_t* next_btns[TOTAL_PAGES];
lv_obj_t* page2_slider = nullptr;
lv_obj_t* page2_switch = nullptr;
int currentPage = 0;

// ==========================================
// 2. Testable Navigation & Input Helpers
// ==========================================
bool go_to_page(int targetPage) {
  if (targetPage < 0 || targetPage >= TOTAL_PAGES) {
    return false; // Reject out-of-bounds navigation
  }
  currentPage = targetPage;
  lv_screen_load_anim(pages[currentPage], LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
  return true;
}

// Clamps raw/noisy touch coordinates to valid 320x240 display bounds
void sanitize_touch_point(int32_t raw_x, int32_t raw_y, uint16_t* out_x, uint16_t* out_y) {
  if (raw_x < 0) raw_x = 0;
  if (raw_x >= SCREEN_W) raw_x = SCREEN_W - 1;
  if (raw_y < 0) raw_y = 0;
  if (raw_y >= SCREEN_H) raw_y = SCREEN_H - 1;
  *out_x = (uint16_t)raw_x;
  *out_y = (uint16_t)raw_y;
}

static uint32_t lvgl_tick_cb(void) { return millis(); }

static void lvgl_flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;
  lcd.startWrite();
  lcd.pushImage(area->x1, area->y1, w, h, (lgfx::rgb565_t*)px_map);
  lcd.endWrite();
  lv_display_flush_ready(disp);
}

void build_test_ui() {
  for (int i = 0; i < TOTAL_PAGES; i++) {
    pages[i] = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(pages[i], lv_color_black(), 0);

    prev_btns[i] = lv_button_create(pages[i]);
    lv_obj_set_size(prev_btns[i], 136, 40);
    lv_obj_set_pos(prev_btns[i], 14, 188);
    if (i == 0) lv_obj_add_state(prev_btns[i], LV_STATE_DISABLED);

    next_btns[i] = lv_button_create(pages[i]);
    lv_obj_set_size(next_btns[i], 136, 40);
    lv_obj_set_pos(next_btns[i], 170, 188);
    if (i == TOTAL_PAGES - 1) lv_obj_add_state(next_btns[i], LV_STATE_DISABLED);

    if (i == 1) {
      page2_switch = lv_switch_create(pages[i]);
      page2_slider = lv_slider_create(pages[i]);
      lv_slider_set_range(page2_slider, 0, 100);
      lv_slider_set_value(page2_slider, 50, LV_ANIM_OFF);
    }
  }
}

// ==========================================
// 3. Unit Tests & Edge Cases
// ==========================================
void setUp(void) {
  // Reset to Page 0 before each test
  go_to_page(0);
  lv_timer_handler();
}

void tearDown(void) {}

void test_initial_state_and_dimensions(void) {
  TEST_ASSERT_EQUAL_INT(0, currentPage);
  TEST_ASSERT_EQUAL_PTR(pages[0], lv_screen_active());
  TEST_ASSERT_EQUAL_INT(320, lv_display_get_horizontal_resolution(NULL));
  TEST_ASSERT_EQUAL_INT(240, lv_display_get_vertical_resolution(NULL));
}

void test_forward_and_backward_navigation(void) {
  TEST_ASSERT_TRUE(go_to_page(1));
  TEST_ASSERT_EQUAL_INT(1, currentPage);
  TEST_ASSERT_EQUAL_PTR(pages[1], lv_screen_active());

  TEST_ASSERT_TRUE(go_to_page(3));
  TEST_ASSERT_EQUAL_INT(3, currentPage);
  TEST_ASSERT_EQUAL_PTR(pages[3], lv_screen_active());

  TEST_ASSERT_TRUE(go_to_page(2));
  TEST_ASSERT_EQUAL_INT(2, currentPage);
}

void test_edge_case_underflow_and_overflow_bounds(void) {
  // Edge Case 1: Trying to go before Page 0
  TEST_ASSERT_FALSE(go_to_page(-1));
  TEST_ASSERT_FALSE(go_to_page(-999));
  TEST_ASSERT_EQUAL_INT(0, currentPage);

  // Edge Case 2: Trying to go past last page (TOTAL_PAGES = 4 -> max index 3)
  go_to_page(TOTAL_PAGES - 1);
  TEST_ASSERT_FALSE(go_to_page(TOTAL_PAGES));
  TEST_ASSERT_FALSE(go_to_page(100));
  TEST_ASSERT_EQUAL_INT(3, currentPage);
}

void test_edge_case_button_disabled_states(void) {
  // Page 0: PREV must be disabled, NEXT must be enabled
  TEST_ASSERT_TRUE(lv_obj_has_state(prev_btns[0], LV_STATE_DISABLED));
  TEST_ASSERT_FALSE(lv_obj_has_state(next_btns[0], LV_STATE_DISABLED));

  // Middle pages: both PREV and NEXT must be enabled
  TEST_ASSERT_FALSE(lv_obj_has_state(prev_btns[1], LV_STATE_DISABLED));
  TEST_ASSERT_FALSE(lv_obj_has_state(next_btns[1], LV_STATE_DISABLED));

  // Last page (Page 3): PREV must be enabled, NEXT must be disabled
  TEST_ASSERT_FALSE(lv_obj_has_state(prev_btns[3], LV_STATE_DISABLED));
  TEST_ASSERT_TRUE(lv_obj_has_state(next_btns[3], LV_STATE_DISABLED));
}

void test_edge_case_touch_coordinate_clamping(void) {
  uint16_t x = 0, y = 0;

  // Negative/underflow spike from noisy XPT2046 SPI read
  sanitize_touch_point(-25, -10, &x, &y);
  TEST_ASSERT_EQUAL_UINT16(0, x);
  TEST_ASSERT_EQUAL_UINT16(0, y);

  // Overflow spike beyond 320x240 bounds
  sanitize_touch_point(450, 999, &x, &y);
  TEST_ASSERT_EQUAL_UINT16(319, x);
  TEST_ASSERT_EQUAL_UINT16(239, y);

  // Valid interior coordinate
  sanitize_touch_point(160, 120, &x, &y);
  TEST_ASSERT_EQUAL_UINT16(160, x);
  TEST_ASSERT_EQUAL_UINT16(120, y);
}

void test_edge_case_widget_state_persistence_across_pages(void) {
  // Modify slider & switch on Page 2, switch away to Page 4, then return
  go_to_page(1);
  lv_slider_set_value(page2_slider, 85, LV_ANIM_OFF);
  lv_obj_add_state(page2_switch, LV_STATE_CHECKED);

  go_to_page(3);
  lv_timer_handler();
  go_to_page(1);
  lv_timer_handler();

  TEST_ASSERT_EQUAL_INT32(85, lv_slider_get_value(page2_slider));
  TEST_ASSERT_TRUE(lv_obj_has_state(page2_switch, LV_STATE_CHECKED));
}

void test_edge_case_rapid_page_stress_cycle(void) {
  // Rapidly cycle 100 transitions to ensure no heap leak or LVGL crash
  uint32_t free_heap_before = ESP.getFreeHeap();
  for (int i = 0; i < 100; i++) {
    go_to_page(i % TOTAL_PAGES);
    lv_timer_handler();
  }
  uint32_t free_heap_after = ESP.getFreeHeap();

  // Allow tiny variance, ensure no progressive memory leak
  TEST_ASSERT_INT32_WITHIN(1024, free_heap_before, free_heap_after);
}

// ==========================================
// 4. Test Runner Entry Points
// ==========================================
void setup() {
  Serial.begin(115200);
  delay(1000); // Wait for Serial Monitor connection

  lcd.init();
  lcd.setRotation(3);

  lv_init();
  lv_tick_set_cb(lvgl_tick_cb);

  lv_display_t* disp = lv_display_create(SCREEN_W, SCREEN_H);
  lv_display_set_flush_cb(disp, lvgl_flush_cb);
  lv_display_set_buffers(disp, draw_buf, NULL, sizeof(draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);

  build_test_ui();

  UNITY_BEGIN();
  RUN_TEST(test_initial_state_and_dimensions);
  RUN_TEST(test_forward_and_backward_navigation);
  RUN_TEST(test_edge_case_underflow_and_overflow_bounds);
  RUN_TEST(test_edge_case_button_disabled_states);
  RUN_TEST(test_edge_case_touch_coordinate_clamping);
  RUN_TEST(test_edge_case_widget_state_persistence_across_pages);
  RUN_TEST(test_edge_case_rapid_page_stress_cycle);
  UNITY_END();
}

void loop() {
  // All unit tests execute once in setup()
}