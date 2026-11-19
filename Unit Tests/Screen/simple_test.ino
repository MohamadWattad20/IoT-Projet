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
      cfg.freq_write  = 40000000;
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
      cfg.invert           = false;
      cfg.rgb_order        = false;
      cfg.dlen_16bit       = false;
      cfg.bus_shared       = true;
      _panel_instance.config(cfg);
    }

    // 3. Touch Controller configuration (XPT2046 over SPI)[cite: 5, 9]
    // Reversed X and Y ranges compared to default (min <-> max)
    {
      auto cfg = _touch_instance.config();
      cfg.x_min      = 3900;
      cfg.x_max      = 300;
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

LGFX lcd;
uint16_t calData[8];

void drawTestScreen() {
  lcd.fillScreen(TFT_BLACK);

  lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  lcd.setTextSize(2);
  lcd.setCursor(16, 18);
  lcd.println("Calibrated Touch Test");

  lcd.setTextSize(1);
  lcd.setCursor(16, 48);
  lcd.println("Tap anywhere to verify X/Y...");
  lcd.drawRect(70, 80, 180, 100, TFT_CYAN);
}

void setup() {
  Serial.begin(115200);
  delay(200);

  lcd.init();
  // Rotation 3 is 180 degrees from rotation 1 (320x240 landscape, upside-down flip)
  lcd.setRotation(3);
  lcd.fillScreen(TFT_BLACK);

  lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
  lcd.setTextSize(2);
  lcd.setCursor(20, 90);
  lcd.println("Tap the target boxes");
  lcd.setTextSize(1);
  lcd.setCursor(20, 120);
  lcd.println("to calibrate touch...");

  // Draws calibration crosshairs/boxes and computes transform into calData
  lcd.calibrateTouch(calData, TFT_WHITE, TFT_BLACK, 20);
  lcd.setTouchCalibrate(calData);

  Serial.print("Calibration data: { ");
  for (int i = 0; i < 8; i++) {
    Serial.print(calData[i]);
    if (i < 7) Serial.print(", ");
  }
  Serial.println(" }");

  drawTestScreen();
}

void loop() {
  uint16_t x = 0, y = 0;
  if (lcd.getTouch(&x, &y)) {
    lcd.fillRect(16, 195, 290, 30, TFT_BLACK);
    lcd.setTextColor(TFT_GREEN, TFT_BLACK);
    lcd.setTextSize(2);
    lcd.setCursor(16, 200);
    lcd.printf("X: %3d   Y: %3d", x, y);

    lcd.fillCircle(x, y, 4, TFT_YELLOW);
    Serial.printf("Calibrated Touch -> X: %d, Y: %d\n", x, y);
    delay(40);
  }
}