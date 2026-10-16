#ifndef __FILE_MENU_H__
#define __FILE_MENU_H__

#include <MenuItemInterface.h>

class FileMenu : public MenuItemInterface {
public:
    FileMenu() : MenuItemInterface("Files") {}

    void optionsMenu(void);
    void drawIcon(float scale);
    bool hasTheme() { return fluxConfig.theme.files; }
    const String& themePath() override { return fluxConfig.theme.paths.files; }
};

#endif
