#pragma once
#include <delta/widgets.h>
#include <algorithm>
#include <string>
#include <vector>
#include <functional>
// HOST FIXTURE, not a device photograph. Production painter bodies below are verbatim
// from e4ba2cd9:src/modules/ble/ble_spam.cpp. Raw sprites bypassed DELTA mapping.
namespace delta_ble_before {
using std::min;using std::max;
class String {
 std::string value;
public:
 String(const char*s=""):value(s?s:""){} String(const std::string&s):value(s){} String(uint32_t n):value(std::to_string(n)){}
 const char*c_str()const{return value.c_str();} size_t length()const{return value.size();}
 void remove(size_t n){value.erase(n);} friend String operator+(const String&a,const String&b){return String(a.value+b.value);}
};
#define PROGMEM
#include "../../lib/TFT_eSPI/Fonts/glcdfont.c"
#undef PROGMEM
static const int tftWidth=320,tftHeight=170,BORDER_PAD_X=10,BORDER_PAD_Y=28,FP=1,FM=2,LH=8;
static const uint16_t TFT_DARKGREY=0x7bef,TFT_YELLOW=0xffe0;
static struct {uint16_t priColor=0xa80f,secColor=0x880f,bgColor=0;} fluxConfig;
static delta::Appearance beforeAppearance;
static delta::Status beforeStatus;
static uint16_t mapColor(uint16_t v){const auto c=delta::ui::palette(beforeAppearance.theme);if(v==fluxConfig.bgColor)return c.background;if(v==fluxConfig.priColor)return c.accent;if(v==fluxConfig.secColor)return c.muted;return v;}
struct Raster {
 uint16_t*p=nullptr;int width=320,height=170,size=1;uint16_t fg=0xffff,bg=0;bool mapped=false;
 std::vector<uint16_t> storage;Raster*parent=nullptr;
 Raster(bool mapping=true):mapped(mapping){} Raster(Raster*owner):mapped(false),parent(owner){}
 Raster*native(){return this;} void setColorDepth(int){} void setTextDatum(int){} void setTextSize(int n){size=n;}
 void setTextColor(uint16_t f,uint16_t b){fg=mapped?mapColor(f):f;bg=mapped?mapColor(b):b;}
 int textWidth(const char*t){return int(std::string(t).size())*6*size;}
 void*createSprite(int w,int h){width=w;height=h;storage.resize(w*h);p=storage.data();return p;}
 void deleteSprite(){storage.clear();p=nullptr;}
 void fillRect(int x,int y,int w,int h,uint16_t c){if(mapped)c=mapColor(c);for(int yy=max(0,y);yy<min(height,y+h);++yy)for(int xx=max(0,x);xx<min(width,x+w);++xx)p[yy*width+xx]=c;}
 void fillSprite(uint16_t c){std::fill(p,p+width*height,c);}
 void drawString(const String&s,int x,int y,int){for(const unsigned char*t=(const unsigned char*)s.c_str();*t;++t,x+=6*size)for(int gy=0;gy<8;++gy)for(int gx=0;gx<6;++gx){bool on=gx<5&&(font[*t*5+gx]&(1<<gy));if(on||fg!=bg)for(int yy=0;yy<size;++yy)for(int xx=0;xx<size;++xx){int px=x+gx*size+xx,py=y+gy*size+yy;if(px>=0&&px<width&&py>=0&&py<height)p[py*width+px]=on?fg:bg;}}}
 void drawCentreString(const String&s,int x,int y,int f){drawString(s,x-textWidth(s.c_str())/2,y,f);}
 void drawRightString(const String&s,int x,int y,int f){drawString(s,x-textWidth(s.c_str()),y,f);}
 void pushSprite(int x,int y){for(int yy=0;yy<height;++yy)for(int xx=0;xx<width;++xx)if(x+xx>=0&&x+xx<parent->width&&y+yy>=0&&y+yy<parent->height)parent->p[(y+yy)*parent->width+x+xx]=p[yy*width+xx];}
};
using TFT_eSprite=Raster;static Raster tft;
static void drawMainBorderWithTitle(const String&title){delta::ui::workBackground(tft.p,beforeAppearance);const auto c=delta::ui::palette(beforeAppearance.theme);delta::ui::box(tft.p,0,0,320,32,c.background);delta::ui::header(tft.p,title.c_str(),beforeAppearance,&beforeStatus);tft.setTextDatum(0);tft.setTextSize(FP);tft.setTextColor(c.text,c.background);}
enum BleSpamTxPower { BLE_SPAM_TX_MAX, BLE_SPAM_TX_HIGH, BLE_SPAM_TX_MEDIUM, BLE_SPAM_TX_LOW };
enum BleSpamMacRandMode {
    BLE_SPAM_MAC_OFF,
    BLE_SPAM_MAC_EVERY_PACKET,
    BLE_SPAM_MAC_EVERY_2,
    BLE_SPAM_MAC_EVERY_3,
    BLE_SPAM_MAC_EVERY_5,
    BLE_SPAM_MAC_EVERY_10,
    BLE_SPAM_MAC_EVERY_25,
    BLE_SPAM_MAC_EVERY_50
};
struct BleSpamConfig {
    uint32_t adv_ms = 5;
    uint32_t gap_ms = 5;
    BleSpamTxPower tx_power = BLE_SPAM_TX_MAX;
    BleSpamMacRandMode mac_rand_mode = BLE_SPAM_MAC_EVERY_PACKET;
};
struct BleSpamListState {
    int cursor = 0;
    int scroll = 0;
    bool redraw = true;
};
struct BleSpamEditState {
    bool editing = false;
    int edit_row = 0;
    uint32_t adv_backup = 0;
    uint32_t gap_backup = 0;
    BleSpamTxPower tx_backup = BLE_SPAM_TX_MAX;
    BleSpamMacRandMode mac_backup = BLE_SPAM_MAC_EVERY_PACKET;
};
struct BleSpamListMetrics {
    int list_x = 0;
    int list_y = 0;
    int list_w = 0;
    int list_h = 0;
    int row_h = 0;
    int visible_rows = 0;
    int footer_y = 0;
};
static const char *bleSpamTxPowerLabel(BleSpamTxPower level) {
    switch (level) {
        case BLE_SPAM_TX_MAX: return "MAX";
        case BLE_SPAM_TX_HIGH: return "HIGH";
        case BLE_SPAM_TX_MEDIUM: return "MEDIUM";
        case BLE_SPAM_TX_LOW: return "LOW";
        default: return "MAX";
    }
}

static const char *bleSpamMacRandLabel(BleSpamMacRandMode mode) {
    switch (mode) {
        case BLE_SPAM_MAC_OFF: return "Off";
        case BLE_SPAM_MAC_EVERY_PACKET: return "Every Packet";
        case BLE_SPAM_MAC_EVERY_2: return "Every 2 Packets";
        case BLE_SPAM_MAC_EVERY_3: return "Every 3 Packets";
        case BLE_SPAM_MAC_EVERY_5: return "Every 5 Packets";
        case BLE_SPAM_MAC_EVERY_10: return "Every 10 Packets";
        case BLE_SPAM_MAC_EVERY_25: return "Every 25 Packets";
        case BLE_SPAM_MAC_EVERY_50: return "Every 50 Packets";
        default: return "Every Packet";
    }
}

static String bleSpamFormatMs(uint32_t ms) { return String(ms) + " ms"; }
static BleSpamListMetrics bleSpamGetListMetrics(int footerLines) {
    BleSpamListMetrics metrics;
    metrics.list_x = 10;
    metrics.list_y = BORDER_PAD_Y + FM * LH + 4;
    metrics.list_w = tftWidth - 20;
    int footer_h = footerLines * (FP * LH + 4);
    metrics.footer_y = tftHeight - footer_h - 10;
    metrics.list_h = metrics.footer_y - metrics.list_y - 2;
    metrics.row_h = max(12, FP * LH + 4);
    metrics.visible_rows = max(1, metrics.list_h / metrics.row_h);
    return metrics;
}

static String bleSpamTruncateText(const String &text, int maxWidth) {
    if (tft.textWidth(text.c_str()) <= maxWidth) return text;

    String trimmed = text;
    const String ellipsis = "...";
    int maxTextWidth = maxWidth - tft.textWidth(ellipsis.c_str());
    if (maxTextWidth <= 0) return ellipsis;

    while (trimmed.length() > 0 && tft.textWidth(trimmed.c_str()) > maxTextWidth) {
        trimmed.remove(trimmed.length() - 1);
    }
    return trimmed + ellipsis;
}

static String bleSpamMakeTitle(const String &title) {
    tft.setTextSize(FP);
    int maxWidth = tftWidth - 2 * BORDER_PAD_X;
    return bleSpamTruncateText(title, maxWidth);
}

static void bleSpamRenderList(
    const String &title, int itemCount, const BleSpamListState &state, const BleSpamListMetrics &metrics,
    const std::function<const char *(int)> &labelFn, const char *footer, bool fullRedraw
) {
    if (fullRedraw) {
        drawMainBorderWithTitle(bleSpamMakeTitle(title));
        if (footer != nullptr) {
            tft.setTextSize(FP);
            tft.setTextColor(TFT_DARKGREY, fluxConfig.bgColor);
            tft.fillRect(8, metrics.footer_y, tftWidth - 16, FP * LH, fluxConfig.bgColor);
            tft.drawCentreString(footer, tftWidth / 2, metrics.footer_y + 2, 1);
        }
    }

#if 1 // Device build defines FLUX_BUILD and HAS_SCREEN
    // Keep the title/status layer intact, but build the complete list viewport
    // off-screen. Direct TFT text writes were visible as stale rows while the
    // previous transfer was still scanning the ST7789.
    static TFT_eSprite frame(tft.native());
    static bool frameReady = false;
    static int frameW = 0;
    static int frameH = 0;
    const int frameHeight = metrics.list_h;
    if (frameW != metrics.list_w || frameH != frameHeight) {
        if (frameReady) frame.deleteSprite();
        frame.setColorDepth(16);
        frameReady = frame.createSprite(metrics.list_w, frameHeight);
        frameW = metrics.list_w;
        frameH = frameHeight;
    }

    if (frameReady) {
        frame.fillSprite(fluxConfig.bgColor);
        frame.setTextDatum(0);
        frame.setTextSize(FP);
        for (int row = 0; row < metrics.visible_rows; row++) {
            int idx = state.scroll + row;
            int row_y = row * metrics.row_h;
            if (idx >= itemCount) continue;

            bool selected = (idx == state.cursor);
            uint16_t bg = selected ? fluxConfig.priColor : fluxConfig.bgColor;
            uint16_t fg = selected ? fluxConfig.bgColor : fluxConfig.priColor;
            frame.fillRect(0, row_y, metrics.list_w, metrics.row_h, bg);
            frame.setTextColor(fg, bg);
            String line = String(selected ? "> " : "  ") + labelFn(idx);
            line = bleSpamTruncateText(line, metrics.list_w - 6);
            frame.drawString(line, 4, row_y + 2, 1);
        }

        // A finished frame is transferred once; no row is drawn directly on TFT.
        frame.pushSprite(metrics.list_x, metrics.list_y);
        return;
    }
#endif

    tft.setTextSize(FP);
    tft.fillRect(metrics.list_x, metrics.list_y, metrics.list_w, metrics.list_h, fluxConfig.bgColor);

    for (int row = 0; row < metrics.visible_rows; row++) {
        int idx = state.scroll + row;
        int row_y = metrics.list_y + row * metrics.row_h;
        tft.fillRect(metrics.list_x, row_y, metrics.list_w, metrics.row_h, fluxConfig.bgColor);

        if (idx >= itemCount) continue;

        bool selected = (idx == state.cursor);
        uint16_t bg = selected ? fluxConfig.priColor : fluxConfig.bgColor;
        uint16_t fg = selected ? fluxConfig.bgColor : fluxConfig.priColor;

        tft.fillRect(metrics.list_x, row_y, metrics.list_w, metrics.row_h, bg);
        tft.setTextColor(fg, bg);

        String line = String(selected ? "> " : "  ") + labelFn(idx);
        line = bleSpamTruncateText(line, metrics.list_w - 6);
        tft.drawString(line, metrics.list_x + 4, row_y + 2, 1);
    }

    if (footer != nullptr) {
        tft.setTextColor(TFT_DARKGREY, fluxConfig.bgColor);
        tft.fillRect(8, metrics.footer_y, tftWidth - 16, FP * LH, fluxConfig.bgColor);
        tft.drawCentreString(footer, tftWidth / 2, metrics.footer_y + 2, 1);
    }
}

static void bleSpamRenderConfigRows(
    const BleSpamConfig &config, int cursor, const BleSpamEditState &editState, int startY, int rowH
) {
    tft.setTextSize(FP);

    struct RowInfo {
        const char *label;
        String value;
    } rows[] = {
        {"Adv ms",   bleSpamFormatMs(config.adv_ms)           },
        {"Gap ms",   bleSpamFormatMs(config.gap_ms)           },
        {"TX Power", bleSpamTxPowerLabel(config.tx_power)     },
        {"MAC Rand", bleSpamMacRandLabel(config.mac_rand_mode)}
    };

    for (int i = 0; i < 4; i++) {
        int rowY = startY + i * rowH;
        bool selected = (cursor == i);
        bool editing = (editState.editing && editState.edit_row == i);

        tft.fillRect(10, rowY, tftWidth - 20, rowH, fluxConfig.bgColor);
        uint16_t fg = selected ? TFT_YELLOW : fluxConfig.priColor;
        tft.setTextColor(fg, fluxConfig.bgColor);
        tft.drawString(rows[i].label, 12, rowY + 2, 1);

        String valueText = rows[i].value;
        if (editing) valueText = "[ " + valueText + " ]";
        tft.drawRightString(valueText, tftWidth - 12, rowY + 2, 1);
    }
}

static void frame(uint16_t*p,int scene){
 tft.p=p;beforeAppearance=delta::Appearance();beforeAppearance.reducedMotion=true;beforeStatus=delta::Status();beforeStatus.time="--:--";beforeStatus.battery=-1;
 if(scene==0){const char*labels[]={"Apple Pairing Prompt","Apple Action Modal","Apple Not Your Device","Android Device Alert","Windows Swift Pair","Samsung BLE Spam","BLE Beacon Spam","Random / All"};BleSpamListState state;state.cursor=1;
 bleSpamRenderList("BLE Spam",8,state,bleSpamGetListMetrics(1),[&](int i){return labels[i];},"Click=Select  ESC=Back",true);
 }else{BleSpamConfig config;config.adv_ms=30;config.gap_ms=100;BleSpamEditState editState;int cursor=1;bool redrawRows=true;
 drawMainBorderWithTitle(bleSpamMakeTitle("Apple Action Modal > Config"));
        if (redrawRows) {
            int rowStart = BORDER_PAD_Y + FM * LH + 10;
            int footerH = FP * LH + 4;
            int footerY = tftHeight - footerH - 8;
            int available = footerY - rowStart - 4;
            int rowH = max(1, min(FP * LH + 4, available / 5));
            int startRowY = rowStart + rowH * 4;

            bleSpamRenderConfigRows(config, cursor, editState, rowStart, rowH);

            tft.fillRect(10, startRowY, tftWidth - 20, rowH, fluxConfig.bgColor);
            uint16_t startColor = (cursor == 4) ? TFT_YELLOW : fluxConfig.priColor;
            tft.setTextColor(startColor, fluxConfig.bgColor);
            tft.drawCentreString("[ Start ]", tftWidth / 2, startRowY + 2, 1);

            tft.setTextColor(TFT_DARKGREY, fluxConfig.bgColor);
            tft.fillRect(8, footerY, tftWidth - 16, footerH, fluxConfig.bgColor);
            tft.drawCentreString("Click=Select  ESC=Back", tftWidth / 2, footerY + 2, 1);

            redrawRows = false;
        }
 }
}
} // namespace delta_ble_before
inline void deltaBleBefore(uint16_t*p,int scene){if(p)delta_ble_before::frame(p,scene);}
