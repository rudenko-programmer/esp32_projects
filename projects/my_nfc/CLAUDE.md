# my_nfc — специфика проекта

ESP32 (DOIT DevKit V1) + PN532 по I2C читает UID NFC-карт и шлёт `POST /webhook/nfc` на FastAPI-сервер в Docker (SQLite + живой веб-монитор через SSE, опциональная пересылка на `FORWARD_URL`).

- Прошивка: `firmware/nfc_reader/` (`FIRMWARE_VERSION` в `nfc_reader.ino` — повышать при изменениях). Библиотека Adafruit PN532 ≥ 1.3.4 (I2C без провода IRQ).
- Сервер: `server/`, compose-проект `my_nfc`, контейнер `nfc-webhook`, порт хоста 8090, данные в volume `server_nfc_data` (историческое имя — не переименовывать, там история сканов).
- Управление сервером — агент `my-nfc-webhook` (`../../.claude/agents/my-nfc-webhook.md`).
- `API_KEY` в `server/.env` должен совпадать с `WEBHOOK_API_KEY` в `firmware/nfc_reader/config.h`; `WEBHOOK_URL` — LAN-IP ПК, не localhost.
- Документация: `docs/01..06`, при изменении поведения обновлять соответствующий документ и таблицу в `06-troubleshooting.md`.
