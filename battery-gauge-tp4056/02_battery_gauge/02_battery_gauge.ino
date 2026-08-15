/*
 * 02_battery_gauge - 18650 잔량 게이지 (TP4056 + 0.96" OLED)
 *
 * 아두이노는 충전에 관여하지 않는다. TP4056이 CC/CV 충전을 전담하고
 * 이 스케치는 전압을 읽어 표시만 한다. 계기판이지 충전기가 아니다.
 *
 * 화면 구성 (128x64):
 *   3.85V              CHRG     <- 전압(좌) / 충전상태(우)
 *  +----------------------+
 *  |##############        |     <- 잔량 바
 *  +----------------------+
 *          62 %                 <- 큰 숫자
 *
 * 라이브러리: U8g2
 *
 * ⚠️ Adafruit_SSD1306 이 아니라 U8g2 페이지 모드를 쓰는 이유
 * Adafruit 쪽은 begin() 안에서 1024바이트 프레임버퍼를 malloc 한다.
 * 이 스케치는 이동평균 버퍼와 ADC 코드까지 얹기 때문에 UNO(SRAM 2KB)에서
 * 여유가 빠듯해진다. U8g2 의 _1_ 생성자는 약 128바이트 페이지 버퍼만 쓴다.
 *
 * ⚠️ firstPage()/nextPage() 블록은 페이지 수만큼(8회) 반복 실행된다.
 * 그 안에 측정이나 카운터 증가를 넣으면 안 된다.
 * 그릴 값은 전부 루프 진입 전에 확정한다.
 */

#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>

// ---- 설정 -------------------------------------------------------------

U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

const uint8_t PIN_BATTERY = A0;
const uint8_t PIN_CHRG = 2;  // TP4056 CHRG 패드 (선택)

// 01_voltage_monitor 에서 구한 값을 여기 적는다.
//
// 이론값은 2.000 (같은 값 저항 두 개) 이지만 저항 오차가 실린다.
// 이 보드 실측: 게이지가 4125mV 로 읽을 때 멀티미터는 4095mV.
//   보정 = 2.000 * (4095 / 4125) = 1.985
// 저항을 바꾸면 다시 맞춰야 한다.
float DIVIDER_RATIO = 1.985;

// 이동평균 창 크기. 키우면 안정적이지만 반응이 느려진다
const uint8_t SMOOTH_WINDOW = 8;

// ---- 방전 곡선 --------------------------------------------------------
//
// 리튬이온의 어려운 점이 여기 있다. 방전 곡선이 중간에서 평탄해서
// 3.7V 부근은 ±0.05V만 흔들려도 잔량 추정이 20%씩 튄다.
// 선형 변환(3.0V=0%, 4.2V=100%)을 쓰면 "80%에서 한참 머물다가
// 갑자기 훅 떨어지는" 부정확한 게이지가 된다.
//
// 아래는 18650 무부하 기준 방전 곡선을 옮긴 표다. 그래도 추정이다.
// 정확한 잔량이 필요하면 전압이 아니라 전류 적산(쿨롱 카운팅)을 하는
// fuel gauge IC(MAX17043 등)를 써야 한다.

struct SocPoint {
  uint16_t mv;
  uint8_t soc;
};

static const SocPoint PROGMEM SOC_TABLE[] = {
  {4200, 100}, {4150, 95}, {4110, 90}, {4080, 85}, {4020, 80},
  {3980, 75},  {3950, 70}, {3910, 65}, {3870, 60}, {3850, 55},
  {3840, 50},  {3820, 45}, {3800, 40}, {3790, 35}, {3770, 30},
  {3750, 25},  {3730, 20}, {3710, 15}, {3690, 10}, {3610, 5},
  {3270, 0}
};
const uint8_t SOC_TABLE_SIZE = sizeof(SOC_TABLE) / sizeof(SOC_TABLE[0]);

uint8_t voltageToSoc(uint16_t mv) {
  SocPoint first;
  memcpy_P(&first, &SOC_TABLE[0], sizeof(SocPoint));
  if (mv >= first.mv) return 100;

  SocPoint last;
  memcpy_P(&last, &SOC_TABLE[SOC_TABLE_SIZE - 1], sizeof(SocPoint));
  if (mv <= last.mv) return 0;

  for (uint8_t i = 1; i < SOC_TABLE_SIZE; i++) {
    SocPoint hi, lo;
    memcpy_P(&hi, &SOC_TABLE[i - 1], sizeof(SocPoint));
    memcpy_P(&lo, &SOC_TABLE[i], sizeof(SocPoint));

    if (mv >= lo.mv) {
      uint16_t span = hi.mv - lo.mv;
      if (span == 0) return lo.soc;
      uint16_t offset = mv - lo.mv;
      return lo.soc + (uint32_t)(hi.soc - lo.soc) * offset / span;
    }
  }
  return 0;
}

// ---- 측정 -------------------------------------------------------------

// USB 전압은 4.7~5.1V로 흔들린다. 5.0V로 가정하면 그게 그대로 오차가 된다.
// ATmega328P 내부 1.1V 밴드갭을 역이용해 실제 AVCC를 구한다.
long readVccMilliVolts() {
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(2);  // 기준전압 안정화. 빼면 첫 샘플이 튄다
  ADCSRA |= _BV(ADSC);
  while (bit_is_set(ADCSRA, ADSC)) {
    ;
  }
  uint8_t low = ADCL;
  uint8_t high = ADCH;
  long result = (high << 8) | low;
  if (result == 0) return 5000;

  // 이론값은 1125300 (= 1.1V * 1023 * 1000) 이지만, 내부 밴드갭에는
  // 개체차가 ±10% 있다. 이 보드는 이론 상수로 AVCC 를 5283mV 로 추정했는데
  // 멀티미터 실측은 5150mV 였다 (2.6% 과대추정).
  //
  // 보정: 새 상수 = 실측mV * (이론상수 / 이론추정mV)
  //              = 5150 * (1125300 / 5283) = 5150 * 213 = 1096950
  //
  // 다른 보드로 옮기면 이 값을 다시 맞춰야 한다. 5V-GND 를 실측한 뒤
  // 실측mV * 213 을 넣으면 된다.
  return 1096950L / result;
}

int readAdcAveraged(uint8_t pin, uint8_t samples) {
  analogRead(pin);  // 첫 샘플 버림 (멀티플렉서 전환 직후라 부정확)
  long sum = 0;
  for (uint8_t i = 0; i < samples; i++) {
    sum += analogRead(pin);
    delay(2);
  }
  return (int)(sum / samples);
}

uint16_t readBatteryMilliVolts() {
  long vccMv = readVccMilliVolts();
  int adc = readAdcAveraged(PIN_BATTERY, 16);
  float pinVolt = (adc * (vccMv / 1000.0)) / 1023.0;
  return (uint16_t)(pinVolt * DIVIDER_RATIO * 1000.0);
}

// ---- 이동평균 ---------------------------------------------------------

uint16_t smoothBuf[SMOOTH_WINDOW];
uint8_t smoothIdx = 0;
bool smoothFilled = false;

uint16_t smooth(uint16_t value) {
  smoothBuf[smoothIdx] = value;
  smoothIdx = (smoothIdx + 1) % SMOOTH_WINDOW;
  if (smoothIdx == 0) smoothFilled = true;

  uint8_t n = smoothFilled ? SMOOTH_WINDOW : smoothIdx;
  uint32_t sum = 0;
  for (uint8_t i = 0; i < n; i++) sum += smoothBuf[i];
  return (uint16_t)(sum / n);
}

// ---- 화면 -------------------------------------------------------------

const uint8_t BAR_X = 0;
const uint8_t BAR_Y = 12;
const uint8_t BAR_W = 128;
const uint8_t BAR_H = 18;

// 이 함수는 페이지마다 반복 호출된다. 인자로 받은 값만 쓰고
// 내부에서 측정하거나 상태를 바꾸지 않는다.
void drawScreen(uint16_t mv, uint8_t soc, bool charging, uint8_t animOffset, bool blinkOn) {
  char buf[10];

  // 상단 좌: 전압
  u8g2.setFont(u8g2_font_6x10_tf);
  // dtostrf 대신 정수 연산으로 소수 2자리를 만든다 (float 포매팅은 AVR에서 비싸다)
  snprintf(buf, sizeof(buf), "%u.%02uV", mv / 1000, (mv % 1000) / 10);
  u8g2.drawStr(0, 9, buf);

  // 상단 우: 충전 상태
  const char *state = charging ? "CHRG" : (soc >= 99 ? "FULL" : "");
  if (state[0] != '\0') {
    uint8_t w = u8g2.getStrWidth(state);
    u8g2.drawStr(128 - w, 9, state);
  }

  // 잔량 바 테두리
  u8g2.drawFrame(BAR_X, BAR_Y, BAR_W, BAR_H);

  // 채움. 안쪽 여백 2px
  uint8_t innerW = BAR_W - 4;
  uint8_t fillW = (uint16_t)innerW * soc / 100;

  // 저전압이면 채운 부분을 깜빡인다
  bool showFill = (soc > 10) || blinkOn;
  if (fillW > 0 && showFill) {
    u8g2.drawBox(BAR_X + 2, BAR_Y + 2, fillW, BAR_H - 4);
  }

  // 충전 중이면 빈 공간에 흐르는 블록을 그린다
  if (charging && fillW < innerW) {
    uint8_t remain = innerW - fillW;
    uint8_t blockW = 6;
    if (remain > blockW) {
      uint8_t pos = animOffset % (remain - blockW + 1);
      u8g2.drawBox(BAR_X + 2 + fillW + pos, BAR_Y + 5, blockW, BAR_H - 10);
    }
  }

  // 하단: 큰 숫자 퍼센트
  // logisoso24_tn 은 숫자 전용 폰트라 '%' 글리프가 없다.
  // 숫자만 큰 폰트로 그리고 '%' 는 작은 폰트로 옆에 붙인다.
  snprintf(buf, sizeof(buf), "%u", soc);
  u8g2.setFont(u8g2_font_logisoso24_tn);
  uint8_t numW = u8g2.getStrWidth(buf);

  u8g2.setFont(u8g2_font_6x10_tf);
  uint8_t pctW = u8g2.getStrWidth(" %");

  uint8_t startX = (128 - (numW + pctW)) / 2;

  u8g2.setFont(u8g2_font_logisoso24_tn);
  u8g2.drawStr(startX, 62, buf);

  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(startX + numW, 62, " %");
}

// ---- 메인 -------------------------------------------------------------

void setup() {
  Serial.begin(9600);
  pinMode(PIN_CHRG, INPUT_PULLUP);

  u8g2.begin();
  // 주소가 0x3D 인 모듈이면 주석 해제 (U8g2 는 8비트 주소를 쓴다)
  // u8g2.setI2CAddress(0x3D * 2);

  // 이동평균 버퍼를 실측값으로 채운다.
  // 0으로 시작하면 처음 몇 초간 게이지가 바닥에서 기어올라온다
  uint16_t mv = readBatteryMilliVolts();
  for (uint8_t i = 0; i < SMOOTH_WINDOW; i++) smooth(mv);

  Serial.println(F("battery gauge ready"));
  Serial.println(F("mV\tSoC%\tcharging"));
}

void loop() {
  static unsigned long lastRead = 0;
  static unsigned long lastAnim = 0;
  static uint16_t battMv = 0;
  static uint8_t soc = 0;
  static uint8_t animOffset = 0;
  static bool blinkOn = true;

  unsigned long now = millis();

  // --- 측정 (페이지 루프 바깥에서만) ---
  if (now - lastRead >= 500) {
    lastRead = now;
    battMv = smooth(readBatteryMilliVolts());
    soc = voltageToSoc(battMv);

    Serial.print(battMv);
    Serial.print(F("\t"));
    Serial.print(soc);
    Serial.print(F("\t"));
    Serial.println(digitalRead(PIN_CHRG) == LOW ? F("YES") : F("no"));
  }

  // --- 애니메이션 상태 갱신 ---
  if (now - lastAnim >= 150) {
    lastAnim = now;
    animOffset++;
    blinkOn = !blinkOn;
  }

  // TP4056 CHRG 는 충전 중 LOW 로 당겨지는 오픈 드레인 출력이다.
  // 연결하지 않았으면 INPUT_PULLUP 때문에 항상 HIGH(=미충전)로 읽힌다.
  bool charging = (digitalRead(PIN_CHRG) == LOW);

  // --- 그리기 ---
  // 여기서부터는 값을 바꾸지 않는다. 이 블록은 8번 반복 실행된다.
  u8g2.firstPage();
  do {
    drawScreen(battMv, soc, charging, animOffset, blinkOn);
  } while (u8g2.nextPage());
}
