# FLUX Firmware

![FLUX](assets/flux-preview.jpg)

FLUX is an open-source ESP32 firmware for the LILYGO T-Embed CC1101 Plus. It
combines a fast rotary-encoder UI with Wi-Fi, BLE, RF, LoRa, RFID and IR tools
in a compact handheld interface.

This repository contains the public source tree and build instructions. The
current local build candidate is intentionally not published as a release yet;
hardware validation must happen before a versioned release is created.

## Build from source

Install PlatformIO and the ESP32 toolchain, then run:

```powershell
pio run -e lilygo-t-embed-cc1101
```

See [BUILD.md](BUILD.md) and the [Building-from-Source wiki page](docs/wiki/Building-from-Source.md)
for platform-specific prerequisites and troubleshooting.

## Documentation

Start with the [Wiki home](docs/wiki/Home.md), then see [Installation](docs/wiki/Installation.md),
[Controls](docs/wiki/Controls.md), [Features](docs/wiki/Features.md), and
[Project Structure](docs/wiki/Project-Structure.md).

## Safety and legal notice

Use radio, wireless and security-testing features only on systems, devices and
frequencies where you have explicit authorization. You are responsible for
following local law and the terms of service of any tested system. See the
[Disclaimer](docs/wiki/Disclaimer.md).

## License

FLUX is distributed under the [GNU Affero General Public License v3.0](LICENSE).
Third-party notices are collected in [THIRD_PARTY.md](THIRD_PARTY.md).
