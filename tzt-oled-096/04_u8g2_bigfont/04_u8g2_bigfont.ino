/*
 * 04_u8g2_bigfont - U8g2 페이지 버퍼 모드 (Uno RAM 절약)
 *
 * Adafruit_SSD1306은 begin()에서 1024바이트를 malloc하지만,
 * U8g2의 _1_ 생성자는 화면을 8줄씩 나눠 그려서 약 128바이트 버퍼만 쓴다.
 * Uno(SRAM 2KB)에서 다른 라이브러리와 같이 쓸 때 이쪽이 훨씬 편하다.
 *
 * 실측 비교 (arduino:avr:uno):
 *   02_hello_oled  전역 527B + 힙 1024B = 실사용 약 1551B
 *   04_u8g2_bigfont 전역 704B + 힙 0B   = 실사용 약 704B
 * 컴파일 리포트의 전역 수치만 보면 U8g2가 더 커 보이지만,
 * Adafruit 쪽 1024B는 리포트에 안 잡히는 힙이라 실제로는 반대다.
 *
 * 생성자 이름 규칙:
 *   U8G2_SSD1306_128X64_NONAME_1_HW_I2C  <- 1페이지 (약 128B, 가장 절약)
 *   U8G2_SSD1306_128X64_NONAME_2_HW_I2C  <- 2페이지 (약 256B)
 *   U8G2_SSD1306_128X64_NONAME_F_HW_I2C  <- Full (1024B, Adafruit와 동일)
 *
 * 트레이드오프: 페이지 모드는 firstPage()/nextPage() 루프 안에서
 * 그리기 코드가 페이지 수만큼 반복 실행된다. 그리기 로직에
 * 부작용(카운터 증가 등)을 넣으면 안 된다.
 */

#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>

// 마지막 인자는 RESET 핀. I2C 모듈은 U8X8_PIN_NONE.
U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

void setup() {
  u8g2.begin();
  // 주소가 0x3D인 모듈이면 아래 주석을 해제한다 (U8g2는 8비트 주소를 쓴다).
  // u8g2.setI2CAddress(0x3D * 2);
}

void loop() {
  // 그릴 값은 루프 바깥에서 미리 확정한다.
  // firstPage/nextPage 블록은 페이지마다 재실행되기 때문이다.
  unsigned long sec = millis() / 1000;
  char timeBuf[12];
  snprintf(timeBuf, sizeof(timeBuf), "%02lu:%02lu", (sec / 60) % 100, sec % 60);

  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(0, 8, "U8g2 page mode");

    u8g2.drawHLine(0, 12, 128);

    // 큰 숫자 폰트
    u8g2.setFont(u8g2_font_logisoso24_tn);
    u8g2.drawStr(18, 44, timeBuf);

    u8g2.setFont(u8g2_font_5x8_tf);
    u8g2.drawStr(0, 62, "RAM: ~128B buffer");

    // 진행 바
    u8g2.drawFrame(86, 55, 40, 8);
    u8g2.drawBox(88, 57, (sec % 10) * 36 / 10, 4);
  } while (u8g2.nextPage());

  delay(200);
}
