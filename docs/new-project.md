# Новый проект

## 1. Создать из шаблона

Из корня монорепы:

```powershell
./tools/new-project.ps1 -Name weather_station -Sketch weather_node -Description "Метеостанция на BME280"
```

| Параметр | Обязателен | Что это |
|---|---|---|
| `-Name` | да | папка проекта в `projects/`, `snake_case` |
| `-Sketch` | нет (= `-Name`) | имя скетча: папка `firmware/<sketch>/` и файл `<sketch>.ino` |
| `-Description` | нет | одна строка для README и шапки скетча |

Скрипт копирует [templates/project/](../templates/project/) и подставляет имена. Получается рабочий скетч: подключается к Wi-Fi, мигает светодиодом, пишет лог в Serial.

## 2. Зарегистрировать

- Строка в таблице проектов: [README.md](../README.md) и [CLAUDE.md](../CLAUDE.md).
- Если есть сервер — порт в реестре [conventions.md](conventions.md#реестр-портов), `name: <project>` в `docker-compose.yml`.
- Если нужен агент Claude Code — `.claude/agents/<project-kebab>-<роль>.md`.

## 3. Наполнить

- Библиотеки — в `docs/01-environment-setup.md`.
- Параметры — в `config.example.h` (и `#ifndef`-умолчание в скетче).
- Схема подключения — в `docs/03-hardware.md`.
- Дополнительные шаги (сервер, тестирование) — новые документы `NN-тема.md` по порядку сборки; обновить таблицу «Порядок сборки» в README проекта.
- Специфику проекта для Claude можно описать в `projects/<project>/CLAUDE.md`.

Шаблон — минимальный стартовый набор. Если проекту что-то не нужно (например, Wi-Fi) — смело удаляйте.
