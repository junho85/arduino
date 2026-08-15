/*
 * 01_voltage_monitor - 전압 측정 + 캘리브레이션
 *
 * 시리얼 모니터(9600)로 측정값을 찍는다.
 * 여기서 나온 보정값을 02_battery_gauge에 옮겨 적는 것이 목적이다.
 *
 * 배선:
 *   TP4056 OUT+ ---[ R1 10k ]---+---[ R2 10k ]--- GND
 *                                |
 *                                +--> A0
 *   TP4056 OUT- ---------------------> Arduino GND (공통 접지 필수)
 *
 * ⚠️ 아두이노 GND 는 반드시 OUT- 에 문다.
 *    B+ 와 OUT+ 는 기판에서 직결된 같은 노드다. 보호회로(DW01+8205A)는
 *    음극 쪽만 끊기 때문에, 실질적인 차이는 접지 기준점에 있다.
 *    GND 를 B- 에 물면 과방전 차단이 걸려도 아두이노가 계속 셀을 소모한다.
 *
 * 왜 분압하나:
 * 18650 만충이 4.2V라 5V ADC 범위 안에 들어가긴 한다. 하지만 아두이노가
 * 꺼진 상태에서 A0에 4.2V가 걸리면 보호 다이오드를 통해 역전류가 흐른다.
 * 10k+10k 로 반으로 나누면 그 위험이 없다.
 *
 * 저항값을 10k 로 잡은 이유:
 * ATmega328P 데이터시트가 아날로그 소스 임피던스를 10k 이하로 권장한다.
 * 10k+10k 의 등가 출력 임피던스는 5k 라 권장 범위 안이다.
 * (100k+100k 면 50k 가 되어 ADC 입력 누설에 의한 오차가 커진다)
 * 대신 배터리 누설이 210uA(4.2V/20k)로 늘지만, 2950mAh 셀 기준 1.6년치이고
 * 아두이노 자체 소비(약 50mA)에 비하면 0.4% 수준이라 무시할 수 있다.
 *
 * 캘리브레이션 절차:
 *   1) 멀티미터로 OUT+/OUT- 사이 실제 전압을 잰다
 *   2) 시리얼에 찍힌 "battery" 값과 비교한다
 *   3) DIVIDER_RATIO 를 조정한다
 *        보정값 = 현재값 * (멀티미터 전압 / 시리얼 전압)
 *   4) 두 값이 0.02V 이내로 맞으면 끝
 */

const uint8_t PIN_BATTERY = A0;
const uint8_t PIN_CHRG = 2;  // TP4056 CHRG 패드 (선택). 02_battery_gauge 와 같은 핀

// 분압비. 같은 값 저항 두 개면 이상적으로는 2.000
// 저항 오차(±5%)와 배선 저항 때문에 실측으로 맞추는 값이다.
float DIVIDER_RATIO = 1.985;  // 실측 보정. 02_battery_gauge 주석 참조

// ATmega328P 내부 1.1V 밴드갭으로 실제 AVCC를 역산한다.
// USB 전압은 4.7~5.1V로 흔들리는데, 그걸 5.0V로 가정하면
// 그대로 측정 오차가 된다.
long readVccMilliVolts() {
  // 내부 1.1V 기준을 AVCC 기준으로 측정
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(2);  // 기준전압 안정화 대기. 빼면 첫 샘플이 튄다
  ADCSRA |= _BV(ADSC);
  while (bit_is_set(ADCSRA, ADSC)) {
    ;
  }
  uint8_t low = ADCL;
  uint8_t high = ADCH;
  long result = (high << 8) | low;
  if (result == 0) return 5000;  // 0 나누기 방지

  // 이론값은 1125300 (= 1.1V * 1023 * 1000). 밴드갭 개체차가 ±10% 있어
  // 이 보드는 실측 5150mV 인데 이론 상수로는 5283mV 로 나왔다.
  // 보정 = 5150 * (1125300 / 5283) = 5150 * 213 = 1096950
  // 다른 보드에서는 5V-GND 실측값 * 213 으로 다시 맞춘다.
  return 1096950L / result;
}

// ADC를 여러 번 읽어 평균. 마지막 비트의 떨림을 줄인다
int readAdcAveraged(uint8_t pin, uint8_t samples) {
  analogRead(pin);  // 첫 샘플은 버린다 (멀티플렉서 전환 직후라 부정확)
  long sum = 0;
  for (uint8_t i = 0; i < samples; i++) {
    sum += analogRead(pin);
    delay(2);
  }
  return (int)(sum / samples);
}

void setup() {
  Serial.begin(9600);
  pinMode(PIN_CHRG, INPUT_PULLUP);
  Serial.println(F("\nvoltage monitor"));
  Serial.println(F("adc\tvcc(mV)\tpin(V)\tbattery(V)\tcharging"));
}

void loop() {
  long vccMv = readVccMilliVolts();
  int adc = readAdcAveraged(PIN_BATTERY, 16);

  // ADC 값 -> A0 핀 전압 -> 분압 복원
  float pinVolt = (adc * (vccMv / 1000.0)) / 1023.0;
  float battVolt = pinVolt * DIVIDER_RATIO;

  // TP4056 CHRG는 충전 중에 LOW로 당겨지는 오픈 드레인 출력이다.
  // 연결 안 했으면 INPUT_PULLUP 때문에 항상 HIGH(=미충전)로 읽힌다.
  bool charging = (digitalRead(PIN_CHRG) == LOW);

  Serial.print(adc);
  Serial.print(F("\t"));
  Serial.print(vccMv);
  Serial.print(F("\t"));
  Serial.print(pinVolt, 3);
  Serial.print(F("\t"));
  Serial.print(battVolt, 3);
  Serial.print(F("\t\t"));
  Serial.println(charging ? F("YES") : F("no"));

  delay(1000);
}
