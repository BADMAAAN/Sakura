#include "settings_commands.h"
#include <globals.h>

uint32_t settingsCallback(cmd *c) {
    Command cmd(c);

    Argument setting_name_arg = cmd.getArgument("setting_name");
    Argument setting_value_arg = cmd.getArgument("setting_value");
    String setting_name = setting_name_arg.getValue();
    String setting_value = setting_value_arg.getValue();
    setting_name.trim();
    setting_value.trim();

    JsonDocument jsonDoc = fluxConfig.toJson();
    JsonObject setting = jsonDoc.as<JsonObject>();

    if (setting_name.length() == 0 && setting_value.length() == 0) {
        // no args, just prints current config
        serializeJsonPretty(jsonDoc, Serial);
        serialDevice->println("");
        return true;
    }

    // Pin/device settings live in fluxConfigPins, which is not part of
    // fluxConfig.toJson(); handle them before the fluxConfig whitelist check
    // below (otherwise they are wrongly rejected as "Invalid field name").
    {
        bool isPinField = true;
        if (setting_name == "bleName") fluxConfigPins.setBleName(setting_value);
        else if (setting_name == "irTx") fluxConfigPins.setIrTxPin(setting_value.toInt());
        else if (setting_name == "irTxRepeats")
            fluxConfigPins.setIrTxRepeats(static_cast<uint8_t>(setting_value.toInt()));
        else if (setting_name == "irRx") fluxConfigPins.setIrRxPin(setting_value.toInt());
        else if (setting_name == "rfTx") fluxConfigPins.setRfTxPin(setting_value.toInt());
        else if (setting_name == "rfRx") fluxConfigPins.setRfRxPin(setting_value.toInt());
        else if (setting_name == "rfModule")
            fluxConfigPins.setRfModule(static_cast<RFModules>(setting_value.toInt()));
        else if (setting_name == "rfFreq") fluxConfigPins.setRfFreq(setting_value.toFloat());
        else if (setting_name == "rfFxdFreq") fluxConfigPins.setRfFxdFreq(setting_value.toInt());
        else if (setting_name == "rfScanRange") fluxConfigPins.setRfScanRange(setting_value.toInt());
        else if (setting_name == "rfidModule")
            fluxConfigPins.setRfidModule(static_cast<RFIDModules>(setting_value.toInt()));
        else isPinField = false;
        if (isPinField) {
            serialDevice->println(setting_name + " = " + setting_value);
            return true;
        }
    }

    if (setting[setting_name].isNull()) {
        serialDevice->println("Invalid field name: " + setting_name);
        return false;
    }

    if (setting_value.length() == 0) {
        serialDevice->print(setting_name + " = ");
        serialDevice->println(setting[setting_name].as<String>());
        return true;
    }

    // TODO: improve this logic and move to FluxConfig
    if (setting_name == "priColor") fluxConfig.setUiColor(setting_value.toInt());
    if (setting_name == "rot") fluxConfigPins.setRotation(setting_value.toInt());
    if (setting_name == "dimmerSet") fluxConfig.setDimmer(setting_value.toInt());
    if (setting_name == "bright") fluxConfig.setBright(setting_value.toInt());
    if (setting_name == "tmz") fluxConfig.setTmz(setting_value.toFloat());
    if (setting_name == "soundEnabled") fluxConfig.setSoundEnabled(setting_value.toInt());
    if (setting_name == "wifiAtStartup") fluxConfig.setWifiAtStartup(setting_value.toInt());
    if (setting_name == "webUI") {
        fluxConfig.setWebUICreds(
            setting_value.substring(0, setting_value.indexOf(",")),
            setting_value.substring(setting_value.indexOf(",") + 1)
        );
    }
    if (setting_name == "wifiAp") {
        fluxConfig.setWifiApCreds(
            setting_value.substring(0, setting_value.indexOf(",")),
            setting_value.substring(setting_value.indexOf(",") + 1)
        );
    }
    if (setting_name == "wifi") {
        fluxConfig.addWifiCredential(
            setting_value.substring(0, setting_value.indexOf(",")),
            setting_value.substring(setting_value.indexOf(",") + 1)
        );
    }
    if (setting_name == "wigleBasicToken") fluxConfig.setWigleBasicToken(setting_value);
    if (setting_name == "wdgwarsApiKey") fluxConfig.setWdgwarsApiKey(setting_value);
    if (setting_name == "devMode") fluxConfig.setDevMode(setting_value.toInt());
    if (setting_name == "disabledMenus") fluxConfig.addDisabledMenu(setting_value);

    return true;
}

uint32_t factoryResetCallback(cmd *c) {
    fluxConfig.factoryReset();
    serialDevice->println("Factory reset done");
    return true;
}

void createSettingsCommands(SimpleCLI *cli) {
    cli->addCommand("factory_reset", factoryResetCallback);

    Command cmd = cli->addCommand("set/tings", settingsCallback);
    cmd.addPosArg("setting_name", "");
    cmd.addPosArg("setting_value", "");
}
