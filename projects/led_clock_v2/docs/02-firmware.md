# 2. Прошивка

Код: [firmware/led_clock_v2/](../firmware/led_clock_v2/)

## 2.1 Конфигурация (необязательно)

На этапе 1 Wi-Fi не используется, секретов нет. Без `config.h` прошивка берёт пины и тайминги по умолчанию. Они совпадают со схемой в [03-hardware.md](03-hardware.md).

`config.h` нужен, только если меняете пины или схему:

```powershell
Copy-Item firmware/led_clock_v2/config.example.h firmware/led_clock_v2/config.h
```

| Параметр | Что значит | По умолчанию |
|---|---|---|
| `SEG_A_PIN` … `SEG_DP_PIN` | GPIO сегментов A–G и точки | 21, 33, 25, 27, 26, 22, 18, 19 |
| `DIGIT1_PIN` / `DIGIT2_PIN` | GPIO анодов левой / правой цифры | 32 / 23 |
| `DIGIT_ON_LEVEL` | `HIGH` — анод напрямую от GPIO; `LOW` — через PNP-транзистор | `HIGH` |
| `SEGMENT_ON_LEVEL` | `LOW` для общего анода | `LOW` |
| `DIGIT_ON_TIME_US` | сколько горит одна цифра, мкс | 2000 |
| `SELFTEST_STEP_MS` | шаг самотеста, мс | 400 |

`config.h` в `.gitignore`.

## 2.2 Заливка

1. Arduino IDE: **Файл → Открыть** → `firmware/led_clock_v2/led_clock_v2.ino`.
2. Плата **DOIT ESP32 DEVKIT V1**, порт `COMx`, **Загрузка**.

Прошивку можно залить и до подключения индикатора, тогда в логе просто пройдёт самотест.

Через arduino-cli (из корня монорепы):

```powershell
arduino-cli compile --fqbn esp32:esp32:esp32doit-devkit-v1 projects/led_clock_v2/firmware/led_clock_v2
arduino-cli upload  --fqbn esp32:esp32:esp32doit-devkit-v1 -p COM5 projects/led_clock_v2/firmware/led_clock_v2
arduino-cli monitor -p COM5 -c baudrate=115200
```

## 2.3 Ожидаемый лог

```
=== led_clock_v2 v2.0.0 ===
[CFG] config.h нет — пины по умолчанию
[CFG] цифры: GPIO 32, 23, включение уровнем HIGH (напрямую)
[CMD] команды (отправлять с переводом строки):
...
[TEST] все сегменты обеих цифр: должно гореть «8.8.»
[TEST] цифра 1, сегмент A  (ножки 10+ / 3-, GPIO 32 / 21)
[TEST] цифра 1, сегмент B  (ножки 10+ / 9-, GPIO 32 / 33)
...
[TEST] цифра 2, сегмент DP (ножки 5+ / 2-, GPIO 23 / 19)
[TEST] готово, режим «секунды»
```

## 2.4 Команды

В мониторе порта Arduino IDE (режим окончания строки — «Новая строка» или «NL и CR»):

| Команда | Что делает |
|---|---|
| `0` … `99` | показать число: `7` → « 7», `07` → «07» |
| `t` | повторить самотест |
| `s` | режим «секунды» 00–59 |
| `h` | справка |

Дальше: [03-hardware.md](03-hardware.md).
