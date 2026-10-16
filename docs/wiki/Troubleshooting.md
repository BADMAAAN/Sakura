# Troubleshooting

## Build fails

Remove the local `.pio/` directory and run the build again. Confirm that
PlatformIO can install the declared toolchain and libraries, then review the
first compiler error rather than the final cascade.

## Device is not detected

Use a USB data cable, install the correct USB serial driver, and check the
serial-port list before connecting or resetting the board. Do not erase or
flash a device that contains important private firmware without a verified
full-flash backup.

## UI or input behaves unexpectedly

Record the board revision, build environment, commit SHA and exact reproduction
steps. Check that the selected board environment matches the physical hardware.
Open a GitHub issue only after removing private logs, photos and device dumps.
