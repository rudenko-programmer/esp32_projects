# shared — общий код проектов

Сюда выносится код, который используют **два и более** проекта. Пока код нужен одному проекту — он живёт в этом проекте.

## Формат

Каждая библиотека — в формате Arduino-библиотеки:

```
shared/libraries/<ИмяБиблиотеки>/
├── library.properties    # name, version, author, sentence, architectures=esp32
├── src/
│   ├── <ИмяБиблиотеки>.h
│   └── <ИмяБиблиотеки>.cpp
└── README.md             # что делает, API, какие проекты используют
```

## Подключение

**arduino-cli** — без установки, флагом:

```powershell
arduino-cli compile --fqbn esp32:esp32:esp32doit-devkit-v1 --library shared/libraries/<Имя> projects/<project>/firmware/<sketch>
```

**Arduino IDE** — один раз создать ссылку (junction) в папке библиотек скетчбука; правки в монорепе сразу видны в IDE:

```powershell
New-Item -ItemType Junction -Path "$env:USERPROFILE\Documents\Arduino\libraries\<Имя>" -Target "D:\Projects\esp32_projects\shared\libraries\<Имя>"
```

## Правила

- Библиотека не знает о конкретном проекте: никаких `#include "config.h"`, всё через параметры конструктора/`begin()`.
- Изменение API общей библиотеки → проверить сборку всех проектов, которые её используют (список — в её README).
