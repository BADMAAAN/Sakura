import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class FluxUiContractTests(unittest.TestCase):
    def test_generated_js_rom_uses_flux_version_api(self) -> None:
        rom = (ROOT / "src/modules/bjs_interpreter/mqjs_stdlib.h").read_text(encoding="utf-8")
        self.assertIn("0x46746567", rom)  # getF, little-endian
        self.assertIn("0x5678756c", rom)  # luxV, little-endian
        self.assertNotIn("0x42746567,\n  0x65637572", rom)

    def test_flux_features_are_menu_gated_not_deleted(self):
        env = (ROOT / "boards/lilygo-t-embed-cc1101/flux-t-embed-cc1101.ini").read_text()
        menu = (ROOT / "src/core/main_menu.cpp").read_text()
        for feature in ("NRF24", "FM", "GPS"):
            self.assertIn(f"FLUX_HIDE_{feature}", env)
            self.assertIn(f"FLUX_HIDE_{feature}", menu)
        self.assertTrue((ROOT / "src/core/menu_items/OthersMenu.cpp").exists())

    def test_customization_is_persistent_and_bounded(self):
        header = (ROOT / "src/core/config.h").read_text()
        config = (ROOT / "src/core/config.cpp").read_text()
        for key in ("menuAnimation", "menuAnimationSpeed", "activeIconEmphasis",
                    "neighboringIcons", "wheelLedEnabled"):
            self.assertIn(key, header)
            self.assertIn(f'setting["{key}"]', config)
        self.assertIn("max(1, min(10, menuAnimationSpeed))", config)

    def test_flux_main_menu_uses_persisted_animation_contract(self):
        menu = (ROOT / "src/core/main_menu.cpp").read_text()
        interface = (ROOT / "include/MenuItemInterface.h").read_text()
        for setting in ("menuAnimation", "menuAnimationSpeed", "activeIconEmphasis",
                        ):
            self.assertIn(f"fluxConfig.{setting}", menu)
        self.assertIn("FLUX_MAIN_FRAME_MS = 16", menu)
        self.assertIn("fluxMainMenuDurationMs", menu)
        self.assertIn("progress * progress * (3.0f - 2.0f * progress)", menu)
        self.assertNotIn("vTaskDelay", menu)
        self.assertIn("tft.fillRect(0, contentY, tftWidth, contentH", menu)
        self.assertNotIn("fillScreen", menu)
        self.assertIn("drawIconAt", interface)

    def test_flux_main_menu_has_no_neighbor_preview_icons(self):
        menu = (ROOT / "src/core/main_menu.cpp").read_text()
        interface = (ROOT / "include/MenuItemInterface.h").read_text()
        self.assertNotIn("_visibleItems[previous]->drawIconAt", menu)
        self.assertNotIn("_visibleItems[next]->drawIconAt", menu)
        self.assertNotIn("fluxConfig.neighboringIcons", menu)
        self.assertIn("item->drawIconAt(activeScale, centerY, activeArea, false)", menu)
        self.assertIn("clearIconAreaEnabled = clearArea", interface)
        self.assertIn("if (!clearIconAreaEnabled) return;", interface)

    def test_flux_main_menu_animation_is_isolated_from_submenu_renderer(self):
        menu = (ROOT / "src/core/main_menu.cpp").read_text()
        display = (ROOT / "src/core/display.cpp").read_text()
        self.assertIn("drawFluxMainMenu", menu)
        self.assertIn(
            "drawSubmenu(index, options, subText, 0, firstRender)",
            display,
        )

    def test_flux_submenu_draws_one_option_without_scrollbar(self):
        display = (ROOT / "src/core/display.cpp").read_text()
        submenu = display[display.index("static bool drawSubmenuBuffered") : display.index("void drawStatusBar")]
        self.assertIn("options[index].label", submenu)
        self.assertNotIn("for (int relative", submenu)
        self.assertNotIn("scrollX", submenu)
        self.assertNotIn("index * viewH / menuSize", submenu)
        self.assertEqual(submenu.count("frame.pushSprite(viewX, viewY)"), 1)

    def test_rotary_navigation_is_inverted_without_reordering_main_items(self):
        menu = (ROOT / "src/core/main_menu.cpp").read_text()
        display = (ROOT / "src/core/display.cpp").read_text()
        self.assertLess(menu.index("&wifiMenu"), menu.index("&bleMenu"))
        self.assertLess(menu.index("&bleMenu"), menu.index("&rfMenu"))
        positive = display[display.index("while (rotarySteps > 0)") : display.index("while (rotarySteps < 0)")]
        negative = display[display.index("while (rotarySteps < 0)") : display.index("renderMoveDirection = rotaryMoveDirection")]
        self.assertIn("findNextEnabled(index, +1)", positive)
        self.assertIn("findNextEnabled(index, -1)", negative)

    def test_flux_status_restores_battery_status(self):
        display = (ROOT / "src/core/display.cpp").read_text()
        status = display[display.index("void drawStatusBar") : display.index("void drawBatteryStatus")]
        self.assertIn("drawBatteryStatus(", status)

    def test_fast_scroll_coalesces_without_discarding_steps(self):
        display = (ROOT / "src/core/display.cpp").read_text()
        spam = (ROOT / "src/modules/ble/ble_spam.cpp").read_text()
        self.assertIn("frame.fillSprite(fluxConfig.bgColor)", display)
        self.assertIn("frame.pushSprite(viewX, viewY)", display)
        self.assertNotIn("SUBMENU_ANIMATION_MS", display)
        self.assertIn("const int32_t rotarySteps = drainRotarySteps()", spam)
        self.assertIn("frame.pushSprite(metrics.list_x, metrics.list_y)", spam)
        self.assertNotIn("LIST_ANIMATION_MS", spam)

    def test_boot_plays_every_private_image_then_waits_for_new_press(self):
        main = (ROOT / "src/main.cpp").read_text()
        boot = main[main.index("void boot_screen_anim()") : main.index("void init_clock()")]
        self.assertNotIn("Preferences", boot)
        self.assertNotIn('"flux_boot"', boot)
        self.assertIn("for (uint8_t index = 0; index < count; ++index)", boot)
        self.assertEqual(boot.count("drawFluxPrivateBranding(index)"), 1)
        self.assertIn("tft.fillScreen(TFT_BLACK)", boot)
        self.assertIn("while (digitalRead(SEL_BTN) == BTN_ACT)", boot)
        self.assertIn("while (!check(SelPress))", boot)
        self.assertNotIn('drawFluxEmbeddedBranding("flux.secondary.png")', boot)

    def test_splash_override_order_and_no_tamagotchi_v01_contract(self):
        main = (ROOT / "src/main.cpp").read_text()
        manifest = (ROOT / "assets/flux/manifest.example.json").read_text()
        self.assertLess(main.index('/flux/assets/branding/flux.splash.jpg'), main.index('/boot.jpg'))
        self.assertNotIn('"tamagotchi"', manifest)
        self.assertNotIn('"required_states"', manifest)


if __name__ == "__main__":
    unittest.main()
