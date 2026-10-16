# Building from Source

Из корня проекта с настроенным Python/PlatformIO:

```powershell
.\scripts\build.ps1 -Environment flux-delta -Python .\.venv\Scripts\python.exe
.\.venv\Scripts\python.exe -m unittest discover -s tests -v
```

Путь Python должен существовать. Wrapper выбирает project-local core при наличии и запрещает параллельные PlatformIO builds. Для host C++ checks нужен compiler, некоторые проверки используют локальный Windows Zig. Preview запускается `tools/delta_preview/preview.py` и требует Pillow.

Приложение: `.pio/build/flux-delta/firmware.bin`, предел текущего app-раздела 4 653 056 байт. Размер и hash проверяются после build. Не запускайте generic `pio run` без выбора среды: default target другой.

[Полные инструкции](../docs/BUILDING.md) · [Application-only установка](Installation.md). Компиляция и автоматическая физическая установка — разные действия.
