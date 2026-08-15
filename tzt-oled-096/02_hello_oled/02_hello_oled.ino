/*
 * 02_hello_oled - Adafruit_SSD1306 기본 출력
 *
 * 텍스트 크기, 반전, 커서 위치, 카운터 갱신까지 한 화면에서 확인한다.
 *
 * 라이브러리: Adafruit SSD1306, Adafruit GFX Library, Adafruit BusIO
 *
 * 주의(Uno): Adafruit_SSD1306은 128*64/8 = 1024바이트 프레임버퍼를 잡는다.
 * 이건 begin() 안에서 malloc으로 할당되므로 컴파일 결과의 "전역 변수" 수치에
 * 나타나지 않는다. 이 스케치는 전역 527바이트로 보고되지만 런타임에는
 * 힙에서 1024바이트를 더 먹어 실제 여유는 500바이트 수준이다.
 * (실측: 라이브러리 소스 Adafruit_SSD1306.cpp:499)
 * 컴파일은 되는데 실행 중 화면이 안 켜지거나 리셋되면 이 malloc이 실패한 것이다.
 * 그 경우 04_u8g2_bigfont의 페이지 버퍼 모드로 옮길 것.
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1     // I2C 모듈은 RESET 핀이 없으므로 -1
#define SCREEN_ADDRESS 0x3C  // 01_i2c_scanner 결과에 맞출 것 (0x3C 또는 0x3D)

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
  Serial.begin(9600);

  // 두 번째 인자 true는 라이브러리가 내부 charge pump를 켜라는 뜻.
  // TZT 모듈처럼 외부 전원 없는 보드는 반드시 true여야 화면이 켜진다.
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 init failed - check address/wiring"));
    for (;;) {
      ;  // 여기서 멈춘다. 01_i2c_scanner로 돌아갈 것.
    }
  }

  display.clearDisplay();

  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("TZT 0.96\" OLED"));
  display.println(F("SSD1306 / I2C"));

  display.setTextSize(2);
  display.setCursor(0, 24);
  display.println(F("Hello!"));

  // 반전 텍스트: 배경색과 전경색을 뒤집어 지정
  display.setTextSize(1);
  display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  display.setCursor(0, 44);
  display.print(F(" inverted text "));
  display.setTextColor(SSD1306_WHITE);

  // display()를 호출해야 RAM 버퍼가 실제 패널로 전송된다.
  display.display();

  delay(2000);
}

void loop() {
  static unsigned long counter = 0;

  display.clearDisplay();

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("uptime counter"));

  display.setTextSize(3);
  display.setCursor(0, 24);
  display.print(counter);

  display.setTextSize(1);
  display.setCursor(0, 56);
  display.print(millis() / 1000);
  display.print(F(" s"));

  display.display();

  counter++;
  delay(500);
}
