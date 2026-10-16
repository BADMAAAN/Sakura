# Сборка SAKURA

Собирайте из корня checkout. Среда SAKURA — **`flux-delta`**, определённая в [boards/flux-delta.ini](../boards/flux-delta.ini). Она наследует совместимый target FLUX и плату T-Embed CC1101. Общий default environment в `platformio.ini` не выбирает SAKURA.

## Инструменты

Нужны Python с PlatformIO, PowerShell для wrapper на Windows и Pillow для подготовки встроенного брендинга/preview. Используйте существующую рабочую среду. Платформа ESP32 закреплена в [platformio.ini](../platformio.ini); не обновляйте её вместе с обычным исправлением UI. Локальная `.tools/platformio-core`, если присутствует, используется wrapper автоматически.

```powershell
.\scripts\build.ps1 -Environment flux-delta -Python .\.venv\Scripts\python.exe
```

`-Python` указывает на реально существующий интерпретатор с PlatformIO; также поддерживается `FLUX_PYTHON`. Wrapper не допускает одновременные сборки PlatformIO. Его действие — build-firmware, без автоматической установки на устройство. Если Python установлен в другом месте, измените только путь аргумента.

Основной образ: `.pio/build/flux-delta/firmware.bin`. Проверьте итоговый exit code и строку размера приложения, а затем hash:

```powershell
Get-FileHash .pio\build\flux-delta\firmware.bin -Algorithm SHA256
```

Предел app-раздела текущей таблицы — 4 653 056 байт. Не расширяйте его за счёт файловой системы. Другие выдаваемые сборкой образы не взаимозаменяемы с application-only.

## Host-проверки и preview

```powershell
.\.venv\Scripts\python.exe -m unittest discover -s tests -v
.\.venv\Scripts\python.exe tools\delta_preview\preview.py
```

C++ host-проверкам нужен установленный компилятор; существующие тесты умеют находить локальный Zig в `.tools/zig`, некоторые проверки требуют именно Windows Zig/DLL. Preview требует Pillow. Веб-просмотр обычно доступен на `http://127.0.0.1:8765`; он моделирует статус и не запускает аппаратные модули.

`test_qr_render.py` выполняет реальный встроенный QR-кодировщик и требует `zxing-cpp` (обычная установка либо локальная `.tools/qr-check`) для точного декодирования пикселей. Без декодера проверка не считается PASS. Сравниваются ASCII, UTF-8 и граничные 154 байта; 155 байт отклоняются. Чтение камерой проверяется отдельно.

Проверяйте доступные цели через `scripts/pio.py --help`; для regression исходного интерфейса используется `flux-t-embed-cc1101`. Историческая конфигурация описана в [BUILD.md](../BUILD.md). Результаты прежних сборок не являются подтверждением текущего бинарного файла. Установка описана [отдельно](INSTALLATION.md).

WireGuard закреплён на использованном commit `4e22d2e16b70072c105de577d4e79350c1eec932`: плавающая ветка изменила четыре связанных исходника при проверке ZIP 0.3.0. Обычный resolver может обращаться к сети даже при сохранённых `.pio/libdeps`; полный локальный ZIP не заявлен автономным offline toolchain. Не убирайте `lib_deps` ради обхода resolver: PlatformIO удаляет неиспользуемый storage, а forced selection NimBLE требуется сборке.
