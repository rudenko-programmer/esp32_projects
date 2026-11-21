# my_nfc — NFC-считыватель на ESP32

Устройство на ESP32 + PN532 считывает UID NFC-карт/брелоков (ISO14443A, Mifare) и отправляет их по Wi-Fi на HTTP-вебхук. В комплекте — Docker-сервер, который принимает вебхуки, хранит сканы в SQLite, **мгновенно показывает каждый скан на веб-странице** и при необходимости пересылает его дальше (n8n, Laravel API и т.п.).

```
[NFC-карта] ~~> [PN532] --I2C--> [ESP32] --Wi-Fi / HTTP POST--> [nfc-webhook (Docker)] --> [n8n / Laravel / ...]
                                                                       |
                                                                    SQLite + веб-панель
```

## Структура

Проект живёт в монорепе `esp32_projects` (общие правила — [docs/conventions.md](../../docs/conventions.md)). Агент Claude Code для сервера — [`.claude/agents/my-nfc-webhook.md`](../../.claude/agents/my-nfc-webhook.md) в корне монорепы.

```
projects/my_nfc/
├── CLAUDE.md                     # специфика проекта для Claude Code
├── docs/                         # техническая документация
│   ├── 01-environment-setup.md   # общая установка ESP32 + библиотека PN532
│   ├── 02-webhook-server.md      # Docker-сервер: запуск, веб-монитор, API
│   ├── 03-firmware.md            # конфиг, заливка, лог, индикация
│   ├── 04-hardware.md            # DIP-переключатели, схема подключения PN532
│   ├── 05-testing.md             # пошаговая проверка всей цепочки
│   └── 06-troubleshooting.md     # типовые проблемы и решения
├── firmware/
│   └── nfc_reader/               # скетч Arduino для ESP32
│       ├── nfc_reader.ino
│       └── config.example.h      # шаблон настроек -> скопировать в config.h
└── server/                       # приёмник вебхуков (FastAPI + SQLite)
    ├── app/main.py           # API + поток событий (SSE)
    ├── app/static/index.html # живой веб-монитор
    ├── Dockerfile
    ├── docker-compose.yml
    ├── requirements.txt
    └── .env.example              # шаблон настроек -> скопировать в .env
```

## Порядок сборки

Документы пронумерованы в том порядке, в котором всё делается:

| Шаг | Что делаем | ESP32 | Модуль PN532 |
|---|---|---|---|
| [01](docs/01-environment-setup.md) | Arduino IDE и пакет esp32 ([общий док](../../docs/esp32-arduino-setup.md)), библиотека PN532 | подключена по USB (проверка порта) | не нужен |
| [02](docs/02-webhook-server.md) | поднять сервер — получить IP и `API_KEY` | не нужна | не нужен |
| [03](docs/03-firmware.md) | `config.h`, заливка, проверка Wi-Fi | по USB | можно не подключать |
| [04](docs/04-hardware.md) | подключить модуль (USB вынут!) и включить | USB вынуть → подключить → вставить | подключаем |
| [05](docs/05-testing.md) | проверка всей цепочки | по USB | подключён |

Если модуль уже подключён до шага 03 — ничего страшного, прошивать можно и так (см. [03-firmware.md](docs/03-firmware.md), раздел 3.2).

## Быстрый старт

1. **Сервер.** В Claude Code скажите «запусти вебхук-сервис», и агент `my-nfc-webhook` сделает всё сам. Вручную:
   ```powershell
   cd projects/my_nfc/server   # из корня монорепы
   Copy-Item .env.example .env   # задайте API_KEY
   docker compose up -d --build
   ```
   Веб-монитор: http://localhost:8090. Сканы появляются на нём сразу, без перезагрузки.

2. **Прошивка**
   - Скопируйте `firmware/nfc_reader/config.example.h` → `config.h`, впишите Wi-Fi, `WEBHOOK_URL` (`http://<IP вашего ПК>:8090/webhook/nfc`) и тот же `API_KEY`.
   - Откройте `firmware/nfc_reader/nfc_reader.ino` в Arduino IDE и залейте на плату ([подробно](docs/03-firmware.md)).

3. **Железо.** Подключите PN532 по I2C ([схема](docs/04-hardware.md)) и поднесите карту — скан сразу отобразится на мониторе.

Подробно — по документам [docs/](docs/) по порядку, начиная с [01-environment-setup.md](docs/01-environment-setup.md).
