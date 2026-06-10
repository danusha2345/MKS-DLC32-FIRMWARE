# Прошивка MKS DLC32 v2.1 готовыми бинарниками

Комплект для прошивки платы **без установки PlatformIO** — нужен только Python
с esptool. Версия сборки — в [`VERSION.txt`](VERSION.txt).

| Файл | Что это | Адрес во flash |
|---|---|---|
| `bootloader.bin` | Загрузчик ESP32 (DOUT, 80 МГц) | `0x1000` |
| `partitions.bin` | Таблица разделов под 8 МБ (OTA: два app-слота по 3 МБ) | `0x8000` |
| `boot_app0.bin` | Селектор OTA-слота | `0xe000` |
| `firmware.bin` | Прошивка Grbl_ESP32 + наши фиксы | `0x10000` |
| `spiffs.bin` | Файловая система с WebUI (ESP3D-WEBUI 2.1.3b0, рус./англ.) | `0x610000` |

Настройки платы (**Wi-Fi, `$$`-параметры GRBL**) при прошивке **сохраняются** —
они лежат в NVS-разделе (`0x9000`), который скрипты не трогают.

## Подготовка (один раз)

1. **Python 3** — [python.org](https://www.python.org/downloads/) (Windows: при
   установке отметить «Add Python to PATH»).
2. **esptool**: `pip install esptool`
3. **Windows**: драйвер USB-моста [CH340](https://www.wch-ic.com/downloads/CH341SER_EXE.html).
   Номер порта смотреть в Диспетчере устройств → «Порты (COM и LPT)».
   **Linux**: пользователь должен быть в группе `dialout`
   (`sudo usermod -aG dialout $USER`, перелогиниться).

## Прошивка по USB

Подключить плату кабелем USB (питание платы 12/24 В подавать не обязательно —
для прошивки хватает USB).

**Linux/macOS:**
```bash
./flash_usb.sh                 # порт /dev/ttyUSB0
./flash_usb.sh /dev/ttyUSB1    # другой порт
```

**Windows:**
```bat
flash_usb.bat            (порт COM3)
flash_usb.bat COM5       (другой порт)
```

Если на скорости 921600 прошивка обрывается (длинный/плохой кабель), добавить
вторым аргументом `115200`.

### Полный сброс (если плата вела себя странно или стояла другая прошивка)

Стереть всё, включая настройки, затем прошить:

```bash
esptool.py --chip esp32 --port /dev/ttyUSB0 erase_flash
./flash_usb.sh
```

После этого плата поднимет точку доступа `MKS_DLC` (пароль `12345678`),
веб-интерфейс: `http://192.168.4.1`. Подключение к домашней сети — в
WebUI → Settings (сеть должна быть **WPA2, 2.4 ГГц** — WPA3-only ESP32 не умеет).

## Обновление по воздуху (OTA), когда плата уже в сети

Из дерева исходников (нужен PlatformIO):

```bash
pio run -e mks_dlc32_v2_1_ota -t upload --upload-port <IP-платы>
```

Только WebUI (SPIFFS): `pio run -e mks_dlc32_v2_1 -t uploadfs` по USB.

## Откуда взялись бинарники

Собраны из этого репозитория: `pio run -e mks_dlc32_v2_1` (+`-t buildfs` для
SPIFFS); `bootloader.bin`/`boot_app0.bin` — стандартные из arduino-esp32 1.0.6
(`bootloader_dout_80m.bin`). Адреса соответствуют тому, что пишет
`pio run -t upload` (проверено `-v`).
