#include "main_menu.h"
#include "display.h"
#include "delta_ui.h"
#include "utils.h"
#include <globals.h>

MainMenu::MainMenu() {
    _menuItems = {
        &wifiMenu,
        &bleMenu,
        &rfMenu,
#if !defined(LITE_VERSION)
        &loraMenu,
#endif
#if !defined(FLUX_HIDE_NRF24)
        &nrf24Menu,
#endif
#if defined(FM_SI4713) && !defined(LITE_VERSION) && !defined(FLUX_HIDE_FM)
        &fmMenu,
#endif
        &irMenu,
#if !defined(LITE_VERSION)
        &ethernetMenu,
#endif
#if !defined(FLUX_HIDE_GPS)
        &gpsMenu,
#endif
        &rfidMenu,
        &fileMenu,
#if !defined(LITE_VERSION) && !defined(DISABLE_INTERPRETER)
        &scriptsMenu,
#endif
        &clockMenu,
        &othersMenu,
        &configMenu,
    };

    _totalItems = _menuItems.size();
}

MainMenu::~MainMenu() {}

#ifdef FLUX_BUILD
namespace {
constexpr uint32_t FLUX_MAIN_FRAME_MS = 16;

struct FluxMainFrame {
    TFT_eSprite sprite;
    bool ready = false;
    int width = 0;
    int height = 0;

    FluxMainFrame() : sprite(tft.native()) {}

    bool ensure() {
        if (ready && width == tftWidth && height == tftHeight - 27) return true;
        if (ready) sprite.deleteSprite();
        sprite.setColorDepth(16);
        ready = sprite.createSprite(tftWidth, tftHeight - 27);
        width = ready ? tftWidth : 0;
        height = ready ? tftHeight - 27 : 0;
        return ready;
    }

    void begin() {
        sprite.fillSprite(fluxConfig.bgColor);
        tft.setRenderTarget(&sprite, 27);
    }

    void end() {
        tft.clearRenderTarget();
        sprite.pushSprite(0, 27);
    }
};

FluxMainFrame &fluxMainFrame() {
    static FluxMainFrame frame;
    return frame;
}

uint32_t fluxMainMenuDurationMs(int mode, int speed) {
    if (mode == 0) return 0;
    const int boundedSpeed = max(1, min(10, speed));
    const uint32_t smoothDuration = static_cast<uint32_t>(120 - boundedSpeed * 4); // 80..116 ms
    const uint32_t fastDuration = smoothDuration > 72 ? smoothDuration - 32 : 48;
    return mode == 1 ? fastDuration : smoothDuration;
}
} // namespace

bool MainMenu::renderFluxMainMenu(void *menuItem, bool shouldRender) {
    if (!shouldRender) return false;
    MenuItemInterface *item = static_cast<MenuItemInterface *>(menuItem);
#ifdef FLUX_DELTA_BUILD
    if (deltaUiSelect(item ? item->getName() : String("Favorites"), mainMenu._mainMoveDirection)) {
        mainMenu._mainMoveDirection = 0;
        return true;
    }
    if (!item) return false;
#endif
    float scale = static_cast<float>(tftWidth) / 240.0f;
    if (fluxConfigPins.rotation & 0b01) scale = static_cast<float>(tftHeight) / 135.0f;
    mainMenu.drawFluxMainMenu(item, scale);
#if defined(HAS_TOUCH)
    TouchFooter();
#endif
    return true;
}

int MainMenu::visibleItemIndex(MenuItemInterface *item) const {
    for (size_t i = 0; i < _visibleItems.size(); ++i) {
        if (_visibleItems[i] == item) return static_cast<int>(i);
    }
    return -1;
}

void MainMenu::drawFluxMainMenuFrame(
    MenuItemInterface *outgoing,
    MenuItemInterface *incoming,
    float scale,
    int offset,
    int direction
) {
    const int contentY = 27;
    const int contentH = tftHeight - contentY;
    const int centerY = contentY + contentH / 2;
    const int travel = contentH;
    const int displayWidth = tftWidth;
    const int areaSize = min(displayWidth, contentH);
    const float activeScale = scale * (fluxConfig.activeIconEmphasis ? 1.0f : 1.0f);

    // Clockwise/NEXT goes down the menu: the new item arrives from below
    // and the old item exits above. Counter-clockwise/PREV is the inverse.
    const int outgoingY = centerY - direction * (travel - offset);
    const int incomingY = centerY + direction * offset;
    if (outgoing != nullptr) {
        outgoing->drawIconAt(activeScale, outgoingY, areaSize, false);
        outgoing->drawTitleAt(scale, outgoingY, areaSize);
    }
    incoming->drawIconAt(activeScale, incomingY, areaSize, false);
    incoming->drawTitleAt(scale, incomingY, areaSize);
}

void MainMenu::drawFluxMainMenuRest(MenuItemInterface *item, float scale) {
    const int contentY = 27;
    const int contentH = tftHeight - contentY;
    const int centerY = contentY + contentH / 2;
    const int displayWidth = tftWidth;
    const int activeArea = min(displayWidth, contentH);
    const float activeScale = scale * (fluxConfig.activeIconEmphasis ? 1.0f : 1.0f);

    tft.fillRect(0, contentY, tftWidth, contentH, fluxConfig.bgColor);
    if (item->checkTheme()) {
        item->draw(activeScale);
        return;
    }

    // MAIN MENU deliberately has one centered icon. Neighbor previews are
    // not drawn here even if the legacy configuration flag is enabled.
    item->drawIconAt(activeScale, centerY, activeArea, false);

    item->drawTitle(scale);
}

void MainMenu::drawFluxMainMenu(MenuItemInterface *item, float scale) {
    // Themes use their own image pipeline. The regular icon menu renders only
    // complete opaque content frames below the independently drawn status bar.
    if (item->checkTheme()) {
        drawMainBorder(false);
        drawFluxMainMenuRest(item, scale);
        _lastRenderedItem = item;
        return;
    }

    const int current = visibleItemIndex(item);
    const int previous = visibleItemIndex(_lastRenderedItem);
    const bool canAnimate = _lastRenderedItem != nullptr && previous >= 0 && current >= 0 && previous != current &&
                            fluxConfig.menuAnimation != 0 && !item->checkTheme() &&
                            !_lastRenderedItem->checkTheme();
    if (canAnimate) {
        const int count = static_cast<int>(_visibleItems.size());
        const int direction = _mainMoveDirection != 0
                                  ? _mainMoveDirection
                                  : (((previous + 1) % count == current) ? 1 : -1);
        const uint32_t duration = fluxMainMenuDurationMs(fluxConfig.menuAnimation, fluxConfig.menuAnimationSpeed);
        const int travel = tftHeight - 27;
        const uint32_t startedAt = millis();
        uint32_t lastFrameAt = startedAt - FLUX_MAIN_FRAME_MS;
        bool interrupted = false;
        bool completed = false;
        while (true) {
            // A new encoder event supersedes the unfinished visual transition.
            // loopOptions will consume the accumulated steps as one batch on
            // its next iteration, so old transitions never form a queue.
            if (RotaryNetSteps != 0) {
                interrupted = true;
                break;
            }
            const uint32_t now = millis();
            const uint32_t elapsed = now - startedAt;
            if (now - lastFrameAt < FLUX_MAIN_FRAME_MS) {
                taskYIELD();
                continue;
            }
            lastFrameAt = now;
            // Snap the last visual frame directly to the settled viewport.
            // This avoids an animation push followed immediately by a second
            // rest-state push when the duration ends between frame ticks.
            const bool finalFrame = elapsed >= duration || elapsed + FLUX_MAIN_FRAME_MS >= duration;
            FluxMainFrame &screen = fluxMainFrame();
            if (!screen.ensure()) break;
            screen.begin();
            if (finalFrame) {
                drawFluxMainMenuRest(item, scale);
                completed = true;
            } else {
                float progress = static_cast<float>(elapsed) / static_cast<float>(duration);
                if (fluxConfig.menuAnimation == 2)
                    progress = progress * progress * (3.0f - 2.0f * progress);
                const int offset = static_cast<int>((1.0f - progress) * travel);
                drawFluxMainMenuFrame(_lastRenderedItem, item, scale, offset, direction);
            }
            screen.end();
            if (completed) break;
            taskYIELD();
        }
        if (interrupted) {
            // Do not flash the stale target at the centre. The next loop pass
            // consumes every pending detent and draws that newest target.
            _lastRenderedItem = nullptr;
            _mainMoveDirection = 0;
            return;
        }
        if (completed) {
            _lastRenderedItem = item;
            _mainMoveDirection = 0;
            return;
        }
    }

    FluxMainFrame &screen = fluxMainFrame();
    if (screen.ensure()) {
        screen.begin();
        drawFluxMainMenuRest(item, scale);
        screen.end();
    } else {
        // Allocation failure is rare, but keep the original renderer as a
        // safe fallback instead of leaving the display blank.
        drawMainBorder(false);
        drawFluxMainMenuRest(item, scale);
    }
    _lastRenderedItem = item;
    _mainMoveDirection = 0;
}
#endif

void MainMenu::begin(void) {
    returnToMenu = false;
    options = {};
#ifdef FLUX_DELTA_BUILD
    deltaUiEnter();
#endif
#ifdef FLUX_BUILD
    _visibleItems.clear();
#endif

    std::vector<String> l = fluxConfig.disabledMenus;
    for (int i = 0; i < _totalItems; i++) {
        String itemName = _menuItems[i]->getName();
        if (find(l.begin(), l.end(), itemName) == l.end()) { // If menu item is not disabled
#ifdef FLUX_BUILD
            _visibleItems.push_back(_menuItems[i]);
#endif
            options.push_back(
                {// selected lambda
                 itemName,
                 [this, i]() { _menuItems[i]->optionsMenu(); },
                 false,                                  // selected = false
                 [](void *menuItem, bool shouldRender) { // render lambda
                     if (!shouldRender) return false;

                     MenuItemInterface *obj = static_cast<MenuItemInterface *>(menuItem);
#ifdef FLUX_BUILD
                     return MainMenu::renderFluxMainMenu(obj, true);
#else
                     float scale = float((float)tftWidth / (float)240);
                     if (fluxConfigPins.rotation & 0b01) scale = float((float)tftHeight / (float)135);
                     drawMainBorder(false);
                     obj->draw(scale);
#if defined(HAS_TOUCH)
                     TouchFooter();
#endif
                     return true;
#endif
                 },
                 _menuItems[i]
                }
            );
        }
    }
#ifdef FLUX_DELTA_BUILD
    options.push_back({"Favorites", deltaFavoritesMenu, false,
                       [](void *, bool render) { return MainMenu::renderFluxMainMenu(nullptr, render); }});
#endif
    _currentIndex = loopOptions(options, MENU_TYPE_MAIN, "Main Menu", _currentIndex);
};

/*********************************************************************
**  Function: hideAppsMenu
**  Menu to Hide or show menus
**********************************************************************/

void MainMenu::hideAppsMenu() {
    auto items = this->getItems();
    int index = 0;
RESTART: // using gotos to avoid stackoverflow after many choices
    options.clear();
    for (auto item : items) {
        String label = item->getName();
        std::vector<String> l = fluxConfig.disabledMenus;
        bool enabled = find(l.begin(), l.end(), label) == l.end();
        options.push_back(
            {label,
             [this, label, enabled]() {
                 if (enabled) fluxConfig.addDisabledMenu(label);
                 else fluxConfig.removeDisabledMenu(label);
             },
             enabled}
        );
    }
    options.push_back({"Show All", [=]() { fluxConfig.disabledMenus.clear(); }, true});
    addOptionToMainMenu();
    index = loopOptions(options, index);
    fluxConfig.saveFile();
    if (!returnToMenu) goto RESTART;
}
