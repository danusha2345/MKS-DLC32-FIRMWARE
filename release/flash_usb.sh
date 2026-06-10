#!/usr/bin/env bash
# Прошивка MKS DLC32 v2.1 по USB (Linux/macOS).
# Использование: ./flash_usb.sh [порт] [скорость]
#   ./flash_usb.sh                  # порт /dev/ttyUSB0, 921600
#   ./flash_usb.sh /dev/ttyUSB1
#   ./flash_usb.sh /dev/ttyUSB0 115200   # если на 921600 нестабильно
#
# Настройки (Wi-Fi, $$-параметры GRBL) сохраняются — NVS-раздел не трогаем.
# Нужен esptool: pip install esptool
set -euo pipefail
cd "$(dirname "$0")"

PORT="${1:-/dev/ttyUSB0}"
BAUD="${2:-921600}"

if command -v esptool.py >/dev/null 2>&1; then
    ESPTOOL="esptool.py"
elif command -v esptool >/dev/null 2>&1; then
    ESPTOOL="esptool"
else
    ESPTOOL="python3 -m esptool"
fi

echo "Прошиваю через $PORT @ $BAUD ..."
# --flash_size/mode/freq ОБЯЗАТЕЛЬНЫ: esptool патчит ими заголовок бутлоадера;
# без них бутлоадер считает флеш меньшим и молча уходит в boot-loop.
$ESPTOOL --chip esp32 --port "$PORT" --baud "$BAUD" write_flash \
    --flash_mode dout --flash_freq 80m --flash_size 8MB \
    0x1000   bootloader.bin \
    0x8000   partitions.bin \
    0xe000   boot_app0.bin \
    0x10000  firmware.bin \
    0x610000 spiffs.bin

echo
echo "Готово. Плата перезагрузится сама."
echo "Wi-Fi по умолчанию: точка доступа MKS_DLC (пароль 12345678), веб: http://192.168.4.1"
