#include "qrcode_menu.h"
#include "../lib/TFT_eSPI_QRcode/src/qrcode.h"
#include "core/config.h"
#include "core/display.h"
#include "core/mykeyboard.h"
#include "core/settings.h"
#include "core/sakura_navigation.h"
#include "core/utils.h"
#include "btc_address.h"
#include <algorithm>
#include <mbedtls/sha256.h>

namespace {
bool bitcoinSha256(const uint8_t *data, size_t length, uint8_t *out) {
    return mbedtls_sha256(data, length, out, 0) == 0;
}

void setBitcoinAddress() {
    String address = keyboard(fluxConfig.btcReceiveAddress, 35, "BTC mainnet (1 / 3):");
    if (address == "\x1B") return;
    if (!address.isEmpty() && !sakura::validBitcoinAddress(address.c_str(), bitcoinSha256)) {
        displayError("Invalid BTC address/checksum", true);
        return;
    }
    if (!fluxConfig.setBtcReceiveAddress(address)) displayError("BTC address save failed", true);
}

void bitcoinWallet() {
    while (!returnToMenu) {
        const bool valid = sakura::validBitcoinAddress(fluxConfig.btcReceiveAddress.c_str(), bitcoinSha256);
        std::vector<Option> wallet;
        if (valid) wallet.push_back({"Show BTC QR", []() { qrcode_display(fluxConfig.btcReceiveAddress); }});
        wallet.push_back({"Set BTC Address", setBitcoinAddress});
        wallet.push_back({"Back", []() {}});
        const int selected = loopOptions(wallet, MENU_TYPE_SUBMENU, "My BTC Wallet");
        if (selected < 0 || selected == int(wallet.size()) - 1) return;
    }
}

bool validQrText(const String &text) {
    if (text.isEmpty()) { displayError("QR text cannot be empty", true); return false; }
    if (text.length() > QRcode::maxPayloadBytes) {
        displayError("QR limit: 154 UTF-8 bytes", true);
        return false;
    }
    if (strlen(text.c_str()) != text.length()) {
        displayError("QR text contains NUL", true);
        return false;
    }
    return true;
}

void editSavedQr(uint32_t id) {
    for (const auto &entry : fluxConfig.qrCodes) {
        if (entry.id != id) continue;
        String name = keyboard(entry.menuName, 100, "QRCode name:");
        if (name == "\x1B") return;
        if (name.isEmpty()) { displayError("Name cannot be empty", true); return; }
        if (std::any_of(fluxConfig.qrCodes.begin(), fluxConfig.qrCodes.end(),
                       [&](const FluxConfig::QrCodeEntry &other) {
                           return other.id != id && other.menuName == name;
                       })) { displayError("Name already exists", true); return; }
        String text = keyboard(entry.content, QRcode::maxPayloadBytes, "Link / text:");
        if (text == "\x1B" || !validQrText(text)) return;
        if (!fluxConfig.updateQrCodeEntry(id, name, text)) displayError("QR save failed", true);
        return;
    }
}

void deleteSavedQr(uint32_t id) {
    if (!fluxConfig.removeQrCodeEntry(id)) { displayError("QR delete save failed", true); return; }
#ifdef SAKURA_BUILD
    sakuraForget(SakuraActionKind::Qr, id);
#endif
}

void savedQrDetails(uint32_t id) {
    while (!returnToMenu) {
        auto found = std::find_if(fluxConfig.qrCodes.begin(), fluxConfig.qrCodes.end(),
                                  [id](const FluxConfig::QrCodeEntry &entry) { return entry.id == id; });
        if (found == fluxConfig.qrCodes.end()) return;
        std::vector<Option> details = {
            {"View QR", [id]() { openSavedQr(id); }},
            {"Edit name / text", [id]() { editSavedQr(id); }},
#ifdef SAKURA_BUILD
            {sakuraIsPinned(SakuraActionKind::Qr, id) ? "Unpin" : "Pin to Favorites", [id]() {
                if (!sakuraSetPinned(SakuraActionKind::Qr, id, !sakuraIsPinned(SakuraActionKind::Qr, id)))
                    displayError("Favorites full / save failed", true);
            }},
#endif
            {"Delete", [id]() { deleteSavedQr(id); }},
            {"Back", []() {}}
        };
        const int selected = loopOptions(details, MENU_TYPE_SUBMENU, found->menuName.c_str());
        if (selected < 0 || selected >= int(details.size()) - 2) return;
    }
}
} // namespace

std::vector<SavedQrLink> savedQrLinks() {
    std::vector<SavedQrLink> links;
    if (LittleFS.totalBytes() == 0) return links;
    for (const auto &entry : fluxConfig.qrCodes) links.push_back({entry.id, entry.menuName});
    return links;
}

bool openSavedQr(uint32_t id) {
    if (LittleFS.totalBytes() == 0) return false;
    for (const auto &entry : fluxConfig.qrCodes) {
        if (entry.id != id) continue;
        if (!validQrText(entry.content)) return false;
        qrcode_display(entry.content);
        return true;
    }
    return false;
}

uint16_t crc_ccitt_update(uint16_t crc, uint8_t data) {
    crc = (uint8_t)(crc >> 8) | (crc << 8);
    crc ^= data;
    crc ^= (uint8_t)(crc & 0xff) >> 4;
    crc ^= crc << 12;
    crc ^= (crc & 0x00ff) << 5;
    return crc;
}

String calculate_crc(String input) {
    size_t len = input.length();
    uint8_t *data = (uint8_t *)input.c_str();
    uint16_t crc = 0xffff;

    for (size_t i = 0; i < len; i++) { crc = crc_ccitt_update(crc, data[i]); }

    String crc_str = String(crc, HEX);
    crc_str.toUpperCase();
    while (crc_str.length() < 4) crc_str = "0" + crc_str; // Pad with zeros if needed

    return crc_str;
}

void qrcode_display(const String &qrcodeUrl) {
    if (!validQrText(qrcodeUrl)) return;
#ifdef HAS_SCREEN
    QRcode qrcode(&tft);
    qrcode.init();
    if (!qrcode.create(qrcodeUrl)) {
        displayError("Cannot create QR: memory / screen", true);
        return;
    }
    delay(300); // Due to M5 sel press, it could be confusing with next line
    while (!check(EscPress) && !check(SelPress)) delay(100);
    tft.fillScreen(fluxConfig.bgColor);
#endif
}

void display_custom_qrcode() {
    String message = keyboard("", QRcode::maxPayloadBytes, "QRCode text:");
    if (message == "\x1B") return;
    return qrcode_display(message);
}

void pix_qrcode() {
    String key = keyboard("", 25, "PIX Key:");
    if (key == "\x1B") return;
    String key_length = key.length() >= 10 ? String(key.length()) : "0" + String(key.length());
    String amount = num_keyboard("1000.00", 10, "Int amount:");
    if (amount == "\x1B") return;
    amount = String(amount.toFloat());
    String amount_length = amount.length() >= 10 ? String(amount.length()) : "0" + String(amount.length());

    String data0 = "0014BR.GOV.BCB.PIX01" + key_length + key;

    String pix_code = "00020126" + String(data0.length()) + data0 + "52040000530398654" + amount_length +
                      amount + "5802BR5909Flux PIX6014Rio de Janeiro62070503***6304";
    String crc = calculate_crc(pix_code);

    return qrcode_display(pix_code + crc);
}

void qrcode_menu() {
    while (!returnToMenu) {
        std::vector<Option> entries = {{"My BTC Wallet", bitcoinWallet}, {"New QR", save_and_display_qrcode}};
        for (const auto &entry : fluxConfig.qrCodes) {
            const uint32_t id = entry.id;
            entries.push_back({entry.menuName.c_str(), [id]() { savedQrDetails(id); }});
        }
        entries.push_back({"PIX", pix_qrcode});
        entries.push_back({"Custom", custom_qrcode_menu});
        entries.push_back({"Back", []() {}});
        const int selected = loopOptions(entries, MENU_TYPE_SUBMENU, "My QR");
        if (selected < 0 || selected == int(entries.size()) - 1) return;
    }
}

void custom_qrcode_menu() {
    std::vector<Option> entries = {
        {"Display",      display_custom_qrcode  },
        {"Save&Display", save_and_display_qrcode},
        {"Remove",       remove_custom_qrcode   },
        {"Back",         []() {}               }
    };
    loopOptions(entries, MENU_TYPE_SUBMENU, "Custom QR");
}

void save_and_display_qrcode() {

    String name = keyboard("", 100, "QRCode name:");
    if (name == "\x1B") return;
    if (name.isEmpty()) {
        displayError("Name cannot be empty!");
        delay(1000);
        return;
    }

    if (std::any_of(
            fluxConfig.qrCodes.begin(),
            fluxConfig.qrCodes.end(),
            [&](const FluxConfig::QrCodeEntry &entry) { return entry.menuName == name; }
        )) {
        displayError("Name already exists!");
        delay(1000);
        return;
    }

    String text = keyboard("", QRcode::maxPayloadBytes, "Link / text:");
    if (text == "\x1B" || !validQrText(text)) return;
    if (!fluxConfig.qrNextId) { displayError("QR storage invalid / IDs exhausted", true); return; }

    if (!fluxConfig.addQrCodeEntry(name, text)) { displayError("QR save failed", true); return; }
    return qrcode_display(text);
}

void remove_custom_qrcode() {
    if (fluxConfig.qrCodes.empty()) {
        displayInfo("There is nothing to remove!");
        delay(1000);
        return;
    }
    std::vector<Option> options;

    // Populate options with the QR codes from the config
    for (const auto &entry : fluxConfig.qrCodes) {
        const uint32_t id = entry.id;
        options.emplace_back(entry.menuName.c_str(), [id]() { deleteSavedQr(id); });
    }

    options.emplace_back("Back", []() {});

    loopOptions(options);
}
