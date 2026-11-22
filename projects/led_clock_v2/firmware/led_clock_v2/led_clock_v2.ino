// =====================================================================
//  led_clock_v2 — часы на семисегментных индикаторах TOYO E20362
//                 (выпаяны с платы электрообогревателя)
//
//  v2 — та же логика, что у led_clock (v1), но удобная разводка:
//  верхний ряд ножек индикатора (10…6) — к верхнему ряду платы (плата горизонтально, USB слева)
//  (ножки 10, 9, 8, 7, 6 <- D32, D33, D25, D26, D27), нижний (1…5) — к нижнему
//  (ножки 1, 2, 3, 4, 5 <- D18, D19, D21, D22, D23). Схема: docs/03-hardware.md.
//
//  Этап 1 — база: один индикатор E20362-I (2 цифры, общий анод).
//    * динамическая индикация (мультиплексирование) двух цифр;
//    * самотест при старте: «88.», затем каждый сегмент по очереди;
//    * режим «секунды»: счётчик 00–59 с мигающей точкой — заготовка часов;
//    * команды из монитора порта (число 0–99, t, s, h).
//
//  Плата:      DOIT ESP32 DEVKIT V1
//  Библиотеки: — (только ядро ESP32)
//  Настройки:  config.h — необязательно (скопировать из config.example.h),
//              без него используются значения по умолчанию ниже.
// =====================================================================

#include <Arduino.h>
#include "driver/gpio.h"

#if __has_include("config.h")
#include "config.h"
#define CONFIG_FOUND 1
#else
#define CONFIG_FOUND 0
#endif

// ---- Значения по умолчанию (переопределяются в config.h) ----

// Сегменты индикатора -> GPIO (через резистор на каждой линии).
// Разводка v2: верхний ряд ножек (10…6) — верхний ряд платы, нижний (1…5) — нижний ряд платы.
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

// Общие аноды цифр -> GPIO
#ifndef DIGIT1_PIN
#define DIGIT1_PIN 32   // D32 -> ножка 10 — левая цифра (верхний ряд)
#endif
#ifndef DIGIT2_PIN
#define DIGIT2_PIN 23   // D23 -> ножка 5 — правая цифра (нижний ряд)
#endif

// Уровень, который ВКЛЮЧАЕТ цифру:
//   HIGH — анод подключён к GPIO напрямую (базовая схема, без транзисторов);
//   LOW  — анод включается PNP-транзистором (S9015 / 2N3906).
#ifndef DIGIT_ON_LEVEL
#define DIGIT_ON_LEVEL HIGH
#endif

// Уровень, который ЗАЖИГАЕТ сегмент. Общий анод -> сегмент горит при LOW.
#ifndef SEGMENT_ON_LEVEL
#define SEGMENT_ON_LEVEL LOW
#endif

// Сколько микросекунд горит одна цифра. 2000 мкс -> каждая цифра
// обновляется 250 раз в секунду, мерцания не видно.
#ifndef DIGIT_ON_TIME_US
#define DIGIT_ON_TIME_US 2000
#endif

// Длительность одного шага самотеста
#ifndef SELFTEST_STEP_MS
#define SELFTEST_STEP_MS 400
#endif

#define FIRMWARE_VERSION "2.0.0"

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
  0x3F,  // 0
  0x06,  // 1
  0x5B,  // 2
  0x4F,  // 3
  0x66,  // 4
  0x6D,  // 5
  0x7D,  // 6
  0x07,  // 7
  0x7F,  // 8
  0x6F   // 9
};
static const uint8_t SEG_BLANK = 0x00;
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

  // 1. Гасим текущую цифру, иначе сегменты следующей мелькнут на ней («призраки»)
  digitalWrite(DIGIT_PINS[active], DIGIT_OFF_LEVEL);

  // 2. Переходим к следующей цифре и выставляем её сегменты
  active ^= 1;
  const uint8_t mask = frame[active];
  for (uint8_t i = 0; i < 8; i++) {
    digitalWrite(SEG_PINS[i], (mask & (1 << i)) ? SEGMENT_ON_LEVEL : SEGMENT_OFF_LEVEL);
  }

  // 3. Включаем цифру
  digitalWrite(DIGIT_PINS[active], DIGIT_ON_LEVEL);
}

// Показать число 0–99. leadingZero=false: 7 -> « 7», true: 7 -> «07».
void displayNumber(uint8_t value, bool leadingZero) {
  value %= 100;
  const uint8_t tens = value / 10;
  frame[0] = (tens == 0 && !leadingZero) ? SEG_BLANK : FONT_DIGITS[tens];
  frame[1] = FONT_DIGITS[value % 10];
}

// ---- Режимы ----

enum Mode { MODE_SELFTEST, MODE_SECONDS, MODE_NUMBER };
static Mode mode = MODE_SELFTEST;

// Самотест: шаг 0 — всё горит («88.» на обеих цифрах),
// шаги 1..16 — по одному сегменту: сначала левая цифра A..DP, затем правая.
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
  // Шаг «всё горит» держим подольше
  const uint32_t duration = selftestStep == 0 ? 3 * SELFTEST_STEP_MS : SELFTEST_STEP_MS;
  if (millis() - selftestStepStart < duration) return;

  selftestStepStart = millis();
  selftestStep++;
  if (selftestStep >= SELFTEST_STEPS) {
    Serial.println("[TEST] готово, режим «секунды»");
    mode = MODE_SECONDS;
    return;
  }
  selftestShowStep();
}

// Секунды с момента включения, 00–59. Точка правее левой цифры
// горит первые полсекунды — как разделитель «:» у будущих часов.
void updateSeconds() {
  const uint32_t ms = millis();
  displayNumber((ms / 1000) % 60, true);
  if (ms % 1000 < 500) frame[0] |= SEG_DP;
}

// ---- Команды из монитора порта ----

void printHelp() {
  Serial.println("[CMD] команды (отправлять с переводом строки):");
  Serial.println("[CMD]   0..99  показать число (например 7, 07, 42)");
  Serial.println("[CMD]   t      самотест сегментов");
  Serial.println("[CMD]   s      режим «секунды» 00–59");
  Serial.println("[CMD]   h      эта справка");
}

void handleCommand(const String& cmd) {
  if (cmd == "t") {
    startSelftest();
  } else if (cmd == "s") {
    mode = MODE_SECONDS;
    Serial.println("[CMD] режим «секунды»");
  } else if (cmd == "h" || cmd == "?") {
    printHelp();
  } else if (cmd.length() <= 2 && isDigit(cmd[0]) && isDigit(cmd[cmd.length() - 1])) {
    mode = MODE_NUMBER;
    displayNumber(cmd.toInt(), cmd.length() == 2);
    Serial.printf("[CMD] показываю %s\n", cmd.c_str());
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

  Serial.begin(115200);
  delay(200);
  Serial.printf("\n=== led_clock_v2 v%s ===\n", FIRMWARE_VERSION);
  Serial.println(CONFIG_FOUND ? "[CFG] config.h найден"
                              : "[CFG] config.h нет — пины по умолчанию");
  Serial.printf("[CFG] цифры: GPIO %u, %u, включение уровнем %s\n",
                DIGIT1_PIN, DIGIT2_PIN, DIGIT_ON_LEVEL == HIGH ? "HIGH (напрямую)" : "LOW (PNP)");
  printHelp();

  startSelftest();
}

void loop() {
  displayRefresh();
  pollSerial();

  switch (mode) {
    case MODE_SELFTEST: updateSelftest(); break;
    case MODE_SECONDS:  updateSeconds();  break;
    case MODE_NUMBER:   break;  // число уже в frame[]
  }
}
