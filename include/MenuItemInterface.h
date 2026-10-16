#ifndef __MENU_ITEM_INTERFACE_H__
#define __MENU_ITEM_INTERFACE_H__

#include "core/display.h"
#include <globals.h>
#include "FluxIconCanvas.h"

class MenuItemInterface {
public:
    virtual ~MenuItemInterface() = default;
    virtual void optionsMenu(void) = 0;
    virtual void drawIcon(float scale = 1) = 0;
    virtual void drawIconImg() {
        drawImg(
            *fluxConfig.themeFS(),
            fluxConfig.getThemeItemImg(themePath()),  // const String& - no heap alloc
            0,
            imgCenterY,
            true,
            fluxConfig.theme.gifDuration,
            false
        );
    }
    virtual bool hasTheme() = 0;
    virtual const String& themePath() = 0;

    bool checkTheme() { return hasTheme() && themePath().length() > 0; }
    String getName() const { return String(_name); }
#ifdef FLUX_CLEAN_UI
    void drawIconInto(TFT_eSprite &frame, float scale, int centerY, int areaSize) {
        iconCanvas.target = &frame;
        drawIconAt(scale, centerY, areaSize, false);
        iconCanvas.target = nullptr;
    }
#endif

    void draw(float scale = 1) {
        if (rotation != fluxConfigPins.rotation) resetCoordinates();
        if (!checkTheme()) {
            tft.fillRect(0, 27, tftWidth, tftHeight - 27, fluxConfig.bgColor);
            drawIcon(scale);
#ifndef FLUX_BUILD
            drawArrows(scale);
#endif
            drawTitle(scale);
        } else {
            if (fluxConfig.theme.label)
                drawTitle(scale); // If using .GIF, labels are draw after complete, which takes some time
            drawIconImg();
            if (fluxConfig.theme.label) drawTitle(scale); // Makes sure to draw over the image
        }
        drawStatusBar();
    }

#ifdef FLUX_BUILD
    // Draw only the icon at a temporary vertical center. MainMenu uses this for
    // lightweight transitions without allocating a full-screen sprite.
    void drawIconAt(float scale, int centerY, int areaSize, bool clearArea = true) {
        if (rotation != fluxConfigPins.rotation) resetCoordinates();

        const int savedCenterY = iconCenterY;
        const int savedAreaX = iconAreaX;
        const int savedAreaY = iconAreaY;
        const int savedAreaW = iconAreaW;
        const int savedAreaH = iconAreaH;
        const bool savedClearIconArea = clearIconAreaEnabled;

        iconCenterY = centerY;
        iconAreaW = areaSize;
        iconAreaH = areaSize;
        iconAreaX = iconCenterX - areaSize / 2;
        iconAreaY = iconCenterY - areaSize / 2;
        clearIconAreaEnabled = clearArea;
        drawIcon(scale);

        clearIconAreaEnabled = savedClearIconArea;
        iconCenterY = savedCenterY;
        iconAreaX = savedAreaX;
        iconAreaY = savedAreaY;
        iconAreaW = savedAreaW;
        iconAreaH = savedAreaH;
    }

    // Keep the label attached to an icon while MainMenu animates the complete
    // viewport in the persistent off-screen frame.
    void drawTitleAt(float scale, int centerY, int areaSize) {
        if (rotation != fluxConfigPins.rotation) resetCoordinates();

        const int savedCenterY = iconCenterY;
        const int savedAreaH = iconAreaH;

        iconCenterY = centerY;
        iconAreaH = areaSize;
        drawTitle(scale);

        iconCenterY = savedCenterY;
        iconAreaH = savedAreaH;
    }
#endif

    void drawArrows(float scale = 1) {
        tft.fillRect(arrowAreaX, iconAreaY, arrowAreaW, iconAreaH, fluxConfig.bgColor);
        tft.fillRect(
            tftWidth - arrowAreaX - arrowAreaW, iconAreaY, arrowAreaW, iconAreaH, fluxConfig.bgColor
        );

        int arrowSize = scale * 10;
        int lineWidth = scale * 3;

        int arrowX = BORDER_PAD_X + 1.5 * arrowSize;
        int arrowY = iconCenterY + 1.5 * arrowSize;

        // Left Arrow
        tft.drawWideLine(
            arrowX,
            arrowY,
            arrowX + arrowSize,
            arrowY + arrowSize,
            lineWidth,
            fluxConfig.priColor,
            fluxConfig.bgColor
        );
        tft.drawWideLine(
            arrowX,
            arrowY,
            arrowX + arrowSize,
            arrowY - arrowSize,
            lineWidth,
            fluxConfig.priColor,
            fluxConfig.bgColor
        );

        // Right Arrow
        tft.drawWideLine(
            tftWidth - arrowX,
            arrowY,
            tftWidth - arrowX - arrowSize,
            arrowY + arrowSize,
            lineWidth,
            fluxConfig.priColor,
            fluxConfig.bgColor
        );
        tft.drawWideLine(
            tftWidth - arrowX,
            arrowY,
            tftWidth - arrowX - arrowSize,
            arrowY - arrowSize,
            lineWidth,
            fluxConfig.priColor,
            fluxConfig.bgColor
        );
    }

    void drawTitle(float scale = 1) {
        int titleY = iconCenterY + iconAreaH / 2 + FG;

        tft.setTextSize(FM);
        tft.drawPixel(0, 0, 0);
#ifdef FLUX_BUILD
        tft.fillRect(0, titleY, tftWidth, LH * FM, fluxConfig.bgColor);
#else
        tft.fillRect(arrowAreaX, titleY, tftWidth - 2 * arrowAreaX, LH * FM, fluxConfig.bgColor);
#endif
        int nchars = (tftWidth - 16) / (LW * FM);
        tft.drawCentreString(getName().substring(0, nchars), iconCenterX, titleY, 1);
    }

protected:
#ifdef FLUX_CLEAN_UI
    FluxIconCanvas iconCanvas;
#endif
    const char *_name = "";
    uint8_t rotation = ROTATION;

    int iconAreaH =
        ((tftHeight - 2 * BORDER_PAD_Y) % 2 == 0 ? tftHeight - 2 * BORDER_PAD_Y
                                                 : tftHeight - 2 * BORDER_PAD_Y + 1);
    int iconAreaW = iconAreaH;

    int iconCenterX = tftWidth / 2;
    int iconCenterY = tftHeight / 2;
    int imgCenterY = 13;

    int iconAreaX = iconCenterX - iconAreaW / 2;
    int iconAreaY = iconCenterY - iconAreaH / 2;

    int arrowAreaX = BORDER_PAD_X;
    int arrowAreaW = iconAreaX - arrowAreaX;
#ifdef FLUX_BUILD
    bool clearIconAreaEnabled = true;
#endif

    MenuItemInterface(const char *name) : _name(name) {}

    void clearIconArea(void) {
#ifdef FLUX_BUILD
        if (!clearIconAreaEnabled) return;
#endif
        tft.fillRect(iconAreaX, iconAreaY, iconAreaW, iconAreaH, fluxConfig.bgColor);
    }
    void clearImgArea(void) { tft.fillRect(7, 27, tftWidth - 14, tftHeight - 34, fluxConfig.bgColor); }
    void resetCoordinates(void) {
        // Recalculate Center and ared due to portrait/landscape changings
        if (tftWidth > tftHeight) {
            iconAreaH =
                ((tftHeight - 2 * BORDER_PAD_Y) % 2 == 0 ? tftHeight - 2 * BORDER_PAD_Y
                                                         : tftHeight - 2 * BORDER_PAD_Y + 1);
        } else {
            iconAreaH =
                ((tftWidth - 2 * BORDER_PAD_Y) % 2 == 0 ? tftWidth - 2 * BORDER_PAD_Y
                                                        : tftWidth - 2 * BORDER_PAD_Y + 1);
        }

        iconAreaW = iconAreaH;

        iconCenterX = tftWidth / 2;
        iconCenterY = tftHeight / 2;

        iconAreaX = iconCenterX - iconAreaW / 2;
        iconAreaY = iconCenterY - iconAreaH / 2;

        arrowAreaX = BORDER_PAD_X;
        arrowAreaW = iconAreaX - arrowAreaX;

        rotation = fluxConfigPins.rotation;
    }

private:
};

#endif // __MENU_ITEM_INTERFACE_H__
