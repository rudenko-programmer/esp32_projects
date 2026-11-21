# Arduino IDE + ESP32: подготовка ПК

Делается **один раз на компьютер** и подходит для всех проектов монорепы. Библиотеки, специфичные для проекта, описаны в его собственном `docs/01-*.md`.

## 1. Arduino IDE

Скачайте и установите последнюю версию [Arduino IDE 2.x](https://www.arduino.cc/en/software).

## 2. Поддержка плат ESP32

1. **Файл → Настройки** (File → Preferences).
2. В поле «Дополнительные ссылки для менеджера плат» вставьте:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. **Инструменты → Плата → Менеджер плат** (или иконка платы на левой панели), найдите `esp32`.
4. Поиск покажет два похожих пакета:

   | Пакет | Ставить? |
   |---|---|
   | **esp32** от **Espressif Systems** | ✅ **да** — содержит DOIT ESP32 DEVKIT V1 и другие обычные ESP32 |
   | Arduino ESP32 Boards от Arduino | ❌ нет — только для плат самой Arduino (Nano ESP32) |

5. У пакета **esp32 от Espressif Systems** оставьте последнюю версию (проверено на **3.3.12**, подходит любая 3.x) и нажмите **«Установка»**. Пакет большой — скачивание займёт несколько минут.

## 3. Подключение ESP32 к ПК

- Используйте **Micro-USB кабель с линиями данных** (многие кабели «только для зарядки» не подходят — плата просто не появится в системе).
- Проверьте, что появился COM-порт:
  ```powershell
  Get-PnpDevice -Class Ports -PresentOnly | Format-Table Status, FriendlyName -AutoSize
  ```
  Ожидаемо что-то вроде `OK  USB-Enhanced-SERIAL CH9102 (COM5)` — номер в скобках и есть порт для Arduino IDE.

  Быстрый вариант — только список имён портов:
  ```powershell
  [System.IO.Ports.SerialPort]::GetPortNames()
  ```
  Выполните до и после подключения платы: новый `COMx` — это ESP32.

- Если порта нет, проверьте, видит ли Windows плату вообще (`VID_1A86` — чипы WCH CH340/CH9102, `VID_10C4` — CP210x):
  ```powershell
  Get-PnpDevice -PresentOnly | Where-Object InstanceId -match 'VID_1A86|VID_10C4' | Format-Table Status, Class, FriendlyName -AutoSize
  ```
  - **Пусто** — плата не определяется: кабель только для зарядки или плохой контакт, возьмите другой кабель/USB-порт.
  - **Статус `Error` / `Unknown`, класс не `Ports`** — нет драйвера. USB-UART чип на плате — **CH9102** (на некоторых ревизиях CP2102): установите драйвер **CH343/CH9102** с сайта WCH (или CP210x от Silicon Labs) и переподключите плату.

  То же самое в GUI: Диспетчер устройств → «Порты (COM и LPT)».

## 4. Выбор платы и порта

| Параметр (Инструменты) | Значение |
|---|---|
| Плата | esp32 → **DOIT ESP32 DEVKIT V1** (список длинный — удобнее ввести «DOIT» в поиске выбора платы) |
| Порт | появившийся COM-порт (`COMx`) |
| Upload Speed | `921600` (если прошивка сбоит — `115200`) |

Встроенные библиотеки пакета ESP32 (`WiFi`, `HTTPClient`, `WiFiClientSecure`, `Wire`, `Preferences` и др.) ставить отдельно не нужно.

## 5. Альтернатива: arduino-cli

```powershell
arduino-cli config add board_manager.additional_urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32
```

Сборка и заливка любого скетча (из корня монорепы):

```powershell
arduino-cli compile --fqbn esp32:esp32:esp32doit-devkit-v1 projects/<project>/firmware/<sketch>
arduino-cli upload  --fqbn esp32:esp32:esp32doit-devkit-v1 -p COM5 projects/<project>/firmware/<sketch>
```

Подключить библиотеку из `shared/` без установки в IDE: добавьте `--library shared/libraries/<Имя>` к `compile`.
