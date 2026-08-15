/*
 * 03_graphics_demo - 도형 그리기 + 애니메이션 + 스크롤
 *
 * Adafruit_GFX가 제공하는 기본 도형 API와 SSD1306 하드웨어 스크롤을
 * 순서대로 보여준다. 각 데모는 2초씩.
 *
 * 배선은 02_hello_oled와 동일.
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// 16x16 하트 비트맵. 1비트가 켜진 픽셀 하나.
static const unsigned char PROGMEM heart_bmp[] = {
  0x0E, 0x70, 0x1F, 0xF8, 0x3F, 0xFC, 0x7F, 0xFE,
  0x7F, 0xFE, 0x7F, 0xFE, 0x7F, 0xFE, 0x3F, 0xFC,
  0x3F, 0xFC, 0x1F, 0xF8, 0x1F, 0xF8, 0x0F, 0xF0,
  0x07, 0xE0, 0x03, 0xC0, 0x01, 0x80, 0x00, 0x00
};

void setup() {
  Serial.begin(9600);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 init failed"));
    for (;;) {
      ;
    }
  }

  display.clearDisplay();
  display.display();
}

void title(const __FlashStringHelper *text) {
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(text);
}

void demoLines() {
  display.clearDisplay();
  title(F("lines"));
  for (int16_t x = 0; x < SCREEN_WIDTH; x += 8) {
    display.drawLine(0, 12, x, SCREEN_HEIGHT - 1, SSD1306_WHITE);
    display.display();
    delay(20);
  }
  delay(800);
}

void demoShapes() {
  display.clearDisplay();
  title(F("shapes"));
  display.drawRect(0, 14, 40, 40, SSD1306_WHITE);
  display.fillRoundRect(46, 14, 36, 40, 8, SSD1306_WHITE);
  display.drawCircle(106, 34, 20, SSD1306_WHITE);
  display.fillTriangle(96, 50, 106, 22, 116, 50, SSD1306_INVERSE);
  display.display();
  delay(2000);
}

void demoBitmap() {
  display.clearDisplay();
  title(F("bitmap"));
  for (int8_t i = 0; i < 6; i++) {
    display.fillRect(0, 12, SCREEN_WIDTH, SCREEN_HEIGHT - 12, SSD1306_BLACK);
    // 두근거리듯 좌우로 세 개를 번갈아 표시
    for (uint8_t n = 0; n < 3; n++) {
      if ((i + n) % 2 == 0) {
        display.drawBitmap(20 + n * 32, 26, heart_bmp, 16, 16, SSD1306_WHITE);
      }
    }
    display.display();
    delay(300);
  }
}

void demoBounce() {
  display.clearDisplay();

  int16_t x = 10, y = 30;
  int8_t dx = 3, dy = 2;
  const int8_t r = 6;

  for (uint16_t frame = 0; frame < 120; frame++) {
    display.clearDisplay();
    title(F("bouncing ball"));

    x += dx;
    y += dy;
    if (x - r <= 0 || x + r >= SCREEN_WIDTH - 1) dx = -dx;
    if (y - r <= 12 || y + r >= SCREEN_HEIGHT - 1) dy = -dy;

    display.drawRect(0, 12, SCREEN_WIDTH, SCREEN_HEIGHT - 12, SSD1306_WHITE);
    display.fillCircle(x, y, r, SSD1306_WHITE);
    display.display();
    delay(16);
  }
}

void demoScroll() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(4, 24);
  display.print(F("SCROLL!"));
  display.display();

  // 하드웨어 스크롤은 패널 컨트롤러가 처리한다.
  // MCU가 프레임을 다시 그리지 않으므로 loop()를 점유하지 않는다.
  display.startscrollright(0x00, 0x0F);
  delay(2000);
  display.stopscroll();

  display.startscrollleft(0x00, 0x0F);
  delay(2000);
  display.stopscroll();

  display.setTextSize(1);
}

void loop() {
  demoLines();
  demoShapes();
  demoBitmap();
  demoBounce();
  demoScroll();
}
