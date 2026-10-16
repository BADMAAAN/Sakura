# LILYGO T-Embed CC1101 Plus hardware map

Status: source-level baseline, not physically validated.

## Official product facts

The current official LILYGO repository identifies ESP32-S3-WROOM-1, 16 MB flash, 8 MB PSRAM, CC1101, PN532 at I2C address 0x24, ST7789 320 x 170, BQ25896 at 0x6B, BQ27220 at 0x55, eight WS2812 LEDs, microphone, speaker, TF card, and a 1300 mAh battery.

Sources:

- https://github.com/Xinyuan-LilyGO/T-Embed-CC1101
- https://github.com/Xinyuan-LilyGO/documentation/blob/master/en/products/t-embed-series/t-embed-cc1101/index.md
- https://github.com/Xinyuan-LilyGO/T-Embed-CC1101/tree/master/hardware

## Confirmed mapping

| Function | Official LILYGO | Bruce baseline |
| --- | --- | --- |
| Shared SPI | MOSI 9, MISO 10, SCK 11 | macros 9, 10, 11 |
| CC1101 | CS 12, GDO0 3, GDO2 38, SW1 47, SW0 48 | exact match |
| SD | CS 13, shared SPI | exact match |
| nRF24 expansion | CE 43, CS 44, shared SPI | exact match |
| I2C | SDA 8, SCL 18 | exact match |
| PN532 | reset 45, IRQ 17, address 0x24 | exact match |
| Encoder | A 4, B 5, key 0 | exact match |
| Microphone | data 42, clock 39 | exact match |
| Speaker | BCLK 46, LRCLK 40, DIN 7 | exact match |
| Battery | BQ25896 0x6B, BQ27220 0x55 | both drivers present |

## Review findings

- The official repository documents known ambiguity in some historical example I2C definitions and recommends SDA 8 / SCL 18. Bruce matches the recommendation.
- Bruce's reversed Arduino SCK/MISO aliases were corrected to match existing macros and official mapping. Both targets compile successfully; physical validation remains pending.
- The shared SPI bus serves display, SD, CC1101, and optional nRF24. Chip-select discipline and bus locking are architecture constraints, not optional optimization.
- Physical operation remains unverified because flashing is explicitly out of scope.
