# 2. Сервер приёма вебхуков (Docker)

Код: [server/](../server/) — FastAPI + SQLite, один контейнер.

Сервер поднимаем **до прошивки**: от него нужны IP компьютера и `API_KEY` — их вписывают в `config.h` ESP32.

Возможности:
- приём сканов `POST /webhook/nfc` с проверкой `X-API-Key`;
- хранение всех сканов в SQLite (Docker volume `nfc_data`);
- справочник карт: UID → имя, ответ устройству содержит `known` / `name`;
- **живой веб-монитор**: каждый скан мгновенно появляется на странице (Server-Sent Events, без перезагрузки);
- необязательная пересылка каждого скана дальше (`FORWARD_URL` — n8n, Laravel и т.д.).

## 2.1 Запуск

Нужен Docker Desktop (Compose v2.24+).

**Через агента Claude Code:** попросите «запусти вебхук-сервис». Агент `my-nfc-webhook` (`.claude/agents/my-nfc-webhook.md` в корне монорепы) сам запустит Docker Desktop, создаст `.env` со случайным ключом, поднимет контейнер, дождётся готовности, назовёт IP для `WEBHOOK_URL`, проверит брандмауэр и сверит настройки с `config.h`. Ещё он умеет `status`, `logs`, `test`, `restart` и `stop`.

**Вручную (PowerShell):**
```powershell
cd projects/my_nfc/server   # из корня монорепы
Copy-Item .env.example .env   # затем задайте API_KEY в .env
docker compose up -d --build
docker compose ps             # статус должен быть healthy
docker compose logs -f        # логи в реальном времени
```

Веб-монитор: http://localhost:8090 · Swagger: http://localhost:8090/docs

### Что такое `<IP ПК>`

В документации `<IP ПК>` — это **локальный IP компьютера в вашей Wi-Fi сети**, его выдаёт роутер (например, `192.168.0.111`). По нему ESP32 и другие устройства в сети находят сервер.

| Адрес | Что это | Для ESP32 |
|---|---|---|
| `localhost` / `127.0.0.1` | «сам себе» | ❌ для ESP32 это она сама, а не ПК |
| **локальный IP** (`192.168.x.x`, `10.x.x.x`) | адрес ПК в домашней сети | ✅ его и указываем в `WEBHOOK_URL` |
| внешний IP | адрес роутера в интернете | ❌ не нужен, всё работает внутри сети |

Узнать локальный IP:
```powershell
Get-NetIPConfiguration | Where-Object { $_.IPv4DefaultGateway -and $_.NetAdapter.Status -eq 'Up' } | ForEach-Object { $_.IPv4Address.IPAddress }
```
Или `ipconfig` → строка «IPv4-адрес» у адаптера Wi-Fi. ПК и ESP32 должны быть в **одной** сети (не гостевой).

## 2.1.1 Веб-монитор

Одностраничник (`server/app/static/index.html`) открывается в браузере на любом устройстве в сети: `http://<IP ПК>:8090`. Удобно держать его открытым на втором мониторе или планшете рядом со считывателем.

- **Крупная карточка последнего скана**: имя или «Неизвестная карта», UID, устройство, время и уровень Wi-Fi. При скане рамка вспыхивает зелёным (известная карта) или оранжевым (новая).
- **Лента последних сканов** — новые строки появляются сверху.
- **Счётчики**: всего сканов, разных карт, карт с именем, устройств.
- **Имя карты**: клик по строке или кнопка «Назвать карту». При первой попытке страница попросит `API_KEY` и запомнит его в браузере. Имя сразу обновится на всех открытых страницах.
- **Звук** при скане (кнопка 🔔): разный сигнал для известной и новой карты.
- Индикатор **Live** показывает, что поток событий подключён. При обрыве страница переподключается сама и подгружает пропущенные сканы.

Остановка: `docker compose down` (данные в volume сохраняются). Удалить вместе с данными: `docker compose down -v`.

## 2.2 Настройки (`server/.env`)

| Переменная | По умолчанию | Описание |
|---|---|---|
| `HOST_PORT` | `8090` | порт на хосте |
| `API_KEY` | пусто | общий секрет с прошивкой. Пусто — проверка отключена (только для отладки) |
| `FORWARD_URL` | пусто | URL, куда пересылать каждый скан (POST JSON) |
| `FORWARD_TIMEOUT` | `5` | таймаут пересылки, с |
| `LOG_LEVEL` | `INFO` | уровень логов |

## 2.3 API

| Метод | Путь | Ключ | Описание |
|---|---|---|---|
| `GET` | `/health` | — | проверка работы (`live_clients` — сколько открыто страниц монитора) |
| `POST` | `/webhook/nfc` | да | приём скана от ESP32 |
| `GET` | `/api/events` | — | поток событий SSE: `scan` (новый скан), `card` (изменилось имя карты) |
| `GET` | `/api/stats` | — | счётчики: сканы, разные UID, карты с именем, устройства |
| `GET` | `/api/scans?limit=50&uid=&device_id=` | — | список сканов (новые первыми) |
| `GET` | `/api/cards` | — | список карт с именами и временем последнего скана |
| `PUT` | `/api/cards/{uid}` | да | задать имя карте: `{"name": "Макс"}` |
| `DELETE` | `/api/cards/{uid}` | да | удалить карту из справочника |
| `GET` | `/` | — | веб-монитор |
| `GET` | `/docs` | — | Swagger UI |

«Ключ» — заголовок `X-API-Key: <API_KEY>`.

### Ответ на скан

```json
{"status": "ok", "scan_id": 12, "uid": "3A7F1C09", "known": true, "name": "Макс"}
```

Ошибки: `401` — неверный/отсутствующий ключ, `422` — некорректный JSON или UID (должен быть 8–20 hex-символов).

### Примеры (PowerShell)

```powershell
$h = @{ "X-API-Key" = "change-me" }

# эмуляция скана без железа — сразу появится на веб-мониторе
Invoke-RestMethod -Method Post -Uri http://localhost:8090/webhook/nfc -Headers $h `
  -ContentType "application/json" -Body '{"uid":"3A7F1C09","device_id":"test"}'

# дать карте имя
Invoke-RestMethod -Method Put -Uri http://localhost:8090/api/cards/3A7F1C09 -Headers $h `
  -ContentType "application/json; charset=utf-8" -Body '{"name":"Макс"}'

# последние 10 сканов
Invoke-RestMethod "http://localhost:8090/api/scans?limit=10"
```

## 2.4 Пересылка в n8n / Laravel

Укажите `FORWARD_URL` в `.env` и перезапустите (`docker compose up -d`). После сохранения скана сервер отправит в фоне:

```json
{
  "uid": "3A7F1C09", "device_id": "nfc-reader-01", "uid_length": 4,
  "firmware": "1.0.0", "rssi": -58, "uptime_ms": 123456,
  "status": "ok", "scan_id": 12, "known": true, "name": "Макс",
  "received_at": "2026-11-07T20:15:03+00:00"
}
```

Ошибки пересылки только логируются — устройство получает ответ сразу, не дожидаясь n8n.

- **n8n**: нода *Webhook* (метод POST) → скопируйте Production URL в `FORWARD_URL`. Если n8n крутится в Docker на той же машине, используйте `http://host.docker.internal:5678/webhook/...` (Docker Desktop) или общую docker-сеть.
- Можно обойтись и без этого сервера: указать адрес n8n/Laravel прямо в `WEBHOOK_URL` прошивки.

## 2.5 Сеть и безопасность

- **Брандмауэр Windows** может блокировать входящие подключения к порту 8090 из локальной сети. Особенно если Wi-Fi отмечен как **общественная сеть** (Public) — тогда входящие закрыты по умолчанию. Проверить тип сети:
  ```powershell
  Get-NetConnectionProfile | Select-Object InterfaceAlias, Name, NetworkCategory
  ```
  **Проверять доступность — только с другого устройства** в той же сети: откройте на телефоне браузер и зайдите на `http://<IP ПК>:8090`. Открылся веб-монитор — порт открыт, ESP32 тоже достучится.

  > Проверка с самого компьютера (`Test-NetConnection <IP ПК>` или браузер на ПК) ничего не доказывает: запрос к собственному адресу идёт внутри компьютера, не так, как запрос от ESP32. А если в `%USERPROFILE%\.wslconfig` стоит `networkingMode=mirrored`, такой запрос вообще не доходит до Docker и всегда выглядит как «закрыто».

  Если с телефона не открывается — откройте порт. PowerShell нужно запустить **от имени администратора**:
  ```powershell
  New-NetFirewallRule -DisplayName "NFC webhook 8090" -Direction Inbound -Protocol TCP -LocalPort 8090 -RemoteAddress LocalSubnet -Action Allow
  ```
  `-RemoteAddress LocalSubnet` пускает только устройства из вашей локальной сети. Для домашнего Wi-Fi можно также сменить тип сети на частную: `Set-NetConnectionProfile -InterfaceAlias "Wi-Fi" -NetworkCategory Private`.
- **IP компьютера** выдаёт роутер (DHCP), и он может смениться — тогда ESP32 потеряет сервер. Закрепите IP за компьютером в настройках роутера («DHCP → Резервирование адресов»).
- Чтение (`/`, `/api/events`, `/api/scans`, `/api/stats`, `/api/cards`) не защищено ключом — сервер рассчитан на локальную сеть. При публикации в интернет ставьте его за reverse proxy (Caddy / Nginx / Traefik) с HTTPS и авторизацией.
- Используйте длинный случайный `API_KEY`, например: `python -c "import secrets; print(secrets.token_urlsafe(32))"`.

## 2.6 Бэкап базы

```powershell
docker compose cp nfc-webhook:/data/nfc.db ./nfc-backup.db
```

Дальше: [03-firmware.md](03-firmware.md) — настроить и залить прошивку.
