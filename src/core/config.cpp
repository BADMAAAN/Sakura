#include "config.h"
#include "mifare_keys_manager.h"
#include "sd_functions.h"
#include <algorithm>

JsonDocument FluxConfig::toJson() const {
    JsonDocument jsonDoc;
    JsonObject setting = jsonDoc.to<JsonObject>();

    setting["priColor"] = String(priColor, HEX);
    setting["secColor"] = String(secColor, HEX);
    setting["bgColor"] = String(bgColor, HEX);
    setting["themeFile"] = themePath;
    setting["themeOnSd"] = theme.fs;

    setting["dimmerSet"] = dimmerSet;
    setting["bright"] = bright;
    setting["automaticTimeUpdateViaNTP"] = automaticTimeUpdateViaNTP;
    setting["tmz"] = tmz;
    setting["dst"] = dst;
    setting["clock24hr"] = clock24hr;
    setting["soundEnabled"] = soundEnabled;
    setting["soundVolume"] = soundVolume;
    setting["wifiAtStartup"] = wifiAtStartup;
    setting["instantBoot"] = instantBoot;
    setting["keyboardLang"] = keyboardLang;

#ifdef FLUX_BUILD
    setting["menuAnimation"] = menuAnimation;
    setting["menuAnimationSpeed"] = menuAnimationSpeed;
    setting["activeIconEmphasis"] = activeIconEmphasis;
    setting["neighboringIcons"] = neighboringIcons;
    setting["wheelLedEnabled"] = wheelLedEnabled;
#endif

#ifdef HAS_RGB_LED
    setting["ledBright"] = ledBright;
    setting["ledColor"] = String(ledColor, HEX);
    setting["ledBlinkEnabled"] = ledBlinkEnabled;
    setting["ledEffect"] = ledEffect;
    setting["ledEffectSpeed"] = ledEffectSpeed;
    setting["ledEffectDirection"] = ledEffectDirection;
#endif

    JsonObject _webUI = setting["webUI"].to<JsonObject>();
    _webUI["user"] = webUI.user;
    _webUI["pwd"] = webUI.pwd;
    JsonObject _webUISessions = setting["webUISessions"].to<JsonObject>();
    for (size_t i = 0; i < webUISessions.size(); i++) { _webUISessions[String(i + 1)] = webUISessions[i]; }

    JsonObject _wifiAp = setting["wifiAp"].to<JsonObject>();
    _wifiAp["ssid"] = wifiAp.ssid;
    _wifiAp["pwd"] = wifiAp.pwd;
    setting["wifiMAC"] = wifiMAC; //@IncursioHack
    setting["TerminalLog"] = TerminalLog;

    JsonArray _evilWifiNames = setting["evilWifiNames"].to<JsonArray>();
    for (auto key : evilWifiNames) _evilWifiNames.add(key);

    JsonObject _evilWifiEndpoints = setting["evilWifiEndpoints"].to<JsonObject>();
    _evilWifiEndpoints["getCredsEndpoint"] = evilPortalEndpoints.getCredsEndpoint;
    _evilWifiEndpoints["setSsidEndpoint"] = evilPortalEndpoints.setSsidEndpoint;
    _evilWifiEndpoints["showEndpoints"] = evilPortalEndpoints.showEndpoints;
    _evilWifiEndpoints["allowSetSsid"] = evilPortalEndpoints.allowSetSsid;
    _evilWifiEndpoints["allowGetCreds"] = evilPortalEndpoints.allowGetCreds;
    _evilWifiEndpoints["gatewayIp"] = evilPortalGatewayIp;

    setting["evilWifiPasswordMode"] = evilPortalPasswordMode;

    JsonObject _wifi = setting["wifi"].to<JsonObject>();
    for (const auto &pair : wifi) { _wifi[pair.first] = pair.second; }

    setting["startupApp"] = startupApp;
    setting["startupAppJSInterpreterFile"] = startupAppJSInterpreterFile;
    setting["wigleBasicToken"] = wigleBasicToken;
    setting["wdgwarsApiKey"] = wdgwarsApiKey;
    setting["devMode"] = devMode;
    setting["colorInverted"] = colorInverted;

    setting["badUSBBLEKeyboardLayout"] = badUSBBLEKeyboardLayout;
    setting["badUSBBLEKeyDelay"] = badUSBBLEKeyDelay;
    setting["badUSBBLEShowOutput"] = badUSBBLEShowOutput;

    JsonArray dm = setting["disabledMenus"].to<JsonArray>();
    for (int i = 0; i < disabledMenus.size(); i++) { dm.add(disabledMenus[i]); }

    JsonArray qrArray = setting["qrCodes"].to<JsonArray>();
    setting["btcReceiveAddress"] = btcReceiveAddress;
    setting["qrNextId"] = qrNextId;
    for (const auto &entry : qrCodes) {
        JsonObject qrEntry = qrArray.add<JsonObject>();
        qrEntry["menuName"] = entry.menuName;
        qrEntry["content"] = entry.content;
        qrEntry["id"] = entry.id;
    }

    return jsonDoc;
}

void FluxConfig::fromFile(bool checkFS) {
    const bool canonicalQr = loadQrFile();
    FS *fs;
    if (checkFS) {
        if (!getFsStorage(fs)) {
            log_i("Fail getting filesystem for config");
            return;
        }
    } else {
        if (checkLittleFsSize()) fs = &LittleFS;
        else return;
    }

    if (!fs->exists(filepath)) {
        log_i("Config file not found. Creating default config");
        return saveFile();
    }

    File file;
    file = fs->open(filepath, FILE_READ);
    if (!file) {
        log_i("Config file not found. Using default values");
        return;
    }

    // Deserialize the JSON document
    JsonDocument jsonDoc;
    if (deserializeJson(jsonDoc, file)) {
        Serial.println("Failed to read config file, using default configuration");
        return;
    }
    file.close();

    JsonObject setting = jsonDoc.as<JsonObject>();
    int count = 0;

    if (!setting["priColor"].isNull()) {
        priColor = strtoul(setting["priColor"], nullptr, 16);
    } else {
        count++;
        log_e("Fail");
    }
    if (!setting["secColor"].isNull()) {
        secColor = strtoul(setting["secColor"], nullptr, 16);
    } else {
        count++;
        log_e("Fail");
    }
    if (!setting["bgColor"].isNull()) {
        bgColor = strtoul(setting["bgColor"], nullptr, 16);
    } else {
        count++;
        log_e("Fail");
    }

    if (!setting["themeFile"].isNull()) {
        themePath = setting["themeFile"].as<String>();
    } else {
        count++;
        log_e("Fail");
    }
    if (!setting["themeOnSd"].isNull()) {
        theme.fs = setting["themeOnSd"].as<int>();
    } else {
        count++;
        log_e("Fail");
    }

    if (!setting["dimmerSet"].isNull()) {
        dimmerSet = setting["dimmerSet"].as<int>();
    } else {
        count++;
        log_e("Fail");
    }
    if (!setting["bright"].isNull()) {
        bright = setting["bright"].as<int>();
    } else {
        count++;
        log_e("Fail");
    }
    if (!setting["automaticTimeUpdateViaNTP"].isNull()) {
        automaticTimeUpdateViaNTP = setting["automaticTimeUpdateViaNTP"].as<bool>();
    } else {
        count++;
        log_e("Fail");
    }
    if (!setting["tmz"].isNull()) {
        tmz = setting["tmz"].as<float>();
    } else {
        count++;
        log_e("Fail");
    }
    if (!setting["dst"].isNull()) {
        dst = setting["dst"].as<bool>();
    } else {
        count++;
        log_e("Fail");
    }
    if (!setting["clock24hr"].isNull()) {
        clock24hr = setting["clock24hr"].as<bool>();
    } else {
        count++;
        log_e("Fail");
    }
    if (!setting["soundEnabled"].isNull()) {
        soundEnabled = setting["soundEnabled"].as<int>();
    } else {
        count++;
        log_e("Fail");
    }
    if (!setting["soundVolume"].isNull()) {
        soundVolume = setting["soundVolume"].as<int>();
    } else {
        count++;
        log_e("Fail");
    }
    if (!setting["wifiAtStartup"].isNull()) {
        wifiAtStartup = setting["wifiAtStartup"].as<int>();
    } else {
        count++;
        log_e("Fail");
    }
    if (!setting["instantBoot"].isNull()) {
        instantBoot = setting["instantBoot"].as<int>();
    } else {
        count++;
        log_e("Fail");
    }
    if (!setting["keyboardLang"].isNull()) {
        keyboardLang = setting["keyboardLang"].as<String>();
    } else {
        keyboardLang = "QWERTY";
    }

#ifdef FLUX_BUILD
    if (!setting["menuAnimation"].isNull()) menuAnimation = setting["menuAnimation"].as<int>();
    if (!setting["menuAnimationSpeed"].isNull())
        menuAnimationSpeed = setting["menuAnimationSpeed"].as<int>();
    if (!setting["activeIconEmphasis"].isNull())
        activeIconEmphasis = setting["activeIconEmphasis"].as<bool>();
    if (!setting["neighboringIcons"].isNull())
        neighboringIcons = setting["neighboringIcons"].as<bool>();
    if (!setting["wheelLedEnabled"].isNull())
        wheelLedEnabled = setting["wheelLedEnabled"].as<bool>();
#endif

#ifdef HAS_RGB_LED
    if (!setting["ledBright"].isNull()) {
        ledBright = setting["ledBright"].as<int>();
    } else {
        count++;
        log_e("Fail");
    }
    if (!setting["ledColor"].isNull()) {
        ledColor = strtoul(setting["ledColor"], nullptr, 16);
    } else {
        count++;
        log_e("Fail");
    }
    if (!setting["ledBlinkEnabled"].isNull()) {
        ledBlinkEnabled = setting["ledBlinkEnabled"].as<int>();
    } else {
        count++;
        log_e("Fail");
    }
    if (!setting["ledEffect"].isNull()) {
        ledEffect = setting["ledEffect"].as<int>();
    } else {
        count++;
        log_e("Fail");
    }
    if (!setting["ledEffectSpeed"].isNull()) {
        ledEffectSpeed = setting["ledEffectSpeed"].as<int>();
    } else {
        count++;
        log_e("Fail");
    }
    if (!setting["ledEffectDirection"].isNull()) {
        ledEffectDirection = setting["ledEffectDirection"].as<int>();
    } else {
        count++;
        log_e("Fail");
    }
#endif

    if (!setting["webUI"].isNull()) {
        JsonObject webUIObj = setting["webUI"].as<JsonObject>();
        webUI.user = webUIObj["user"].as<String>();
        webUI.pwd = webUIObj["pwd"].as<String>();
    } else {
        count++;
        log_e("Fail");
    }

    if (!setting["webUISessions"].isNull()) {
        webUISessions.clear();
        JsonObject webUISessionsObj = setting["webUISessions"].as<JsonObject>();
        for (JsonPair kv : webUISessionsObj) { webUISessions.push_back(kv.value().as<String>()); }
    } else {
        count++;
        log_e("Fail");
    }

    if (!setting["wifiAp"].isNull()) {
        JsonObject wifiApObj = setting["wifiAp"].as<JsonObject>();
        wifiAp.ssid = wifiApObj["ssid"].as<String>();
        wifiAp.pwd = wifiApObj["pwd"].as<String>();
    } else {
        count++;
        log_e("Fail");
    }

    //@IncursioHack
    if (!setting["wifiMAC"].isNull()) {
        wifiMAC = setting["wifiMAC"].as<String>();
    } else {
        wifiMAC = "";
        count++;
        log_e("wifiMAC not found, using default");
    }
    if (!setting["TerminalLog"].isNull()) {
        TerminalLog = setting["TerminalLog"].as<bool>();
    } else {
        count++;
        log_e("TerminalLog not found, using default");
    }

    // Wifi List
    if (!setting["wifi"].isNull()) {
        wifi.clear();
        JsonObject wifiObj = setting["wifi"].as<JsonObject>();
        for (JsonPair kv : wifiObj) wifi[kv.key().c_str()] = kv.value().as<String>();
    } else {
        count++;
        log_e("Fail");
    }

    if (!setting["evilWifiNames"].isNull()) {
        evilWifiNames.clear();
        JsonArray _evilWifiNames = setting["evilWifiNames"].as<JsonArray>();
        for (JsonVariant key : _evilWifiNames) evilWifiNames.insert(key.as<String>());
    } else {
        count++;
        log_e("Fail");
    }

    if (!setting["evilWifiEndpoints"].isNull()) {
        JsonObject evilPortalEndpointsObj = setting["evilWifiEndpoints"].as<JsonObject>();
        evilPortalEndpoints.getCredsEndpoint = evilPortalEndpointsObj["getCredsEndpoint"].as<String>();
        evilPortalEndpoints.setSsidEndpoint = evilPortalEndpointsObj["setSsidEndpoint"].as<String>();
        evilPortalEndpoints.showEndpoints = evilPortalEndpointsObj["showEndpoints"].as<bool>();
        evilPortalEndpoints.allowSetSsid = evilPortalEndpointsObj["allowSetSsid"].as<bool>();
        evilPortalEndpoints.allowGetCreds = evilPortalEndpointsObj["allowGetCreds"].as<bool>();
        if (!evilPortalEndpointsObj["gatewayIp"].isNull()) {
            evilPortalGatewayIp = evilPortalEndpointsObj["gatewayIp"].as<String>();
        } else {
            evilPortalGatewayIp = "172.0.0.1";
        }
    } else {
        count++;
        log_e("Fail");
    }

    if (!setting["evilWifiPasswordMode"].isNull()) {
        int mode = setting["evilWifiPasswordMode"].as<int>();
        if (mode >= 0 && mode <= 2) {
            evilPortalPasswordMode = static_cast<EvilPortalPasswordMode>(mode);
        } else {
            evilPortalPasswordMode = FULL_PASSWORD;
            log_w("Invalid evilWifiPasswordMode, using FULL_PASSWORD");
        }
    } else {
        count++;
        log_e("Fail");
    }

    if (!setting["startupApp"].isNull()) {
        startupApp = setting["startupApp"].as<String>();
    } else {
        count++;
        log_e("Fail");
    }
    if (!setting["startupAppJSInterpreterFile"].isNull()) {
        startupAppJSInterpreterFile = setting["startupAppJSInterpreterFile"].as<String>();
    } else {
        count++;
        log_e("Fail");
    }

    if (!setting["wigleBasicToken"].isNull()) {
        wigleBasicToken = setting["wigleBasicToken"].as<String>();
    } else {
        count++;
        log_e("Fail");
    }
    if (!setting["wdgwarsApiKey"].isNull()) {
        wdgwarsApiKey = setting["wdgwarsApiKey"].as<String>();
    } else {
        count++;
        log_e("Fail");
    }
    if (!setting["devMode"].isNull()) {
        devMode = setting["devMode"].as<int>();
    } else {
        count++;
        log_e("Fail");
    }
    if (!setting["colorInverted"].isNull()) {
        colorInverted = setting["colorInverted"].as<int>();
    } else {
        count++;
        log_e("Fail");
    }

    if (!setting["badUSBBLEKeyboardLayout"].isNull()) {
        badUSBBLEKeyboardLayout = setting["badUSBBLEKeyboardLayout"].as<int>();
    } else {
        count++;
        log_e("Fail");
    }

    if (!setting["badUSBBLEKeyDelay"].isNull()) {
        badUSBBLEKeyDelay = setting["badUSBBLEKeyDelay"].as<int>();
    } else {
        count++;
        log_e("Fail");
    }

    if (!setting["badUSBBLEShowOutput"].isNull()) {
        badUSBBLEShowOutput = setting["badUSBBLEShowOutput"].as<bool>();
    } else {
        count++;
        log_e("Fail");
    }

    if (!setting["disabledMenus"].isNull()) {
        disabledMenus.clear();
        JsonArray dm = setting["disabledMenus"].as<JsonArray>();
        for (JsonVariant e : dm) { disabledMenus.push_back(e.as<String>()); }
    } else {
        count++;
        log_e("Fail");
    }

    if (!canonicalQr) {
        loadQrJson(setting);
        if (setting["qrCodes"].isNull()) { ++count; log_e("Fail to load qrCodes"); }
    }

    validateConfig();
    if (count > 0) saveFile();

    log_i("Using config from file");
}

void FluxConfig::saveFile() {
    if (LittleFS.totalBytes() == 0) {
        log_e("LittleFS unavailable; settings kept in memory without saving");
        return;
    }
    FS *fs = &LittleFS;
    JsonDocument jsonDoc = toJson();

    // Open file for writing
    File file = fs->open(filepath, FILE_WRITE);
    if (!file) {
        log_e("Failed to open config file");
        file.close();
        return;
    };

    // Serialize JSON to file
    serializeJsonPretty(jsonDoc, Serial);
    if (serializeJsonPretty(jsonDoc, file) < 5) log_e("Failed to write config file");
    else log_i("config file written successfully");

    file.close();

    if (setupSdCard()) copyToFs(LittleFS, SD, filepath, false);
}

void FluxConfig::factoryReset() {
    FS *fs = &LittleFS;
    fs->rename(String(filepath), "/bak." + String(filepath).substring(1));
    if (setupSdCard()) SD.rename(String(filepath), "/bak." + String(filepath).substring(1));
    ESP.restart();
}

void FluxConfig::validateConfig() {
    validateDimmerValue();
    validateBrightValue();
    validateTmzValue();
    validateSoundEnabledValue();
    validateSoundVolumeValue();
    validateWifiAtStartupValue();
#ifdef FLUX_BUILD
    menuAnimation = max(0, min(2, menuAnimation));
    menuAnimationSpeed = max(1, min(10, menuAnimationSpeed));
#endif
#ifdef HAS_RGB_LED
    validateLedBrightValue();
    validateLedColorValue();
    validateLedBlinkEnabledValue();
    validateLedEffectValue();
    validateLedEffectSpeedValue();
    validateLedEffectDirectionValue();
#endif
    validateMifareKeysItems();
    validateDevModeValue();
    validateColorInverted();
    validateBadUSBBLEKeyboardLayout();
    validateBadUSBBLEKeyDelay();
    validateEvilEndpointCreds();
    validateEvilEndpointSsid();
    validateEvilPasswordMode();
    validateEvilGatewayIp();
}

void FluxConfig::setUiColor(uint16_t primary, uint16_t *secondary, uint16_t *background) {
    FluxTheme::_setUiColor(primary, secondary, background);
    saveFile();
}

void FluxConfig::setDimmer(int value) {
    dimmerSet = value;
    validateDimmerValue();
    saveFile();
}

void FluxConfig::validateDimmerValue() {
    if (dimmerSet < 0) dimmerSet = 10;
    if (dimmerSet > 60) dimmerSet = 0;
}

void FluxConfig::setBright(uint8_t value) {
    bright = value;
    validateBrightValue();
    saveFile();
}

void FluxConfig::validateBrightValue() {
    if (bright > 100) bright = 100;
}

void FluxConfig::setAutomaticTimeUpdateViaNTP(bool value) {
    automaticTimeUpdateViaNTP = value;
    saveFile();
}

void FluxConfig::setTmz(float value) {
    tmz = value;
    validateTmzValue();
    saveFile();
}

void FluxConfig::validateTmzValue() {
    if (tmz < -12 || tmz > 14) tmz = 0;
}

void FluxConfig::setDST(bool value) {
    dst = value;
    saveFile();
}

void FluxConfig::setClock24Hr(bool value) {
    clock24hr = value;
    saveFile();
}

void FluxConfig::setSoundEnabled(int value) {
    soundEnabled = value;
    validateSoundEnabledValue();
    saveFile();
}

void FluxConfig::setSoundVolume(int value) {
    soundVolume = value;
    validateSoundVolumeValue();
    saveFile();
}

void FluxConfig::validateSoundEnabledValue() {
    if (soundEnabled > 1) soundEnabled = 1;
}

void FluxConfig::validateSoundVolumeValue() {
    if (soundVolume > 100) soundVolume = 100;
}

void FluxConfig::setWifiAtStartup(int value) {
    wifiAtStartup = value;
    validateWifiAtStartupValue();
    saveFile();
}

void FluxConfig::validateWifiAtStartupValue() {
    if (wifiAtStartup > 1) wifiAtStartup = 1;
}

#ifdef HAS_RGB_LED
void FluxConfig::setLedBright(int value) {
    ledBright = value;
    validateLedBrightValue();
    saveFile();
}

void FluxConfig::validateLedBrightValue() { ledBright = max(0, min(100, ledBright)); }

void FluxConfig::setLedColor(uint32_t value) {
    ledColor = value;
    validateLedColorValue();
    saveFile();
}

void FluxConfig::validateLedColorValue() {
    ledColor = max<uint32_t>(0, min<uint32_t>(0xFFFFFFFF, ledColor));
}

void FluxConfig::setLedBlinkEnabled(int value) {
    ledBlinkEnabled = value;
    validateLedBlinkEnabledValue();
    saveFile();
}

void FluxConfig::validateLedBlinkEnabledValue() {
    if (ledBlinkEnabled > 1) ledBlinkEnabled = 1;
}

void FluxConfig::setLedEffect(int value) {
    ledEffect = value;
    validateLedEffectValue();
    saveFile();
}

void FluxConfig::validateLedEffectValue() {
    if (ledEffect < 0 || ledEffect > 9) ledEffect = 0;
}

void FluxConfig::setLedEffectSpeed(int value) {
    ledEffectSpeed = value;
    validateLedEffectSpeedValue();
    saveFile();
}

void FluxConfig::validateLedEffectSpeedValue() {
#ifdef HAS_ENCODER_LED
    if (ledEffectSpeed > 11) ledEffectSpeed = 11;
#else
    if (ledEffectSpeed > 10) ledEffectSpeed = 10;
#endif
    if (ledEffectSpeed < 0) ledEffectSpeed = 1;
}

void FluxConfig::setLedEffectDirection(int value) {
    ledEffectDirection = value;
    validateLedEffectDirectionValue();
    saveFile();
}

void FluxConfig::validateLedEffectDirectionValue() {
    if (ledEffectDirection > 1 || ledEffectDirection == 0) ledEffectDirection = 1;
    if (ledEffectDirection < -1) ledEffectDirection = -1;
}
#endif

void FluxConfig::setWebUICreds(const String &usr, const String &pwd) {
    webUI.user = usr;
    webUI.pwd = pwd;
    saveFile();
}

void FluxConfig::setWifiApCreds(const String &ssid, const String &pwd) {
    wifiAp.ssid = ssid;
    wifiAp.pwd = pwd;
    saveFile();
}

void FluxConfig::setTerminalLog(bool value) {
    TerminalLog = value;
    saveFile();
}

void FluxConfig::addWifiCredential(const String &ssid, const String &pwd) {
    wifi[ssid] = pwd;
    saveFile();
}

String FluxConfig::getWifiPassword(const String &ssid) const {
    auto it = wifi.find(ssid);
    if (it != wifi.end()) return it->second;
    return "";
}

void FluxConfig::addEvilWifiName(String value) {
    evilWifiNames.insert(value);
    saveFile();
}

void FluxConfig::removeEvilWifiName(String value) {
    evilWifiNames.erase(value);
    saveFile();
}

void FluxConfig::setEvilEndpointCreds(String value) {
    evilPortalEndpoints.getCredsEndpoint = value;
    validateEvilEndpointCreds();
    saveFile();
}

void FluxConfig::validateEvilEndpointCreds() {
    if (evilPortalEndpoints.getCredsEndpoint == evilPortalEndpoints.setSsidEndpoint) {
        // on collision reset to defaults
        evilPortalEndpoints.getCredsEndpoint = "/creds";
    }
    if (evilPortalEndpoints.getCredsEndpoint[0] != '/') {
        evilPortalEndpoints.getCredsEndpoint = '/' + evilPortalEndpoints.getCredsEndpoint;
    }
}

void FluxConfig::setEvilEndpointSsid(String value) {
    evilPortalEndpoints.setSsidEndpoint = value;
    validateEvilEndpointCreds();
    saveFile();
}

void FluxConfig::validateEvilEndpointSsid() {
    if (evilPortalEndpoints.getCredsEndpoint == evilPortalEndpoints.setSsidEndpoint) {
        // on collision reset to defaults
        evilPortalEndpoints.setSsidEndpoint = "/ssid";
    }
    if (evilPortalEndpoints.setSsidEndpoint[0] != '/') {
        evilPortalEndpoints.setSsidEndpoint = '/' + evilPortalEndpoints.setSsidEndpoint;
    }
}

void FluxConfig::setEvilAllowEndpointDisplay(bool value) {
    evilPortalEndpoints.showEndpoints = value;
    saveFile();
}

void FluxConfig::setEvilAllowGetCreds(bool value) {
    evilPortalEndpoints.allowGetCreds = value;
    saveFile();
}

void FluxConfig::setEvilAllowSetSsid(bool value) {
    evilPortalEndpoints.allowSetSsid = value;
    saveFile();
}

void FluxConfig::setEvilPasswordMode(EvilPortalPasswordMode value) {
    evilPortalPasswordMode = value;
    saveFile();
}

void FluxConfig::validateEvilPasswordMode() {
    if (evilPortalPasswordMode < 0 || evilPortalPasswordMode > 2) evilPortalPasswordMode = FULL_PASSWORD;
}

void FluxConfig::setEvilGatewayIp(String value) {
    evilPortalGatewayIp = value;
    validateEvilGatewayIp();
    saveFile();
}

void FluxConfig::validateEvilGatewayIp() {
    IPAddress gatewayIp;
    if (!gatewayIp.fromString(evilPortalGatewayIp)) evilPortalGatewayIp = "172.0.0.1";
}

void FluxConfig::setStartupApp(String value) {
    startupApp = value;
    saveFile();
}

void FluxConfig::setStartupAppJSInterpreterFile(String value) {
    startupAppJSInterpreterFile = value;
    saveFile();
}

void FluxConfig::setWigleBasicToken(String value) {
    wigleBasicToken = value;
    saveFile();
}

void FluxConfig::setWdgwarsApiKey(String value) {
    wdgwarsApiKey = value;
    saveFile();
}

void FluxConfig::setDevMode(int value) {
    devMode = value;
    validateDevModeValue();
    saveFile();
}

void FluxConfig::validateDevModeValue() {
    if (devMode > 1) devMode = 1;
}

void FluxConfig::setColorInverted(int value) {
    colorInverted = value;
    validateColorInverted();
    saveFile();
}

void FluxConfig::validateColorInverted() {
    if (colorInverted > 1) colorInverted = 1;
}

void FluxConfig::setBadUSBBLEKeyboardLayout(int value) {
    badUSBBLEKeyboardLayout = value;
    validateBadUSBBLEKeyboardLayout();
    saveFile();
}

void FluxConfig::validateBadUSBBLEKeyboardLayout() {
    if (badUSBBLEKeyboardLayout < 0 || badUSBBLEKeyboardLayout > 13) badUSBBLEKeyboardLayout = 0;
}

void FluxConfig::setBadUSBBLEKeyDelay(uint16_t value) {
    badUSBBLEKeyDelay = value;
    validateBadUSBBLEKeyDelay();
    saveFile();
}

void FluxConfig::validateBadUSBBLEKeyDelay() {
    if (badUSBBLEKeyDelay > 500) badUSBBLEKeyDelay = 500;
}

void FluxConfig::setBadUSBBLEShowOutput(bool value) {
    badUSBBLEShowOutput = value;
    saveFile();
}
void FluxConfig::ensureMifareKeysLoaded() {
    if (!_mifareKeysLoaded) {
        MifareKeysManager::ensureLoaded(mifareKeys);
        _mifareKeysLoaded = true;
    }
}

void FluxConfig::addMifareKey(String value) {
    ensureMifareKeysLoaded();
    MifareKeysManager::addKey(mifareKeys, value);
}

void FluxConfig::validateMifareKeysItems() {
    if (_mifareKeysLoaded) MifareKeysManager::validateKeys(mifareKeys);
}

void FluxConfig::addDisabledMenu(String value) {
    if (std::find(disabledMenus.begin(), disabledMenus.end(), value) != disabledMenus.end()) return;
    disabledMenus.push_back(value);
    saveFile();
}

void FluxConfig::removeDisabledMenu(String value) {
    auto it = std::find(disabledMenus.begin(), disabledMenus.end(), value);
    if (it == disabledMenus.end()) return;
    disabledMenus.erase(it);
    saveFile();
}

void FluxConfig::loadQrJson(JsonObject setting) {
    btcReceiveAddress = setting["btcReceiveAddress"] | "";
    if (!setting["qrCodes"].isNull()) {
        qrCodes.clear();
        qrNextId = setting["qrNextId"] | uint32_t(1);
        JsonArray qrArray = setting["qrCodes"].as<JsonArray>();
        for (JsonObject qrEntry : qrArray) {
            String menuName = qrEntry["menuName"].as<String>();
            String content = qrEntry["content"].as<String>();
            uint32_t id = qrEntry["id"] | uint32_t(0);
            if (id && std::any_of(qrCodes.begin(), qrCodes.end(),
                                 [id](const QrCodeEntry &entry) { return entry.id == id; })) id = 0;
            qrCodes.push_back({menuName, content, id});
            if (qrNextId && id >= qrNextId) qrNextId = id == UINT32_MAX ? 0 : id + 1;
        }
        // Allocate above the saved high-water mark; retired IDs must never be rebound.
        // A legacy file without qrNextId starts at 1. Exhaustion leaves content intact with ID 0.
        for (auto &entry : qrCodes) {
            if (entry.id || !qrNextId) continue;
            entry.id = qrNextId;
            qrNextId = qrNextId == UINT32_MAX ? 0 : qrNextId + 1;
        }
    }
}

bool FluxConfig::loadQrFile() {
    _qrStorageValid = true;
    if (LittleFS.totalBytes() == 0) {
        _qrStorageValid = false;
        qrCodes.clear();
        qrNextId = 0;
        btcReceiveAddress = "";
        log_e("QR storage unavailable; original data retained");
        return true; // Never resolve persistent Favorites against a different legacy store.
    }
    if (!LittleFS.exists("/sakura-qr.json")) return false;
    File file = LittleFS.open("/sakura-qr.json", FILE_READ);
    JsonDocument document;
    bool valid = file && !deserializeJson(document, file) && !document.overflowed();
    file.close();
    JsonObject data = document.as<JsonObject>();
    valid = valid && data["qrCodes"].is<JsonArray>() && data["qrNextId"].is<uint32_t>() &&
            data["btcReceiveAddress"].is<const char *>();
    if (valid) {
        for (JsonObject entry : data["qrCodes"].as<JsonArray>()) {
            if (!entry["id"].is<uint32_t>() || !entry["menuName"].is<const char *>() ||
                !entry["content"].is<const char *>()) { valid = false; break; }
        }
    }
    if (valid) loadQrJson(data);
    else {
        _qrStorageValid = false;
        qrCodes.clear();
        qrNextId = 0;
        btcReceiveAddress = "";
        log_e("QR storage invalid; original file retained");
    }
    return true; // An existing invalid file must never fall back to legacy IDs.
}

bool FluxConfig::saveQrFileChecked() {
    if (!_qrStorageValid || LittleFS.totalBytes() == 0) return false;
    FS *fs = &LittleFS;
    JsonDocument jsonDoc;
    jsonDoc["qrNextId"] = qrNextId;
    jsonDoc["btcReceiveAddress"] = btcReceiveAddress;
    JsonArray entries = jsonDoc["qrCodes"].to<JsonArray>();
    for (const auto &entry : qrCodes) {
        JsonObject saved = entries.add<JsonObject>();
        saved["id"] = entry.id;
        saved["menuName"] = entry.menuName;
        saved["content"] = entry.content;
    }
    if (jsonDoc.overflowed()) return false;
    const size_t expected = measureJson(jsonDoc);
    String payload;
    if (!payload.reserve(expected) || serializeJson(jsonDoc, payload) != expected ||
        payload.length() != expected) return false;
    const char *destination = "/sakura-qr.json";
    const String temporary = "/sakura-qr.json.tmp";
    File file = fs->open(temporary, FILE_WRITE);
    if (!file) return false;
    const size_t written = file.write(reinterpret_cast<const uint8_t *>(payload.c_str()), expected);
    file.flush();
    file.close();
    if (written != expected) { fs->remove(temporary); return false; }
    File verify = fs->open(temporary, FILE_READ);
    bool valid = verify && verify.size() == expected;
    uint8_t block[128];
    for (size_t offset = 0; valid && offset < expected; offset += sizeof(block)) {
        const size_t length = std::min(sizeof(block), expected - offset);
        valid = verify.read(block, length) == length &&
                memcmp(block, payload.c_str() + offset, length) == 0;
    }
    verify.close();
    if (!valid || !fs->rename(temporary, destination)) { fs->remove(temporary); return false; }
    return true;
}

bool FluxConfig::addQrCodeEntry(const String &menuName, const String &content) {
    if (!qrNextId) return false;
    const uint32_t previousNextId = qrNextId;
    qrCodes.push_back({menuName, content, qrNextId++});
    if (saveQrFileChecked()) return true;
    qrCodes.pop_back();
    qrNextId = previousNextId;
    return false;
}

bool FluxConfig::updateQrCodeEntry(uint32_t id, const String &menuName, const String &content) {
    for (auto &entry : qrCodes) {
        if (entry.id != id) continue;
        const QrCodeEntry previous = entry;
        entry.menuName = menuName;
        entry.content = content;
        if (saveQrFileChecked()) return true;
        entry = previous;
        return false;
    }
    return false;
}

bool FluxConfig::removeQrCodeEntry(uint32_t id) {
    const auto previous = qrCodes;
    qrCodes.erase(std::remove_if(qrCodes.begin(), qrCodes.end(),
                                [id](const QrCodeEntry &entry) { return entry.id == id; }), qrCodes.end());
    if (qrCodes.size() == previous.size()) return false;
    if (saveQrFileChecked()) return true;
    qrCodes = previous;
    return false;
}

bool FluxConfig::removeQrCodeEntry(const String &menuName) {
    const auto previous = qrCodes;
    size_t writeIndex = 0;

    for (size_t readIndex = 0; readIndex < qrCodes.size(); ++readIndex) {
        const QrCodeEntry &entry = qrCodes[readIndex];

        if (entry.menuName != menuName) {
            if (writeIndex != readIndex) { qrCodes[writeIndex] = std::move(qrCodes[readIndex]); }
            ++writeIndex;
        }
    }

    if (writeIndex < qrCodes.size()) { qrCodes.erase(qrCodes.begin() + writeIndex, qrCodes.end()); }

    if (qrCodes.size() == previous.size()) return false;
    if (saveQrFileChecked()) return true;
    qrCodes = previous;
    return false;
}

bool FluxConfig::setBtcReceiveAddress(const String &address) {
    const String previous = btcReceiveAddress;
    btcReceiveAddress = address;
    if (saveQrFileChecked()) return true;
    btcReceiveAddress = previous;
    return false;
}

void FluxConfig::addWebUISession(const String &token) {
    webUISessions.push_back(token);
    // Limit to maximum 5 sessions - remove oldest (first element) if exceeded
    if (webUISessions.size() > 5) { webUISessions.erase(webUISessions.begin()); }
    saveFile();
}

void FluxConfig::removeWebUISession(const String &token) {
    for (auto it = webUISessions.begin(); it != webUISessions.end(); ++it) {
        if (*it == token) {
            webUISessions.erase(it);
            break;
        }
    }
    saveFile();
}

bool FluxConfig::isValidWebUISession(const String &token) {
    auto it = std::find(webUISessions.begin(), webUISessions.end(), token);

    if (it == webUISessions.end()) {
        return false; // Token not found
    }

    // Check if token is already at the end (most recent position)
    if (it == webUISessions.end() - 1) {
        return true; // Already most recent, no changes needed
    }

    // Move token to end and save
    webUISessions.erase(it);
    webUISessions.push_back(token);

    // Limit to maximum 10 sessions
    if (webUISessions.size() > 10) { webUISessions.erase(webUISessions.begin()); }

    saveFile();
    return true;
}
