# Installation

Для настроенного устройства используйте только совместимое application-only обновление. Перед записью нужны: проверенный приватный backup, прочитанная partition table, metadata образа, размер и SHA-256.

Текущие исходники ожидают factory `0x10000`, размер `0x470000`, но запись по этим значениям допустима только после совпадения с установленной разметкой. Файл — `.pio/build/flux-delta/firmware.bin`, не merged distribution image.

Не стирайте flash/NVS, не форматируйте LittleFS/SD и не заменяйте partitions/bootloader для исправления интерфейса. После записи нужны verify, boot log и физическая проверка экрана, батареи, сна и QR.

Подробный протокол и возврат: [docs/INSTALLATION](../docs/INSTALLATION.md). Сборка: [Building from Source](Building-from-Source.md). Эта страница не заявляет, что конкретный образ уже установлен.
