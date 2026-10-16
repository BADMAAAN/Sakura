#pragma once
#include <globals.h>

#ifdef FLUX_CLEAN_UI
// Route only icon primitives into the menu's buffer. Other tasks keep using TFT.
class FluxIconCanvas {
public:
    TFT_eSprite *target = nullptr;
#define FLUX_ICON_PRIMITIVE(name) \
    template <typename... Args> void name(Args... args) { \
        if (target) target->name(args...); \
        else ::tft.name(args...); \
    }
    FLUX_ICON_PRIMITIVE(drawArc)
    FLUX_ICON_PRIMITIVE(drawCircle)
    FLUX_ICON_PRIMITIVE(drawEllipse)
    FLUX_ICON_PRIMITIVE(drawLine)
    FLUX_ICON_PRIMITIVE(drawRect)
    FLUX_ICON_PRIMITIVE(drawRoundRect)
    FLUX_ICON_PRIMITIVE(drawTriangle)
    FLUX_ICON_PRIMITIVE(drawWideLine)
    FLUX_ICON_PRIMITIVE(fillCircle)
    FLUX_ICON_PRIMITIVE(fillRect)
    FLUX_ICON_PRIMITIVE(fillRoundRect)
    FLUX_ICON_PRIMITIVE(fillTriangle)
#undef FLUX_ICON_PRIMITIVE
};
#endif
