# Архитектура

SAKURA — редакция существующего firmware, а не новая реализация драйверов. Пользовательское имя SAKURA отделено от совместимых `FLUX_*` define, namespace `delta` и target `flux-delta`.

| Слой | Файлы / ответственность |
|---|---|
| Board | `boards/lilygo-t-embed-cc1101/`: GPIO, encoder, I2C gauge, power transitions |
| Основной цикл | `src/main.cpp`, `src/core/main_menu.cpp`: загрузка и dispatch существующих категорий |
| UI adapter | `src/core/delta_ui.cpp`: состояния меню, appearance, общие PSRAM buffers, present |
| Portable renderer | `include/delta/`: композиция, текст, status, widgets; общий для firmware и host |
| Legacy display | `src/core/display.cpp`, `include/tftLogger.h`: старые инструменты и переход к общему UI |
| Модули | `src/modules/`: Wi-Fi, BLE, RF, RFID, IR, scripts и прочие обработчики |
| Configuration | `src/core/config.*`: совместимый `/flux.conf`; appearance отдельно в NVS |
| Тесты | `tests/`: portable C++, runtime fixtures и source invariants |
| Preview | `tools/delta_preview/`: локальный host renderer и моделируемый статус |

Каждый полный RGB565 canvas 320×170 занимает 108 800 байт. Основной frame, предыдущий frame для сравнения и статический фон располагаются в PSRAM при успешном выделении. Буферы переиспользуются; включение кэша не означает отсутствия всех динамических аллокаций в прошивке.

Статичная сцена отделена от динамического overlay с ограниченными лепестками. Retained страницы сравнивают изменённые полосы; неотслеживаемый legacy owner сбрасывает предыдущую валидность. После wake требуется полный present, чтобы partial redraw не оставлял старую GRAM.

Существующие данные не мигрируют массовым переименованием: `/flux.conf` и NVS `flux_delta`/`appearance` сохранены. Appearance имеет version/checksum и миграцию старой записи в памяти. При ошибке LittleFS используется недеструктивный путь без форматирования.

Причины сохранения имён: совместимость конфигурации и малый риск изменения аппаратных путей. Отдельный backend, новая система модулей и общий event bus для этой редакции не требуются. Исторические решения остаются в [docs/decisions](decisions).
