// =====================================================================
//  __SKETCH__ — __DESCRIPTION__
//
//  Плата:      DOIT ESP32 DEVKIT V1
//  Библиотеки: —
//  Настройки:  config.h (скопировать из config.example.h)
// =====================================================================

#include <WiFi.h>

#if __has_include("config.h")
#include "config.h"
#else
#error "Нет файла config.h: скопируйте config.example.h в config.h и заполните свои данные"
#endif

// Значения по умолчанию, если в config.h (созданном из старого шаблона) их нет
#ifndef STATUS_LED_PIN
#define STATUS_LED_PIN 2
#endif
#ifndef WIFI_CONNECT_TIMEOUT_MS
#define WIFI_CONNECT_TIMEOUT_MS 20000
#endif
#ifndef WIFI_RETRY_INTERVAL_MS
#define WIFI_RETRY_INTERVAL_MS 10000
#endif

#define FIRMWARE_VERSION "0.1.0"

static uint32_t lastWifiAttempt = 0;

bool connectWifi() {
  Serial.printf("[WiFi] подключение к %s...\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
    digitalWrite(STATUS_LED_PIN, !digitalRead(STATUS_LED_PIN));  // мигаем, пока подключаемся
    delay(250);
  }
  lastWifiAttempt = millis();

  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(STATUS_LED_PIN, HIGH);
    Serial.printf("[WiFi] подключено, IP %s\n", WiFi.localIP().toString().c_str());
    return true;
  }
  digitalWrite(STATUS_LED_PIN, LOW);
  Serial.println("[WiFi] не удалось подключиться, повтор позже");
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.printf("\n=== __SKETCH__ v%s (%s) ===\n", FIRMWARE_VERSION, DEVICE_ID);

  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);

  connectWifi();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED && millis() - lastWifiAttempt > WIFI_RETRY_INTERVAL_MS) {
    connectWifi();
  }

  // TODO: основная логика проекта

  delay(10);
}
