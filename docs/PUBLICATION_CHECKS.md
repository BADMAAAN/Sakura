# Проверка оформления и публикационного состава

Дата: **2026-10-16**. Статус продукта: **Development**, стабильный релиз не заявлен.

## Актуальная основа

Экспорт обновлён из коммита `50518d5964fc1c1006b491286019e341fd59507a`. В нём сохранены keyboard/power hotfix `d3fb18dc`, Sakura Tamagotchi `b0ff9c9a` и их результаты проверки. До оформления все 1949 экспортируемых файлов сопоставлены с Git blobs: различий нет. Прошивка, обработчики, зависимости, аппаратная конфигурация и установленное устройство в этом этапе не изменялись.

Повторный прогон в настроенной рабочей среде исходного проекта: `python -m unittest discover -s tests -v` — **114 тестов PASS за 34,556 с**. Последняя сборка этой основы документирована в [отчёте hotfix](KEYBOARD_TAMAGOTCHI_HOTFIX.md); новая сборка и прошивка устройства на этапе оформления не выполнялись.

Это не результат запуска в чистом опубликованном checkout. Проверки C++ требуют локального Zig, QR — декодера; некоторые исторические regression-тесты требуют объектов прежнего Git. Пробный запуск без этих ресурсов не прошёл. Публикационный снимок не содержит приватный toolchain или старую локальную историю. [Настройка сборки и host-проверок](BUILDING.md).

## RU/EN и графика

Основная страница — корневой русский [README](../README.md); [English](../README.en.md) — полный перевод с обратной относительной ссылкой. README в `.github` отсутствует. Обе версии имеют одинаковые существенные функции, ограничения, команды, даты и назначения документационных ссылок.

Баннер сделан из **`assets/delta/cyber-reference.png`**, визуально подтверждённого одобренного фона: дерево слева, озеро, горы, луна справа. Оригинал сохранён. Повторное кодирование исходника точно совпало с 54 400 индексами background и 64 цветами palette0 в актуальном firmware artwork. Баннер — отдельный JPEG 1440×576; шесть PNG 640×340 получены из общего production renderer 320×170 с синтетическими состояниями. Общий размер семи изображений — **460 605 байт**. [Происхождение и SHA-256 изображений](assets/provenance.json).

Кадры не подтверждают физическую работу LilyGO, радио, SSH или фактический FPS. В них нет реальных SSID, MAC, SSH-адресов, пользователей и платёжных данных. Личные фотографии, старый FLUX preview и сторонние баннеры не используются в презентации.

До push выполнен рендер обеих страниц через **GitHub Markdown API** и проверка в Edge с актуальными стилями GitHub: ширина 1280 и 390 px, светлая и тёмная темы. Все семь изображений загрузились в каждом варианте; горизонтального переполнения нет. Это предварительная проверка renderer; окончательная проверка страницы на github.com выполняется после push. Независимое визуальное ревью охватывает шапку, галерею и документацию.

## Состав публикации

Сохранены [LICENSE](../LICENSE), [THIRD_PARTY](../THIRD_PARTY.md) и copyright notices. Унаследованные workflows и FUNDING сохранены в [неактивном архиве](upstream-workflows/README.md): они не создают релизы и не загружают бинарники при первом push SAKURA. Рабочие исходники и старый FLUX сохраняются отдельно.

Исключены приватные настройки, дампы, сборочные файлы, кэши, toolchain, девять generated build logs и необязательная NFC-анимация с идентификаторами неопределённого происхождения. Существующие проверенные ZIP ресурсов PCB/тем/SSID сохранены; полного ZIP проекта и firmware-бинарников в публикации нет.

Проверка секретов и происхождения имеет практические границы: сигнатурный поиск не доказывает отсутствие любого возможного секрета, сохранение licenses не является новым подтверждением прав каждого унаследованного ресурса. Исторические отчёты и диагностические fixtures не выдаются за текущую физическую квалификацию. [Оставшиеся ограничения](KNOWN_ISSUES.md).

## English

Source: `50518d5964fc1c1006b491286019e341fd59507a`, including keyboard/power and Sakura Tamagotchi hotfixes. The configured source environment passed **114 tests in 34.556 s on 2026-10-16**. This is not a clean-public-checkout test result: native tests need Zig/QR tools and some historical regression tests need prior Git objects. No firmware changes, new device flash or stable release were made in this presentation stage.

RU/EN contents, relative links, development status and limitations match. The exact approved Sakura background was verified against current embedded artwork. Seven optimized images total 460,605 bytes; six UI captures use production rendering with synthetic host states, not physical hardware or live radio results. GitHub Markdown API output was checked at 1280/390 px in light/dark themes before publication; the actual github.com pages are checked after push. Licenses remain intact, private/build files are excluded, and inherited release/funding workflows are archived outside active GitHub configuration.
