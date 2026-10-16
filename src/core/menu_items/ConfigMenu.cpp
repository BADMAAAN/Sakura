#include "ConfigMenu.h"
#include "core/delta_ui.h"
#ifdef SAKURA_BUILD
#include "core/input_diagnostics.h"
#include "core/sakura_navigation.h"
#endif
#include "../mykeyboard.h"
#include "core/display.h"
#include "core/i2c_finder.h"
#include "core/main_menu.h"
#include "core/settings.h"
#include "core/utils.h"
#include "core/wifi/wifi_common.h"
#ifdef HAS_RGB_LED
#include "core/led_control.h"
#endif

/*********************************************************************
**  Function: optionsMenu
**  Main Config menu entry point
**********************************************************************/
void ConfigMenu::optionsMenu() {
    returnToMenu = false;
    while (true) {
        // Check if we need to exit to Main Menu (e.g., DevMode disabled)
        if (returnToMenu) {
            returnToMenu = false; // Reset flag
            return;
        }

        std::vector<Option> localOptions = {
#ifdef FLUX_DELTA_BUILD
            {"Sakura Appearance", deltaAppearanceMenu},
#ifdef SAKURA_BUILD
            {"Diagnostics", inputDiagnosticsMenu},
            {"Recent screens", sakuraRecentMenu},
#endif
#endif
            {"Display & UI",  [this]() { displayUIMenu(); }},
#ifdef FLUX_BUILD
            {"Customization", [this]() { customizationMenu(); }},
#endif
#ifdef HAS_RGB_LED
            {"LED Config",    [this]() { ledMenu(); }      },
#endif
#if !defined(LITE_VERSION) && (defined(BUZZ_PIN) || defined(HAS_NS4168_SPKR))
            {"Audio Config",  [this]() { audioMenu(); }    },
#endif
            {"System Config", [this]() { systemMenu(); }   },
            {"Power",         [this]() { powerMenu(); }    },
        };

#if !defined(LITE_VERSION)
        if (!appStoreInstalled()) {
            localOptions.push_back({"Install App Store", []() { installAppStoreJS(); }});
        }
#endif

        if (fluxConfig.devMode) {
            localOptions.push_back({"Dev Mode", [this]() { devMenu(); }});
        }

        localOptions.push_back({"About", showDeviceInfo});
        localOptions.push_back({"Main Menu", backToMenu});

        int selected = loopOptions(localOptions, MENU_TYPE_SUBMENU, "Config");

        // Exit to Main Menu only if user pressed Back
        if (selected == -1 || selected == localOptions.size() - 1) { return; }
        // Otherwise rebuild Config menu after submenu returns
    }
}

#ifdef FLUX_BUILD
void ConfigMenu::animationModeMenu() {
    std::vector<Option> modes = {
        {"Off",    [this]() { fluxConfig.menuAnimation = 0; fluxConfig.saveFile(); }},
        {"Fast",   [this]() { fluxConfig.menuAnimation = 1; fluxConfig.saveFile(); }},
        {"Smooth", [this]() { fluxConfig.menuAnimation = 2; fluxConfig.saveFile(); }},
    };
    loopOptions(modes, fluxConfig.menuAnimation);
}

void ConfigMenu::animationSpeedMenu() {
    std::vector<Option> speeds;
    for (int value = 1; value <= 10; value++) {
        speeds.push_back({String(value), [value]() {
            fluxConfig.menuAnimationSpeed = value;
            fluxConfig.saveFile();
        }});
    }
    loopOptions(speeds, fluxConfig.menuAnimationSpeed - 1);
}

void ConfigMenu::customizationMenu() {
    while (true) {
        const char *mode = fluxConfig.menuAnimation == 0 ? "Off" :
                           fluxConfig.menuAnimation == 1 ? "Fast" : "Smooth";
        std::vector<Option> localOptions = {
            {String("Menu animation: ") + mode, [this]() { animationModeMenu(); }},
            {String("Animation speed: ") + fluxConfig.menuAnimationSpeed,
             [this]() { animationSpeedMenu(); }},
            {String("Active emphasis: ") + (fluxConfig.activeIconEmphasis ? "ON" : "OFF"), []() {
                 fluxConfig.activeIconEmphasis = !fluxConfig.activeIconEmphasis;
                 fluxConfig.saveFile();
             }},
            {String("Neighbor icons: ") + (fluxConfig.neighboringIcons ? "ON" : "OFF"), []() {
                 fluxConfig.neighboringIcons = !fluxConfig.neighboringIcons;
                 fluxConfig.saveFile();
             }},
#if defined(HAS_ENCODER_LED) && defined(HAS_RGB_LED)
            {String("Wheel LED: ") + (fluxConfig.wheelLedEnabled ? "ON" : "OFF"), []() {
                 fluxConfig.wheelLedEnabled = !fluxConfig.wheelLedEnabled;
                 fluxConfig.saveFile();
                 beginLed();
                 setLedBrightness(fluxConfig.wheelLedEnabled ? fluxConfig.ledBright : 0);
                 if (fluxConfig.wheelLedEnabled) setLedEffect(fluxConfig.ledEffect);
             }},
            {"Wheel brightness", []() {
                 beginLed();
                 setLedBrightnessConfig();
                 if (!fluxConfig.wheelLedEnabled) setLedBrightness(0);
             }},
            {"Wheel effect", []() {
                 beginLed();
                 setLedEffectConfig();
                 if (!fluxConfig.wheelLedEnabled) setLedBrightness(0);
             }},
#endif
            {"Back", []() {}},
        };
        int selected = loopOptions(localOptions, MENU_TYPE_SUBMENU, "Customization");
        if (selected == -1 || selected == localOptions.size() - 1) return;
    }
}
#endif

/*********************************************************************
**  Function: displayUIMenu
**  Display & UI configuration submenu with auto-rebuild
**********************************************************************/
void ConfigMenu::displayUIMenu() {
    while (true) {
        std::vector<Option> localOptions = {
            {"Brightness",  [this]() { setBrightnessMenu(); }               },
            {"Dim Time",    [this]() { setDimmerTimeMenu(); }               },
            {"Orientation", [this]() { lambdaHelper(gsetRotation, true)(); }},
            {"UI Color",    [this]() { setUIColor(); }                      },
            {"UI Theme",    [this]() { setTheme(); }                        },
            {"Back",        []() {}                                         },
        };

        int selected = loopOptions(localOptions, MENU_TYPE_SUBMENU, "Display & UI");

        // Exit only if user pressed Back or ESC
        if (selected == -1 || selected == localOptions.size() - 1) { return; }
        // Otherwise loop continues and menu rebuilds
    }
}

/*********************************************************************
**  Function: ledMenu
**  LED configuration submenu with auto-rebuild for toggles
**********************************************************************/
#ifdef HAS_RGB_LED
void ConfigMenu::ledMenu() {
    while (true) {
        std::vector<Option> localOptions = {
            {"LED Color",
             [this]() {
                 beginLed();
                 setLedColorConfig();
             }                                                                            },
            {"LED Effect",
             [this]() {
                 beginLed();
                 setLedEffectConfig();
             }                                                                            },
            {"LED Brightness",
             [this]() {
                 beginLed();
                 setLedBrightnessConfig();
             }                                                                            },
            {String("LED Blink: ") + (fluxConfig.ledBlinkEnabled ? "ON" : "OFF"),
             [this]() {
                 // Toggle LED blink setting
                 fluxConfig.ledBlinkEnabled = !fluxConfig.ledBlinkEnabled;
                 fluxConfig.saveFile();
             }                                                                            },
            {"Back",                                                               []() {}},
        };

        int selected = loopOptions(localOptions, MENU_TYPE_SUBMENU, "LED Config");

        // Exit only if user pressed Back or ESC
        if (selected == -1 || selected == localOptions.size() - 1) { return; }
        // Menu rebuilds to update toggle label
    }
}
#endif
/*********************************************************************
**  Function: audioMenu
**  Audio configuration submenu with auto-rebuild for toggles
**********************************************************************/
void ConfigMenu::audioMenu() {
    while (true) {
        std::vector<Option> localOptions = {
#if !defined(LITE_VERSION)
#if defined(BUZZ_PIN) || defined(HAS_NS4168_SPKR)

            {String("Sound: ") + (fluxConfig.soundEnabled ? "ON" : "OFF"),
                                                             [this]() {
                 // Toggle sound setting
                 fluxConfig.soundEnabled = !fluxConfig.soundEnabled;
                 fluxConfig.saveFile();
             }                                                                                                                                            },
#if defined(HAS_NS4168_SPKR)
            {"Sound Volume",                                                [this]() { setSoundVolume(); }},
#endif  // BUZZ_PIN || HAS_NS4168_SPKR
#endif  //  HAS_NS4168_SPKR
#endif  //  LITE_VERSION
            {"Back",                                                        []() {}                       },
        };

        int selected = loopOptions(localOptions, MENU_TYPE_SUBMENU, "Audio Config");

        // Exit only if user pressed Back or ESC
        if (selected == -1 || selected == localOptions.size() - 1) { return; }
        // Menu rebuilds to update toggle label
    }
}

/*********************************************************************
**  Function: systemMenu
**  System configuration submenu with auto-rebuild for toggles
**********************************************************************/
void ConfigMenu::systemMenu() {
    while (true) {
        std::vector<Option> localOptions = {
            {String("InstaBoot: ") + (fluxConfig.instantBoot ? "ON" : "OFF"),
             [this]() {
                 // Toggle InstaBoot setting
                 fluxConfig.instantBoot = !fluxConfig.instantBoot;
                 fluxConfig.saveFile();
             }                                                                                                           },
            {String("WiFi Startup: ") + (fluxConfig.wifiAtStartup ? "ON" : "OFF"),
             [this]() {
                 // Toggle WiFi at startup setting
                 fluxConfig.wifiAtStartup = !fluxConfig.wifiAtStartup;
                 fluxConfig.saveFile();
             }                                                                                                           },
            {"Startup App",                                                         [this]() { setStartupApp(); }        },
            {"Hide/Show Apps",                                                      [this]() { mainMenu.hideAppsMenu(); }},
            {"Clock",                                                               [this]() { setClock(); }             },
            {String("Keyboard Language: ") + fluxConfig.keyboardLang,              [this]() { setKeyboardLanguage(); }  },
            {"Advanced",                                                            [this]() { advancedMenu(); }         },
            {"Back",                                                                []() {}                              },
        };

        int selected = loopOptions(localOptions, MENU_TYPE_SUBMENU, "System Config");

        // Exit only if user pressed Back or ESC
        if (selected == -1 || selected == localOptions.size() - 1) { return; }
        // Menu rebuilds to update toggle labels
    }
}

/*********************************************************************
**  Function: advancedMenu
**  Advanced settings submenu (nested under System Config)
**********************************************************************/
void ConfigMenu::advancedMenu() {
    while (true) {
        std::vector<Option> localOptions = {
            {"Set Device pins", [this]() { pinsMenu(); }           },
#if !defined(LITE_VERSION)
            {"Toggle BLE API",  [this]() { enableBLEAPI(); }       },
            {"BadUSB/BLE",      [this]() { setBadUSBBLEMenu(); }   },
#endif
            {"BLE name",
             [this]() {
                 String name = keyboard(fluxConfigPins.bleName, 30, "BLE device name");
                 if (name.length() > 0 && name != "\x1B") fluxConfigPins.setBleName(name);
             }                                                     },
            {"Network Creds",   [this]() { setNetworkCredsMenu(); }},
            {"Factory Reset",
             []() {
                 // Confirmation dialog for destructive action
                 drawMainBorder(true);
                 int8_t choice = displayMessage(
                     "Are you sure you want\nto Factory Reset?\nAll data will be lost!",
                     "No",
                     nullptr,
                     "Yes",
                     TFT_RED
                 );

                 if (choice == 1) {
                     // User confirmed - perform factory reset
                     fluxConfigPins.factoryReset();
                     fluxConfig.factoryReset(); // Restarts ESP
                 }
                 // If cancelled, loop continues and menu rebuilds
             }                                                     },
            {"Back",            []() {}                            },
        };

        int selected = loopOptions(localOptions, MENU_TYPE_SUBMENU, "Advanced");

        // Exit to System Config menu
        if (selected == -1 || selected == localOptions.size() - 1) { return; }
        // Menu rebuilds after each action
    }
}
/*********************************************************************
**  Function: powerMenu
**  Power management submenu with auto-rebuild
**********************************************************************/
void ConfigMenu::powerMenu() {
    while (true) {
        std::vector<Option> localOptions = {
            {"Deep Sleep", goToDeepSleep          },
            {"Sleep",      setSleepMode           },
            {"Restart",    []() { ESP.restart(); }},
            {"Power Off",
             []() {
                 // Confirmation dialog for power off
                 drawMainBorder(true);
                 int8_t choice = displayMessage("Power Off Device?", "No", nullptr, "Yes", TFT_RED);

                 if (choice == 1) { powerOff(); }
             }                                    },
            {"Back",       []() {}                },
        };

        int selected = loopOptions(localOptions, MENU_TYPE_SUBMENU, "Power Menu");

        // Exit to Config menu
        if (selected == -1 || selected == localOptions.size() - 1) { return; }
        // Menu rebuilds after each action
    }
}

/*********************************************************************
**  Function: devMenu
**  Developer mode menu for advanced hardware configuration
**********************************************************************/
void ConfigMenu::devMenu() {
    while (true) {
        std::vector<Option> localOptions = {
            {"Serial use USB",  [this]() { switchToUSBSerial(); }          },
            {"Serial use UART", [this]() { switchToUARTSerial(); }         },
            {"Disable DevMode", [this]() { fluxConfig.setDevMode(false); }},
            {"Back",            []() {}                                    },
        };

        int selected = loopOptions(localOptions, MENU_TYPE_SUBMENU, "Dev Mode");

        // Check if "Disable DevMode" was pressed (second-to-last option)
        if (selected == localOptions.size() - 2) {
            returnToMenu = true; // Signal to exit all Config menus
            return;
        }

        // Exit to Config menu on Back or ESC
        if (selected == -1 || selected == localOptions.size() - 1) { return; }
        // Menu rebuilds after each action
    }
}

/*********************************************************************
**  Function: pinsMenu
**  Developer mode menu for advanced hardware configuration
**********************************************************************/
void ConfigMenu::pinsMenu() {
    while (true) {
        std::vector<Option> localOptions = {
            {"I2C Finder",     [this]() { find_i2c_addresses(); }                      },
            {"CC1101 Pins",    [this]() { setSPIPinsMenu(fluxConfigPins.CC1101_bus); }},
            {"NRF24  Pins",    [this]() { setSPIPinsMenu(fluxConfigPins.NRF24_bus); } },
#if !defined(LITE_VERSION)
            {"LoRa Pins",      [this]() { setSPIPinsMenu(fluxConfigPins.LoRa_bus); }  },
            {"ST25R3916 Pins", [this]() { setSPIPinsMenu(fluxConfigPins.ST25R_bus); } },
            {"W5500 Pins",     [this]() { setSPIPinsMenu(fluxConfigPins.W5500_bus); } },
#endif
            {"SDCard Pins",    [this]() { setSPIPinsMenu(fluxConfigPins.SDCARD_bus); }},
            {"I2C Pins",       [this]() { setI2CPinsMenu(fluxConfigPins.i2c_bus); }   },
            {"UART Pins",      [this]() { setUARTPinsMenu(fluxConfigPins.uart_bus); } },
            {"GPS Pins",       [this]() { setUARTPinsMenu(fluxConfigPins.gps_bus); }  },
            //{"Serial use USB",  [this]() { switchToUSBSerial(); }                       },
            //{"Serial use UART", [this]() { switchToUARTSerial(); }                      },
            {"Back",           []() {}                                                 },
        };

        int selected = loopOptions(localOptions, MENU_TYPE_SUBMENU, "Pins Setup");

        // Exit to Config menu on Back or ESC
        if (selected == -1 || selected == localOptions.size() - 1) { return; }
        // Menu rebuilds after each action
    }
}

/*********************************************************************
**  Function: switchToUSBSerial
**  Switch serial output to USB Serial
**********************************************************************/
void ConfigMenu::switchToUSBSerial() {
    USBserial.setSerialOutput(&Serial);
    Serial1.end();
}

/*********************************************************************
**  Function: switchToUARTSerial
**  Switch serial output to UART (handles pin conflicts)
**********************************************************************/
void ConfigMenu::switchToUARTSerial() {
    // Check and resolve SD card pin conflicts
    if (fluxConfigPins.SDCARD_bus.checkConflict(fluxConfigPins.uart_bus.rx) ||
        fluxConfigPins.SDCARD_bus.checkConflict(fluxConfigPins.uart_bus.tx)) {
        sdcardSPI.end();
    }

    // Check and resolve CC1101/NRF24 pin conflicts
    if (fluxConfigPins.CC1101_bus.checkConflict(fluxConfigPins.uart_bus.rx) ||
        fluxConfigPins.CC1101_bus.checkConflict(fluxConfigPins.uart_bus.tx) ||
        fluxConfigPins.NRF24_bus.checkConflict(fluxConfigPins.uart_bus.rx) ||
        fluxConfigPins.NRF24_bus.checkConflict(fluxConfigPins.uart_bus.tx)) {
        AUX_SPI.end();
    }

    // Configure UART pins and switch serial output
    pinMode(fluxConfigPins.uart_bus.rx, INPUT);
    pinMode(fluxConfigPins.uart_bus.tx, OUTPUT);
    Serial1.begin(115200, SERIAL_8N1, fluxConfigPins.uart_bus.rx, fluxConfigPins.uart_bus.tx);
    USBserial.setSerialOutput(&Serial1);
}
/*********************************************************************
**  Function: drawIcon
**  Draw config gear icon
**********************************************************************/
void ConfigMenu::drawIcon(float scale) {
    clearIconArea();
    int radius = scale * 9;

    // Draw 6 gear teeth segments
    for (int i = 0; i < 6; i++) {
        tft.drawArc(
            iconCenterX,
            iconCenterY,
            3.5 * radius,
            2 * radius,
            15 + 60 * i,
            45 + 60 * i,
            fluxConfig.priColor,
            fluxConfig.bgColor,
            true
        );
    }

    // Draw inner circle
    tft.drawArc(
        iconCenterX,
        iconCenterY,
        2.5 * radius,
        radius,
        0,
        360,
        fluxConfig.priColor,
        fluxConfig.bgColor,
        false
    );
}
