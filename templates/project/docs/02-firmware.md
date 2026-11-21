# 2. Прошивка

Код: [firmware/__SKETCH__/](../firmware/__SKETCH__/)

## 2.1 Конфигурация

```powershell
Copy-Item firmware/__SKETCH__/config.example.h firmware/__SKETCH__/config.h
```

| Параметр | Что вписать |
|---|---|
| `WIFI_SSID` / `WIFI_PASSWORD` | сеть 2.4 ГГц |
| `DEVICE_ID` | имя устройства |

`config.h` в `.gitignore` — пароли не попадут в репозиторий.

## 2.2 Заливка

1. Arduino IDE: **Файл → Открыть** → `firmware/__SKETCH__/__SKETCH__.ino`.
2. Плата **DOIT ESP32 DEVKIT V1**, порт `COMx`, **Загрузка**.

Через arduino-cli (из корня монорепы):

```powershell
arduino-cli compile --fqbn esp32:esp32:esp32doit-devkit-v1 projects/__PROJECT__/firmware/__SKETCH__
arduino-cli upload  --fqbn esp32:esp32:esp32doit-devkit-v1 -p COM5 projects/__PROJECT__/firmware/__SKETCH__
arduino-cli monitor -p COM5 -c baudrate=115200
```

## 2.3 Ожидаемый лог

```
=== __SKETCH__ v0.1.0 ===
[WiFi] подключение к <SSID>...
[WiFi] подключено, IP 192.168.x.x
```

Дальше: [03-hardware.md](03-hardware.md).
