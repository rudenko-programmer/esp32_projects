#pragma once
// =====================================================================
//  Конфигурация NFC-считывателя.
//  Скопируйте этот файл в config.h (в той же папке) и заполните.
//  config.h добавлен в .gitignore — пароли не попадут в репозиторий.
// =====================================================================

// ---- Wi-Fi (ESP32 работает только с 2.4 ГГц) ----
#define WIFI_SSID       "YOUR_WIFI_SSID"
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"

// ---- Вебхук ----
// Для локального docker-сервера укажите IP компьютера в локальной сети,
// НЕ localhost/127.0.0.1 (для ESP32 это он сам).
#define WEBHOOK_URL     "http://192.168.1.100:8090/webhook/nfc"

// Отправляется в заголовке X-API-Key. Должен совпадать с API_KEY в server/.env.
// Пустая строка "" — заголовок не отправляется.
#define WEBHOOK_API_KEY "change-me"

// Идентификатор устройства (полезно, если считывателей несколько)
#define DEVICE_ID       "nfc-reader-01"

// Для https:// без этого параметра сертификат сервера НЕ проверяется (setInsecure).
// Чтобы включить проверку — раскомментируйте и вставьте корневой CA в PEM:
// #define WEBHOOK_CA_CERT "-----BEGIN CERTIFICATE-----\n...\n-----END CERTIFICATE-----\n"

// ---- Пины ----
// I2C: SDA -> D21, SCL -> D22 (стандартные пины ESP32).
// Менять не обязательно. Если меняете — НЕЛЬЗЯ: 0, 2, 5, 12, 15 (влияют на загрузку),
// 6–11 (флеш-память), 34–39 (только вход). Подробнее: docs/04-hardware.md, 4.3.1.
#define I2C_SDA_PIN      21
#define I2C_SCL_PIN      22

// Конструктор библиотеки требует номера IRQ- и reset-пинов, но к ним ничего НЕ подключаем:
// - IRQ: в режиме I2C библиотека проверяет готовность по шине (байт RDY), а не по IRQ;
// - RSTO на PN532 V3 — это выход модуля, а не вход сброса.
// Подробно: docs/04-hardware.md, раздел 4.4. Достаточно 4 проводов: VCC, GND, SDA, SCL.
#define PN532_IRQ_PIN    18
#define PN532_RESET_PIN  19
#define STATUS_LED_PIN   2    // встроенный синий светодиод DOIT DevKit V1

// ---- Тайминги ----
#define NFC_READ_TIMEOUT_MS     500    // сколько ждать карту за один цикл loop()
#define SAME_CARD_COOLDOWN_MS   1000   // ту же карту отправим снова, если её убрали хотя бы на это время
#define HTTP_TIMEOUT_MS         5000
#define WIFI_CONNECT_TIMEOUT_MS 20000
#define WIFI_RETRY_INTERVAL_MS  10000
