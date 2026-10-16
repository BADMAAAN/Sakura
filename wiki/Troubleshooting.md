# Troubleshooting

Сначала запишите версию/commit, SHA-256 приложения, плату, USB/батарея, ориентацию и точные шаги. Отдельно отметьте: это host-preview или физический ST7789.

При зелёном/неверном цвете проверяют RGB565, swap/inversion, полный framebuffer и sleep/wake; при красной батарее — достоверность I2C, INITCOMP/CFGUPDATE/CALMD и charger status. Нельзя считать эти физические симптомы доказанно устранёнными одним screenshot host-preview.

Если QR отсутствует, начните с Set BTC Address; поддерживаются только mainnet Base58Check 1/3. Если пропала запись настроек, проверьте сообщения storage без форматирования.

Проблемы сборки: explicit target `flux-delta`, рабочий Python с PlatformIO/Pillow, отсутствие второго build. COM-портом управляет один процесс. Уберите личные данные из публикуемых логов.

[Полная таблица диагностики](../docs/TROUBLESHOOTING.md) · [Installation](Installation.md).
