# __PROJECT__ — __DESCRIPTION__

<!-- Одна-две фразы: что делает устройство и куда отдаёт данные. -->

```
[датчик / вход] --> [ESP32] --Wi-Fi--> [куда уходят данные]
```

**С чего начать:** [docs/00-guide.md](docs/00-guide.md) — путеводитель по шагам со ссылками на подробные документы.

## Структура

```
projects/__PROJECT__/
├── docs/
│   ├── components/               # описания деталей и даташиты
│   ├── diagrams/                 # schematics.ipynb -> SVG-схемы для документов
│   ├── 00-guide.md               # путеводитель: все шаги по порядку со ссылками
│   ├── 01-environment-setup.md   # общая установка ESP32 + библиотеки проекта
│   ├── 02-firmware.md            # конфиг, заливка, лог
│   ├── 03-hardware.md            # схема подключения
│   └── 04-troubleshooting.md     # типовые проблемы
└── firmware/
    └── __SKETCH__/
        ├── __SKETCH__.ino
        └── config.example.h      # шаблон настроек -> скопировать в config.h
```

## Порядок сборки

| Шаг | Что делаем | ESP32 | Периферия |
|---|---|---|---|
| [00](docs/00-guide.md) | путеводитель: весь путь по шагам с чек-листами | — | — |
| [01](docs/01-environment-setup.md) | среда и библиотеки | по USB | не нужна |
| [02](docs/02-firmware.md) | `config.h`, заливка, проверка лога | по USB | не нужна |
| [03](docs/03-hardware.md) | подключение (USB вынут!) | USB вынуть → подключить → вставить | подключаем |

## Быстрый старт

1. `Copy-Item firmware/__SKETCH__/config.example.h firmware/__SKETCH__/config.h` и заполнить.
2. Открыть `firmware/__SKETCH__/__SKETCH__.ino` в Arduino IDE, плата DOIT ESP32 DEVKIT V1, залить.
3. Монитор порта 115200 — должно появиться `[WiFi] подключено`.
