@echo off
rem Прошивка MKS DLC32 v2.1 по USB (Windows).
rem Использование: flash_usb.bat [порт] [скорость]
rem   flash_usb.bat              - порт COM3, 921600
rem   flash_usb.bat COM5
rem   flash_usb.bat COM3 115200  - если на 921600 нестабильно
rem
rem Настройки (Wi-Fi, $$-параметры GRBL) сохраняются - NVS-раздел не трогаем.
rem Нужны: драйвер CH340 и esptool (pip install esptool)
setlocal
cd /d "%~dp0"

set "PORT=%~1"
if "%PORT%"=="" set "PORT=COM3"
set "BAUD=%~2"
if "%BAUD%"=="" set "BAUD=921600"

set "ESPTOOL=python -m esptool"
where esptool.py >nul 2>nul && set "ESPTOOL=esptool.py"

echo Прошиваю через %PORT% @ %BAUD% ...
rem --flash_size/mode/freq обязательны: esptool патчит ими заголовок бутлоадера,
rem без них бутлоадер не примет 8МБ-разметку и плата уйдёт в boot-loop.
%ESPTOOL% --chip esp32 --port %PORT% --baud %BAUD% write_flash ^
    --flash_mode dout --flash_freq 80m --flash_size 8MB ^
    0x1000   bootloader.bin ^
    0x8000   partitions.bin ^
    0xe000   boot_app0.bin ^
    0x10000  firmware.bin ^
    0x610000 spiffs.bin
if errorlevel 1 (
    echo.
    echo ОШИБКА. Проверьте номер COM-порта (Диспетчер устройств - Порты COM и LPT,
    echo устройство CH340) и что установлен esptool: pip install esptool
    exit /b 1
)

echo.
echo Готово. Плата перезагрузится сама.
echo Wi-Fi по умолчанию: точка доступа MKS_DLC (пароль 12345678), веб: http://192.168.4.1
endlocal
