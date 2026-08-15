/*
 * 01_i2c_scanner - I2C 주소 스캐너
 *
 * 가장 먼저 돌려볼 스케치. 화면에 아무것도 안 나올 때
 * "배선 문제인가 코드 문제인가"를 여기서 가른다.
 *
 * 기대 출력: I2C device found at address 0x3C  (모듈에 따라 0x3D)
 * 아무것도 안 나오면 배선/전원 문제이므로 다음 스케치로 넘어가지 말 것.
 *
 * 배선 (Uno / Nano):
 *   OLED VCC -> 5V (모듈에 레귤레이터가 있으면 5V, 없으면 3.3V)
 *   OLED GND -> GND
 *   OLED SCL -> A5
 *   OLED SDA -> A4
 */

#include <Wire.h>

void setup() {
  Wire.begin();
  Serial.begin(9600);
  while (!Serial) {
    ;  // Leonardo/Micro 계열에서 시리얼 준비 대기
  }
  Serial.println(F("\nI2C Scanner"));
}

void loop() {
  byte count = 0;

  Serial.println(F("Scanning..."));

  for (byte address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    byte error = Wire.endTransmission();

    if (error == 0) {
      Serial.print(F("I2C device found at address 0x"));
      if (address < 16) Serial.print('0');
      Serial.println(address, HEX);
      count++;
    }
  }

  if (count == 0) {
    Serial.println(F("No I2C devices found - check wiring/power"));
  } else {
    Serial.print(count);
    Serial.println(F(" device(s) found"));
  }

  delay(3000);
}
