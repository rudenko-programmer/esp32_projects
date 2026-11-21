// =====================================================================
//  ESP32 + PN532 (I2C) NFC-считыватель -> HTTP webhook
//
//  Плата:      DOIT ESP32 DEVKIT V1
//  Библиотеки: Adafruit PN532 (+ Adafruit BusIO)
//  Настройки:  config.h (скопировать из config.example.h)
// =====================================================================

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <Adafruit_PN532.h>

#if __has_include("config.h")
#include "config.h"
#else
#error "Нет файла config.h: скопируйте config.example.h в config.h и заполните свои данные"
#endif

// Значения по умолчанию, если в config.h (созданном из старого шаблона) их нет
#ifndef I2C_SDA_PIN
#define I2C_SDA_PIN 21
#endif
#ifndef I2C_SCL_PIN
#define I2C_SCL_PIN 22
#endif

#define FIRMWARE_VERSION "1.1.2"

Adafruit_PN532 nfc(PN532_IRQ_PIN, PN532_RESET_PIN);  // I2C-режим, пины SDA/SCL — в config.h

// Буфер ответа PN532 — глобальная переменная библиотеки Adafruit (Adafruit_PN532.cpp).
// Сразу после readPassiveTargetID в нём лежит ответ InListPassiveTarget:
//   [9..10] ATQA (SENS_RES), [11] SAK (SEL_RES) — по ним определяем тип карты.
extern byte pn532_packetbuffer[];

struct CardInfo {
  uint16_t atqa;
  uint8_t sak;
  String type;
};

String lastUid;
unsigned long lastUidSeenAt = 0;
bool holdNoticePrinted = false;

// ---------------------------------------------------------------------
//  Индикация
// ---------------------------------------------------------------------
void blink(uint8_t times, uint16_t onMs, uint16_t offMs = 100) {
  for (uint8_t i = 0; i < times; i++) {
    digitalWrite(STATUS_LED_PIN, HIGH);
    delay(onMs);
    digitalWrite(STATUS_LED_PIN, LOW);
    delay(offMs);
  }
}

// ---------------------------------------------------------------------
//  Wi-Fi
// ---------------------------------------------------------------------
bool connectWiFi(unsigned long timeoutMs) {
  if (WiFi.status() == WL_CONNECTED) return true;

  Serial.printf("[WiFi] Connecting to \"%s\"", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WiFi] Connected. IP: %s, RSSI: %d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
    return true;
  }
  Serial.println("[WiFi] Connection failed, will retry later");
  return false;
}

// Неблокирующая проверка соединения — вызывается в каждом цикле loop()
void maintainWiFi() {
  static unsigned long lastAttempt = 0;
  if (WiFi.status() == WL_CONNECTED) return;
  if (millis() - lastAttempt < WIFI_RETRY_INTERVAL_MS) return;
  lastAttempt = millis();

  Serial.println("[WiFi] Not connected, reconnecting...");
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

// ---------------------------------------------------------------------
//  NFC
// ---------------------------------------------------------------------
// Освобождение «застрявшей» шины I2C (процедура bus clear из спецификации I2C).
// Если ESP32 перезагрузили посреди обмена (прошивка, кнопка EN, ESP.restart), PN532 остаётся
// с питанием и может ждать недочитанные биты, удерживая SDA. 9 тактов SCL дают ему досдать байт,
// затем формируем STOP — модуль возвращается в исходное состояние без отключения питания.
void i2cBusRecover(int sda, int scl) {
  pinMode(sda, OUTPUT_OPEN_DRAIN);
  pinMode(scl, OUTPUT_OPEN_DRAIN);
  digitalWrite(sda, HIGH);  // HIGH в open-drain = «отпустить», линию подтягивает резистор
  digitalWrite(scl, HIGH);
  delayMicroseconds(10);

  if (digitalRead(sda) == LOW) {
    Serial.println("[NFC] I2C bus stuck (SDA held low), recovering...");
  }
  for (uint8_t i = 0; i < 9; i++) {
    digitalWrite(scl, LOW);
    delayMicroseconds(10);
    digitalWrite(scl, HIGH);
    delayMicroseconds(10);
  }
  // STOP: SDA 0 -> 1 при SCL = 1
  digitalWrite(sda, LOW);
  delayMicroseconds(10);
  digitalWrite(scl, HIGH);
  delayMicroseconds(10);
  digitalWrite(sda, HIGH);
  delayMicroseconds(10);

  pinMode(sda, INPUT);
  pinMode(scl, INPUT);
}

void initNfc() {
  Serial.printf("[NFC] I2C pins: SDA=GPIO%d, SCL=GPIO%d\n", I2C_SDA_PIN, I2C_SCL_PIN);

  for (uint8_t attempt = 1; attempt <= 5; attempt++) {
    // Перед каждой попыткой: останавливаем драйвер I2C, освобождаем шину вручную
    // и снова назначаем пины (библиотека запустит шину через Wire.begin() без параметров)
    Wire.end();
    i2cBusRecover(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setPins(I2C_SDA_PIN, I2C_SCL_PIN);

    nfc.begin();
    uint32_t versiondata = nfc.getFirmwareVersion();
    if (versiondata) {
      Serial.printf("[NFC] Found chip PN5%02X, firmware %d.%d\n",
                    (unsigned)((versiondata >> 24) & 0xFF),
                    (int)((versiondata >> 16) & 0xFF),
                    (int)((versiondata >> 8) & 0xFF));
      nfc.SAMConfig();
      // НЕ вызываем nfc.setPassiveActivationRetries(): в Adafruit PN532 1.3.4 по I2C он не читает
      // ответ модуля, ответ остаётся в PN532 и сдвигает все следующие обмены (карты не читаются).
      return;
    }
    Serial.printf("[NFC] PN532 not found (attempt %d/5). Check wiring and DIP switches (I2C: 1=ON, 2=OFF)\n", attempt);
    blink(5, 50, 50);
    delay(2000);
  }
  // ESP.restart() не снимает питание с PN532 — если не помогло, нужно вынуть USB на 5 секунд
  Serial.println("[NFC] PN532 not responding, restarting ESP32... If this repeats, unplug USB for 5 s.");
  delay(1000);
  ESP.restart();
}

// Семейство карты по SAK/ATQA (NXP AN10833 «MIFARE type identification procedure»)
const char* cardTypeBySak(uint16_t atqa, uint8_t sak) {
  switch (sak) {
    case 0x00: return "MIFARE Ultralight / NTAG";
    case 0x08: return "MIFARE Classic 1K";
    case 0x09: return "MIFARE Mini";
    case 0x18: return "MIFARE Classic 4K";
    case 0x10: return "MIFARE Plus 2K";
    case 0x11: return "MIFARE Plus 4K";
    case 0x28: return "SmartMX + MIFARE Classic 1K";
    case 0x38: return "SmartMX + MIFARE Classic 4K";
    case 0x88: return "MIFARE Classic 1K (Infineon)";
    case 0x20: return atqa == 0x0344 ? "MIFARE DESFire" : "ISO 14443-4 smart card";
    default:   return "Unknown ISO 14443A";
  }
}

// Ultralight/NTAG (SAK 0x00): уточняем модель по Capability Container (страница 3).
// CC[0] = 0xE1 — карта размечена под NDEF, CC[2] — размер области данных / 8.
// Пустая строка — уточнить не удалось (карта не размечена или чтение не прошло).
String refineType2() {
  uint8_t cc[4];
  if (!nfc.ntag2xx_ReadPage(3, cc) || cc[0] != 0xE1) return "";
  switch (cc[2]) {
    case 0x06: return "MIFARE Ultralight";
    case 0x12: return "NTAG213";
    case 0x3E: return "NTAG215";
    case 0x6D: return "NTAG216";
    default:   return String("NFC Type 2 (") + (cc[2] * 8) + " bytes)";
  }
}

String uidToHex(const uint8_t* uid, uint8_t len) {
  String out;
  out.reserve(len * 2);
  for (uint8_t i = 0; i < len; i++) {
    if (uid[i] < 0x10) out += "0";
    out += String(uid[i], HEX);
  }
  out.toUpperCase();
  return out;
}

// ---------------------------------------------------------------------
//  Webhook
// ---------------------------------------------------------------------
String buildPayload(const String& uid, uint8_t uidLen, const CardInfo& card) {
  char atqa[5], sak[3];
  snprintf(atqa, sizeof(atqa), "%04X", card.atqa);
  snprintf(sak, sizeof(sak), "%02X", card.sak);

  String p = "{";
  p += "\"uid\":\"" + uid + "\",";
  p += "\"uid_length\":" + String((int)uidLen) + ",";
  p += "\"card_type\":\"" + card.type + "\",";
  p += "\"atqa\":\"" + String(atqa) + "\",";
  p += "\"sak\":\"" + String(sak) + "\",";
  p += "\"device_id\":\"" DEVICE_ID "\",";
  p += "\"firmware\":\"" FIRMWARE_VERSION "\",";
  p += "\"rssi\":" + String((int)WiFi.RSSI()) + ",";
  p += "\"uptime_ms\":" + String((unsigned long)millis());
  p += "}";
  return p;
}

// Возвращает HTTP-код ответа (>0) или отрицательный код ошибки HTTPClient
int sendWebhook(const String& uid, uint8_t uidLen, const CardInfo& card) {
  if (!connectWiFi(5000)) return -1;

  const String url = WEBHOOK_URL;
  HTTPClient http;
  WiFiClient plainClient;
  WiFiClientSecure secureClient;
  bool started;

  if (url.startsWith("https://")) {
#ifdef WEBHOOK_CA_CERT
    secureClient.setCACert(WEBHOOK_CA_CERT);
#else
    secureClient.setInsecure();
#endif
    started = http.begin(secureClient, url);
  } else {
    started = http.begin(plainClient, url);
  }
  if (!started) {
    Serial.println("[HTTP] Invalid WEBHOOK_URL");
    return -1;
  }

  http.setTimeout(HTTP_TIMEOUT_MS);
  http.addHeader("Content-Type", "application/json");
  if (strlen(WEBHOOK_API_KEY) > 0) {
    http.addHeader("X-API-Key", WEBHOOK_API_KEY);
  }

  const String payload = buildPayload(uid, uidLen, card);
  const int code = http.POST(payload);

  if (code > 0) {
    Serial.printf("[HTTP] %d %s\n", code, http.getString().c_str());
  } else {
    Serial.printf("[HTTP] Request failed: %s\n", http.errorToString(code).c_str());
  }
  http.end();
  return code;
}

// ---------------------------------------------------------------------
//  Arduino
// ---------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.printf("\n=== NFC reader %s, firmware %s ===\n", DEVICE_ID, FIRMWARE_VERSION);

  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);

  connectWiFi(WIFI_CONNECT_TIMEOUT_MS);
  initNfc();

  Serial.println("[NFC] Waiting for an ISO14443A card...");
  blink(2, 100);
}

void loop() {
  maintainWiFi();

  uint8_t uid[10] = {0};
  uint8_t uidLength = 0;

  // Ждём карту не дольше NFC_READ_TIMEOUT_MS, чтобы loop() не зависал навсегда
  if (!nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, NFC_READ_TIMEOUT_MS)) {
    return;
  }

  // ATQA/SAK забираем сразу: следующая команда к PN532 перезапишет буфер
  CardInfo card;
  card.atqa = ((uint16_t)pn532_packetbuffer[9] << 8) | pn532_packetbuffer[10];
  card.sak = pn532_packetbuffer[11];

  const String cardId = uidToHex(uid, uidLength);
  const unsigned long now = millis();

  // Та же карта и её не убирали дольше SAME_CARD_COOLDOWN_MS — считаем, что она всё ещё лежит
  if (cardId == lastUid && now - lastUidSeenAt < SAME_CARD_COOLDOWN_MS) {
    lastUidSeenAt = now;
    if (!holdNoticePrinted) {
      Serial.printf("[NFC] %s still on reader - remove it for %d ms to send again\n",
                    cardId.c_str(), SAME_CARD_COOLDOWN_MS);
      holdNoticePrinted = true;
    }
    delay(100);
    return;
  }
  lastUid = cardId;
  lastUidSeenAt = now;
  holdNoticePrinted = false;  // подсказку «всё ещё лежит» покажем один раз за каждое поднесение

  card.type = cardTypeBySak(card.atqa, card.sak);
  if (card.sak == 0x00) {
    const String refined = refineType2();
    if (refined.length()) card.type = refined;
  }

  Serial.printf("[NFC] Card detected. UID: %s (%d bytes), %s [ATQA %04X, SAK %02X]\n",
                cardId.c_str(), uidLength, card.type.c_str(), card.atqa, card.sak);
  digitalWrite(STATUS_LED_PIN, HIGH);

  const int code = sendWebhook(cardId, uidLength, card);

  digitalWrite(STATUS_LED_PIN, LOW);
  if (code >= 200 && code < 300) {
    blink(1, 300);        // успех: одна длинная вспышка
  } else {
    blink(3, 80, 80);     // ошибка: три коротких
  }
}
