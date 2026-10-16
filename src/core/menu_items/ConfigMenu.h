#ifndef __CONFIG_MENU_H__
#define __CONFIG_MENU_H__

#include <MenuItemInterface.h>

class ConfigMenu : public MenuItemInterface {
public:
    ConfigMenu() : MenuItemInterface("Config") {}

    void optionsMenu(void);
    void drawIcon(float scale);
    bool hasTheme() { return fluxConfig.theme.config; }
    const String &themePath() override { return fluxConfig.theme.paths.config; }

private:
    // Submenus
    void displayUIMenu(void);
#ifdef FLUX_BUILD
    void customizationMenu(void);
    void animationModeMenu(void);
    void animationSpeedMenu(void);
#endif
    void ledMenu(void);
    void audioMenu(void);
    void systemMenu(void);
    void advancedMenu(void);
    void powerMenu(void);
    void pinsMenu(void);
    void devMenu(void);

    // Helper methods for complex operations
    void switchToUSBSerial(void);
    void switchToUARTSerial(void);
};

#endif
