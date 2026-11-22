// =====================================================================
//  led_thermometer — термометр на NTC-датчике с платы обогревателя,
//                    показания на индикаторе TOYO E20362 (2 цифры)
//
//  Индикатор подключён как в led_clock_v2 (разводка v2):
//    ножки 10, 9, 8, 7, 6 <- D32, D33, D25, D26, D27 (верхний ряд платы),
//    ножки 1, 2, 3, 4, 5  <- D18, D19, D21, D22, D23 (нижний ряд платы).
//  Датчик — делитель напряжения:
//    GPIO14 (HIGH = 3,3 В) -> резистор 10 кОм -> GPIO34 (АЦП) -> NTC -> GND.
//
//  Что делает:
//    * самотест индикатора при старте, затем — температура в °C (−9…99);
//    * лог в монитор порта раз в 2 с: температура, напряжение, сопротивление;
//    * команды из монитора порта (c, t, 0–99, h).
//
//  Плата:      DOIT ESP32 DEVKIT V1
//  Библиотеки: — (только ядро ESP32)
//  Настройки:  config.h — необязательно (скопировать из config.example.h).
// =====================================================================

#include <Arduino.h>
#include <math.h>
#include "driver/gpio.h"

#if __has_include("config.h")
#include "config.h"
#define CONFIG_FOUND 1
#else
#define CONFIG_FOUND 0
#endif

// ---- Значения по умолчанию (переопределяются в config.h) ----

// Индикатор, разводка v2. Сегменты — через резистор 470 Ом на каждой линии.
#ifndef SEG_A_PIN
#define SEG_A_PIN  21   // D21 -> ножка 3 (нижний ряд)
#endif
#ifndef SEG_B_PIN
#define SEG_B_PIN  33   // D33 -> ножка 9 (верхний ряд)
#endif
#ifndef SEG_C_PIN
#define SEG_C_PIN  25   // D25 -> ножка 8 (верхний ряд)
#endif
#ifndef SEG_D_PIN
#define SEG_D_PIN  27   // D27 -> ножка 6 (верхний ряд)
#endif
#ifndef SEG_E_PIN
#define SEG_E_PIN  26   // D26 -> ножка 7 (верхний ряд)
#endif
#ifndef SEG_F_PIN
#define SEG_F_PIN  22   // D22 -> ножка 4 (нижний ряд)
#endif
#ifndef SEG_G_PIN
#define SEG_G_PIN  18   // D18 -> ножка 1 (нижний ряд)
#endif
#ifndef SEG_DP_PIN
#define SEG_DP_PIN 19   // D19 -> ножка 2 (нижний ряд)
#endif
#ifndef DIGIT1_PIN
#define DIGIT1_PIN 32   // D32 -> ножка 10 — анод левой цифры
#endif
#ifndef DIGIT2_PIN
#define DIGIT2_PIN 23   // D23 -> ножка 5 — анод правой цифры
#endif

// HIGH — анод от GPIO напрямую; LOW — через PNP-транзистор
#ifndef DIGIT_ON_LEVEL
#define DIGIT_ON_LEVEL HIGH
#endif
// Общий анод: сегмент горит при LOW
#ifndef SEGMENT_ON_LEVEL
#define SEGMENT_ON_LEVEL LOW
#endif
#ifndef DIGIT_ON_TIME_US
#define DIGIT_ON_TIME_US 2000   // время свечения одной цифры, мкс
#endif
#ifndef SELFTEST_STEP_MS
#define SELFTEST_STEP_MS 400    // шаг самотеста, мс
#endif

// Датчик температуры (NTC) — делитель напряжения
#ifndef NTC_ADC_PIN
#define NTC_ADC_PIN    34       // D34: вход АЦП1 (работает и при включённом Wi-Fi)
#endif
#ifndef NTC_POWER_PIN
#define NTC_POWER_PIN  14       // D14: питание делителя (HIGH = 3,3 В); -1 — делитель от 3V3
#endif
#ifndef NTC_R_FIXED
#define NTC_R_FIXED    10000.0  // резистор делителя, Ом (между питанием и GPIO34)
#endif
#ifndef NTC_R25
#define NTC_R25        10000.0  // сопротивление датчика при 25 °C, Ом
#endif
#ifndef NTC_BETA
#define NTC_BETA       3950.0   // коэффициент B датчика
#endif
#ifndef NTC_SUPPLY_MV
#define NTC_SUPPLY_MV  3300     // напряжение питания делителя, мВ
#endif
#ifndef TEMP_OFFSET_C
#define TEMP_OFFSET_C  0.0      // поправка показаний, °C (если сверяли с термометром)
#endif
#ifndef TEMP_SAMPLE_MS
#define TEMP_SAMPLE_MS 100      // как часто мерить, мс
#endif
#ifndef TEMP_LOG_MS
#define TEMP_LOG_MS    2000     // как часто писать в лог, мс
#endif

#define FIRMWARE_VERSION "1.0.0"

#define DIGIT_OFF_LEVEL   (DIGIT_ON_LEVEL == HIGH ? LOW : HIGH)
#define SEGMENT_OFF_LEVEL (SEGMENT_ON_LEVEL == HIGH ? LOW : HIGH)

// ---- Индикатор ----

// Порядок бит в маске: bit0 = A, bit1 = B, ... bit6 = G, bit7 = DP
static const uint8_t SEG_PINS[8] = {
  SEG_A_PIN, SEG_B_PIN, SEG_C_PIN, SEG_D_PIN,
  SEG_E_PIN, SEG_F_PIN, SEG_G_PIN, SEG_DP_PIN
};
static const char*   SEG_NAMES[8]        = {"A", "B", "C", "D", "E", "F", "G", "DP"};
static const uint8_t SEG_DISPLAY_PINS[8] = {3, 9, 8, 6, 7, 4, 1, 2};  // ножки E20362

static const uint8_t DIGIT_PINS[2]         = {DIGIT1_PIN, DIGIT2_PIN};
static const uint8_t DIGIT_DISPLAY_PINS[2] = {10, 5};

//      A
//    F   B
//      G
//    E   C
//      D   DP
static const uint8_t FONT_DIGITS[10] = {
  0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F  // 0..9
};
static const uint8_t SEG_BLANK = 0x00;
static const uint8_t SEG_MINUS = 0x40;  // только G — «-»
static const uint8_t SEG_DP    = 0x80;

// Что сейчас показывает каждая цифра (маски сегментов)
static uint8_t frame[2] = {SEG_BLANK, SEG_BLANK};

void displayInit() {
  // Сначала выставляем «выключено», потом делаем выходом — без вспышки при старте
  for (uint8_t pin : SEG_PINS) {
    digitalWrite(pin, SEGMENT_OFF_LEVEL);
    pinMode(pin, OUTPUT);
  }
  for (uint8_t pin : DIGIT_PINS) {
    digitalWrite(pin, DIGIT_OFF_LEVEL);
    pinMode(pin, OUTPUT);
    // Анод цифры отдаёт ток всех её сегментов -> максимальная нагрузочная способность пина
    gpio_set_drive_capability((gpio_num_t)pin, GPIO_DRIVE_CAP_3);
  }
}

// Вызывается из loop() как можно чаще. По очереди зажигает цифры.
void displayRefresh() {
  static uint8_t  active = 0;
  static uint32_t lastSwitch = 0;

  const uint32_t now = micros();
  if (now - lastSwitch < DIGIT_ON_TIME_US) return;
  lastSwitch = now;

  digitalWrite(DIGIT_PINS[active], DIGIT_OFF_LEVEL);       // 1. погасить текущую
  active ^= 1;                                              // 2. следующая цифра
  const uint8_t mask = frame[active];
  for (uint8_t i = 0; i < 8; i++) {
    digitalWrite(SEG_PINS[i], (mask & (1 << i)) ? SEGMENT_ON_LEVEL : SEGMENT_OFF_LEVEL);
  }
  digitalWrite(DIGIT_PINS[active], DIGIT_ON_LEVEL);         // 3. включить её
}

// Показать число 0–99. leadingZero=false: 7 -> « 7», true: 7 -> «07».
void displayNumber(uint8_t value, bool leadingZero) {
  value %= 100;
  const uint8_t tens = value / 10;
  frame[0] = (tens == 0 && !leadingZero) ? SEG_BLANK : FONT_DIGITS[tens];
  frame[1] = FONT_DIGITS[value % 10];
}

// Показать целое от −9 до 99; вне диапазона — «--»
void displaySigned(int value) {
  if (value > 99 || value < -9) {
    frame[0] = frame[1] = SEG_MINUS;
  } else if (value < 0) {
    frame[0] = SEG_MINUS;
    frame[1] = FONT_DIGITS[-value];
  } else {
    displayNumber(value, false);
  }
}

// ---- Датчик температуры ----

enum TempStatus { TEMP_NONE, TEMP_OK, TEMP_OPEN, TEMP_SHORT };
static TempStatus tempStatus = TEMP_NONE;
static float tempC   = NAN;   // сглаженная температура
static float ntcMv   = 0;     // последнее напряжение на GPIO34, мВ
static float ntcOhm  = 0;     // последнее сопротивление датчика, Ом

void tempInit() {
  if (NTC_POWER_PIN >= 0) {
    pinMode(NTC_POWER_PIN, OUTPUT);
    digitalWrite(NTC_POWER_PIN, HIGH);        // питание делителя
  }
  analogReadResolution(12);
  analogSetPinAttenuation(NTC_ADC_PIN, ADC_11db);  // вход до ~3,1 В
}

// Сопротивление датчика из напряжения делителя:
//   U = Uпит * Rntc / (Rfix + Rntc)  =>  Rntc = Rfix * U / (Uпит - U)
// Температура по формуле B-параметра:
//   1/T = 1/T25 + ln(Rntc / R25) / B,   T — в кельвинах
void tempSample() {
  static uint32_t last = 0;
  if (millis() - last < TEMP_SAMPLE_MS) return;
  last = millis();

  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) sum += analogReadMilliVolts(NTC_ADC_PIN);  // усреднение шума
  ntcMv = sum / 8.0f;

  if (ntcMv > NTC_SUPPLY_MV - 60) { tempStatus = TEMP_OPEN;  return; }   // датчика нет / обрыв
  if (ntcMv < 60)                 { tempStatus = TEMP_SHORT; return; }   // замыкание

  ntcOhm = NTC_R_FIXED * ntcMv / (NTC_SUPPLY_MV - ntcMv);
  const float kelvin = 1.0f / (1.0f / 298.15f + logf(ntcOhm / NTC_R25) / NTC_BETA);
  const float c = kelvin - 273.15f + TEMP_OFFSET_C;

  // Экспоненциальное сглаживание: новое значение входит с весом 20 %
  tempC = (tempStatus == TEMP_OK && !isnan(tempC)) ? tempC + 0.2f * (c - tempC) : c;
  tempStatus = TEMP_OK;
}

void tempLog() {
  static uint32_t last = 0;
  if (millis() - last < TEMP_LOG_MS) return;
  last = millis();

  switch (tempStatus) {
    case TEMP_OK:
      Serial.printf("[TEMP] %.1f °C  (GPIO%d: %.2f В, датчик %.2f кОм)\n",
                    tempC, NTC_ADC_PIN, ntcMv / 1000.0f, ntcOhm / 1000.0f);
      break;
    case TEMP_OPEN:
      Serial.printf("[TEMP] нет датчика или обрыв: %.2f В на GPIO%d (почти напряжение питания)\n",
                    ntcMv / 1000.0f, NTC_ADC_PIN);
      break;
    case TEMP_SHORT:
      Serial.printf("[TEMP] замыкание датчика: %.2f В на GPIO%d (почти 0)\n",
                    ntcMv / 1000.0f, NTC_ADC_PIN);
      break;
    default:
      break;
  }
}

// ---- Режимы ----

enum Mode { MODE_SELFTEST, MODE_TEMP, MODE_NUMBER };
static Mode mode = MODE_SELFTEST;

// Самотест: шаг 0 — всё горит («8.8.»), шаги 1..16 — по одному сегменту
static const uint8_t SELFTEST_STEPS = 1 + 2 * 8;
static uint8_t  selftestStep = 0;
static uint32_t selftestStepStart = 0;

void selftestShowStep() {
  if (selftestStep == 0) {
    frame[0] = frame[1] = 0xFF;
    Serial.println("[TEST] все сегменты обеих цифр: должно гореть «8.8.»");
    return;
  }
  const uint8_t digit = (selftestStep - 1) / 8;
  const uint8_t seg   = (selftestStep - 1) % 8;
  frame[digit]     = 1 << seg;
  frame[digit ^ 1] = SEG_BLANK;
  Serial.printf("[TEST] цифра %u, сегмент %-2s (ножки %u+ / %u-, GPIO %u / %u)\n",
                digit + 1, SEG_NAMES[seg],
                DIGIT_DISPLAY_PINS[digit], SEG_DISPLAY_PINS[seg],
                DIGIT_PINS[digit], SEG_PINS[seg]);
}

void startSelftest() {
  mode = MODE_SELFTEST;
  selftestStep = 0;
  selftestStepStart = millis();
  selftestShowStep();
}

void updateSelftest() {
  const uint32_t duration = selftestStep == 0 ? 3 * SELFTEST_STEP_MS : SELFTEST_STEP_MS;
  if (millis() - selftestStepStart < duration) return;

  selftestStepStart = millis();
  selftestStep++;
  if (selftestStep >= SELFTEST_STEPS) {
    Serial.println("[TEST] готово, режим «температура»");
    mode = MODE_TEMP;
    return;
  }
  selftestShowStep();
}

// Температура на индикаторе. Целые градусы с гистерезисом: показание меняется,
// только когда температура ушла от него больше чем на 0,6 °C — без дрожания 23/24.
void updateTempDisplay() {
  static int shown = INT16_MIN;
  if (tempStatus != TEMP_OK) {
    frame[0] = frame[1] = SEG_MINUS;    // «--»: нет датчика или ошибка
    shown = INT16_MIN;
    return;
  }
  if (shown == INT16_MIN || fabsf(tempC - shown) > 0.6f) {
    shown = (int)lroundf(tempC);
  }
  displaySigned(shown);
}

// ---- Команды из монитора порта ----

void printHelp() {
  Serial.println("[CMD] команды (отправлять с переводом строки):");
  Serial.println("[CMD]   c      температура (режим по умолчанию)");
  Serial.println("[CMD]   t      самотест сегментов");
  Serial.println("[CMD]   0..99  показать число (например 7, 07, 42)");
  Serial.println("[CMD]   h      эта справка");
}

void handleCommand(const String& cmd) {
  if (cmd == "c") {
    mode = MODE_TEMP;
    Serial.println("[CMD] режим «температура»");
  } else if (cmd == "t") {
    startSelftest();
  } else if (cmd == "h" || cmd == "?") {
    printHelp();
  } else if (cmd.length() <= 2 && isDigit(cmd[0]) && isDigit(cmd[cmd.length() - 1])) {
    mode = MODE_NUMBER;
    displayNumber(cmd.toInt(), cmd.length() == 2);
    Serial.printf("[CMD] показываю %s (вернуть температуру — c)\n", cmd.c_str());
  } else {
    Serial.printf("[CMD] неизвестная команда «%s», h — справка\n", cmd.c_str());
  }
}

void pollSerial() {
  static String line;
  while (Serial.available()) {
    const char c = Serial.read();
    if (c == '\n' || c == '\r') {
      line.trim();
      if (line.length() > 0) handleCommand(line);
      line = "";
    } else if (line.length() < 16) {
      line += c;
    }
  }
}

// ---- Arduino ----

void setup() {
  displayInit();  // первым делом — чтобы индикатор не светился мусором
  tempInit();

  Serial.begin(115200);
  delay(200);
  Serial.printf("\n=== led_thermometer v%s ===\n", FIRMWARE_VERSION);
  Serial.println(CONFIG_FOUND ? "[CFG] config.h найден"
                              : "[CFG] config.h нет — значения по умолчанию");
  Serial.printf("[CFG] цифры: GPIO %u, %u; датчик: АЦП GPIO%d, питание делителя %s\n",
                DIGIT1_PIN, DIGIT2_PIN, NTC_ADC_PIN,
                NTC_POWER_PIN >= 0 ? ("GPIO" + String(NTC_POWER_PIN)).c_str() : "3V3");
  Serial.printf("[CFG] NTC: R25 = %.0f Ом, B = %.0f, резистор делителя %.0f Ом\n",
                (double)NTC_R25, (double)NTC_BETA, (double)NTC_R_FIXED);
  printHelp();

  startSelftest();
}

void loop() {
  displayRefresh();
  pollSerial();
  tempSample();
  tempLog();

  switch (mode) {
    case MODE_SELFTEST: updateSelftest();    break;
    case MODE_TEMP:     updateTempDisplay(); break;
    case MODE_NUMBER:   break;  // число уже в frame[]
  }
}
