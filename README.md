# esp32_projects

Монорепа проектов на ESP32: прошивки, серверная часть, документация — всё в одном репозитории.

## Проекты

| Проект | Описание | Железо | Сервер |
|---|---|---|---|
| [my_nfc](projects/my_nfc/) | NFC-считыватель: UID карт → HTTP-вебхук, живой веб-монитор | ESP32 DevKit V1 + PN532 (I2C) | FastAPI + SQLite, порт 8090 |
| [led_clock](projects/led_clock/) | Часы на семисегментных индикаторах с платы обогревателя (этап 1: 2 цифры), разводка v1 | ESP32 DevKit V1 + TOYO E20362-I (общий анод) | — |
| [led_clock_v2](projects/led_clock_v2/) | То же, разводка v2 без пересечений проводов: верхний ряд ножек индикатора — верхний ряд платы, нижний — нижний | ESP32 DevKit V1 + TOYO E20362-I (общий анод) | — |

## Структура

```
esp32_projects/
├── projects/            # проекты (каждый самодостаточен)
│   ├── my_nfc/
│   ├── led_clock/
│   └── led_clock_v2/
├── docs/                # общая документация
│   ├── conventions.md         # архитектура проекта и правила
│   ├── diagrams.md            # схемы кодом (Jupyter + schemdraw + WireViz в Docker)
│   ├── esp32-arduino-setup.md # подготовка ПК (один раз)
│   └── new-project.md         # как создать новый проект
├── shared/              # общий код для нескольких проектов
├── templates/project/   # шаблон нового проекта
├── templates/component.md  # образец описания компонента
├── tools/               # скрипты монорепы (new-project.ps1, diagrams.ps1)
├── .claude/agents/      # агенты Claude Code
└── CLAUDE.md            # правила для Claude Code
```

## С чего начать

1. Подготовить ПК (Arduino IDE, пакет ESP32, драйвер, COM-порт): [docs/esp32-arduino-setup.md](docs/esp32-arduino-setup.md).
2. Открыть README нужного проекта и идти по его `docs/` по порядку.
3. Новый проект: [docs/new-project.md](docs/new-project.md) — коротко:
   ```powershell
   ./tools/new-project.ps1 -Name weather_station -Sketch weather_node -Description "Метеостанция на BME280"
   ```

Правила структуры, именования, секретов и портов: [docs/conventions.md](docs/conventions.md).
