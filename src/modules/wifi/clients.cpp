#ifndef LITE_VERSION
// SSH borrowed from https://github.com/m5stack/M5Cardputer :)

#include "clients.h"
#include "ssh_profiles.h"

// SSH libs
#include "libssh_esp32.h"
#include <libssh/libssh.h>

#include "core/display.h"
#ifdef FLUX_DELTA_BUILD
#include "core/delta_ui.h"
#endif
#include "core/mykeyboard.h"
#include "core/sd_functions.h"
#include "core/wifi/wifi_common.h"
#include <Arduino.h>
#include <errno.h>
#include <atomic>
#include <fcntl.h>
#include <esp_event.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <globals.h>
#include <lwip/sockets.h>
#include <lwip/dns.h>
#include <lwip/tcpip.h>
#include <memory>
#include <string.h>

#ifndef KEY_TAB
#define KEY_TAB 0x2b
#endif
// SSH server configuration
String ssh_host = "";
String ssh_user = "";
String ssh_port = "";
String ssh_password = "";

String commandBuffer = "> ";
int cursorY = 0;
unsigned long lastKeyPressMillis = 0;
const unsigned long debounceDelay = 200;

String telnet_server_string = "";
String telnet_port_string = "";
int telnet_server_port = 23;

bool filterAnsiSequences = true;

namespace {
constexpr size_t CLIENT_IO_BUFFER_SIZE = 256;
constexpr size_t MAX_QUEUED_OUTPUT_BYTES = 128;
constexpr const char *SSH_USER_KNOWN_HOSTS_PATH = "/littlefs/known_hosts";
constexpr const char *SSH_GLOBAL_KNOWN_HOSTS_PATH = "/littlefs/ssh_known_hosts";
constexpr int TERMINAL_PAD_X = 3;
#ifdef FLUX_DELTA_BUILD
constexpr int TERMINAL_PAD_Y = 17; // One compact session row; no decorative frame around terminal data.
#else
constexpr int TERMINAL_PAD_Y = 3;
#endif

enum class ClientProtocol : uint8_t { None, SSH, Telnet };

SemaphoreHandle_t clientStateMutex = nullptr;
TaskHandle_t clientTaskHandle = nullptr;

ClientProtocol activeProtocol = ClientProtocol::None;
File sessionLogFile;
bool sessionLogEnabled = false;
String sessionLogPath = "";
String queuedCommand = "";
String queuedOutput = "";
String queuedPromptPrefix = "> ";
String queuedPromptContext = "";
String pendingTerminalLine = "";
String terminalLiveInput = "";
String pendingEchoSuppression = "";
String sessionStatus = "";
String sessionHost = "";
String sessionUser = "";
uint16_t sessionPort = 0;

bool sessionReady = false;
bool sessionActive = false;
bool sessionClosed = false;
bool sessionFailed = false;
bool sessionConnecting = false;
bool stopSessionRequested = false;
bool previousOutputWasCR = false;
bool awaitingTabDebugResponse = false;
String pendingHostFingerprint;
int hostTrustDecision = 0; // 0 pending, 1 explicitly trusted, -1 rejected.

void clearSshPassword(String &password) {
    for (size_t i = 0; i < password.length(); ++i) password.setCharAt(i, '\0');
    password = "";
}
struct PasswordGuard {
    String &password;
    ~PasswordGuard() { clearSshPassword(password); }
};

enum class EscapeParseState : uint8_t { Normal, Esc, Csi, Osc, OscEsc, Charset };
EscapeParseState escapeParseState = EscapeParseState::Normal;

int telnetSock = -1;

bool initClientMutex() {
    if (clientStateMutex != nullptr) return true;
    clientStateMutex = xSemaphoreCreateMutex();
    return clientStateMutex != nullptr;
}

const char *getProtocolName(ClientProtocol protocol) {
    switch (protocol) {
        case ClientProtocol::SSH: return "SSH";
        case ClientProtocol::Telnet: return "Telnet";
        default: return "Terminal";
    }
}

String sanitizeSessionLogComponent(String value) {
    value.trim();
    if (value.isEmpty()) return "unknown";

    for (size_t i = 0; i < value.length(); ++i) {
        char c = value[i];
        bool allowed = isAlphaNumeric(c) || c == '.' || c == '-' || c == '_';
        if (!allowed) value.setCharAt(i, '_');
    }
    return value;
}

void closeSessionLogUnlocked() {
    if (sessionLogFile) {
        sessionLogFile.flush();
        sessionLogFile.close();
    }
    sessionLogEnabled = false;
    sessionLogPath = "";
}

void appendSessionLogUnlocked(const char *data, size_t len) {
    if (!sessionLogEnabled || !sessionLogFile || data == nullptr || len == 0) return;
    sessionLogFile.write(reinterpret_cast<const uint8_t *>(data), len);
    sessionLogFile.flush();
}

template <typename Fn> void withClientLock(Fn &&fn) {
    if (clientStateMutex && xSemaphoreTake(clientStateMutex, portMAX_DELAY)) {
        fn();
        xSemaphoreGive(clientStateMutex);
    }
}

void appendSessionLog(const String &text) {
    if (text.isEmpty()) return;
    withClientLock([&]() { appendSessionLogUnlocked(text.c_str(), text.length()); });
}

void appendSessionCommandToLog(const String &command) {
    if (command.isEmpty()) return;
    appendSessionLog("\n[CLIENT] " + command + "\n");
}

void startSessionLog(ClientProtocol protocol) {
    withClientLock([&]() {
        closeSessionLogUnlocked();

        if (!fluxConfig.TerminalLog || !sdcardMounted) return;

        if (!SD.exists("/Flux")) SD.mkdir("/Flux");
        if (!SD.exists("/Flux/Terminal")) SD.mkdir("/Flux/Terminal");

        String basePath = "/Flux/Terminal/" + String(getProtocolName(protocol)) + "-" +
                          sanitizeSessionLogComponent(sessionHost);
        for (uint16_t index = 1; index < 10000; ++index) {
            String candidate = basePath + "_" + String(index) + ".log";
            if (SD.exists(candidate)) continue;

            sessionLogFile = SD.open(candidate, FILE_WRITE);
            if (!sessionLogFile) return;

            sessionLogEnabled = true;
            sessionLogPath = candidate;

            String header = "=== Flux Terminal Session ===\n";
            header += "Protocol: " + String(getProtocolName(protocol)) + "\n";
            header += "Host: " + sessionHost + "\n";
            if (!sessionUser.isEmpty()) header += "User: " + sessionUser + "\n";
            header += "Port: " + String(sessionPort) + "\n";
            header += "Started at millis: " + String(millis()) + "\n";
            header += "==============================\n";
            appendSessionLogUnlocked(header.c_str(), header.length());
            return;
        }
    });
}

void setClientTaskHandle(TaskHandle_t handle) {
    withClientLock([&]() { clientTaskHandle = handle; });
}

TaskHandle_t getClientTaskHandle() {
    TaskHandle_t handle = nullptr;
    withClientLock([&]() { handle = clientTaskHandle; });
    return handle;
}

void resetClientState(ClientProtocol protocol) {
    withClientLock([&]() {
        closeSessionLogUnlocked();
        activeProtocol = protocol;
        queuedCommand = "";
        queuedOutput = "";
        queuedPromptPrefix = "> ";
        queuedPromptContext = "";
        pendingTerminalLine = "";
        terminalLiveInput = "";
        pendingEchoSuppression = "";
        sessionStatus = "";
        sessionReady = false;
        sessionActive = false;
        sessionClosed = false;
        sessionFailed = false;
        sessionConnecting = false;
        stopSessionRequested = false;
        previousOutputWasCR = false;
        awaitingTabDebugResponse = false;
        pendingHostFingerprint = "";
        hostTrustDecision = 0;
    });
}

void markSessionConnecting() {
    withClientLock([&]() {
        sessionConnecting = true;
        sessionClosed = false;
        sessionFailed = false;
        sessionActive = true;
    });
}

void markSessionReady() {
    withClientLock([&]() {
        sessionConnecting = false;
        sessionReady = true;
        sessionActive = true;
    });
}

void markSessionClosed(const String &status, bool failed) {
    withClientLock([&]() {
        sessionStatus = status;
        sessionClosed = true;
        sessionFailed = failed;
        sessionActive = false;
        sessionReady = false;
        sessionConnecting = false;
    });
}

void appendSessionOutput(const char *data, size_t len) {
    if (len == 0 || data == nullptr) return;
    withClientLock([&]() {
        queuedOutput.reserve(queuedOutput.length() + len);
        String normalizedOutput = "";
        normalizedOutput.reserve(len);
        for (size_t i = 0; i < len; ++i) {
            uint8_t raw = static_cast<uint8_t>(data[i]);
            char c = static_cast<char>(raw);
            if (filterAnsiSequences) {
                switch (escapeParseState) {
                    case EscapeParseState::Normal:
                        if (raw == 0x1B) {
                            escapeParseState = EscapeParseState::Esc;
                            continue;
                        }
                        break;
                    case EscapeParseState::Esc:
                        if (c == '[') {
                            escapeParseState = EscapeParseState::Csi;
                            continue;
                        }
                        if (c == ']') {
                            escapeParseState = EscapeParseState::Osc;
                            continue;
                        }
                        if (c == '(' || c == ')') {
                            escapeParseState = EscapeParseState::Charset;
                            continue;
                        }
                        escapeParseState = EscapeParseState::Normal;
                        continue;
                    case EscapeParseState::Csi:
                        if (raw >= 0x40 && raw <= 0x7E) escapeParseState = EscapeParseState::Normal;
                        continue;
                    case EscapeParseState::Osc:
                        if (raw == 0x07) escapeParseState = EscapeParseState::Normal;
                        else if (raw == 0x1B) escapeParseState = EscapeParseState::OscEsc;
                        continue;
                    case EscapeParseState::OscEsc:
                        escapeParseState = (c == '\\') ? EscapeParseState::Normal : EscapeParseState::Osc;
                        continue;
                    case EscapeParseState::Charset: escapeParseState = EscapeParseState::Normal; continue;
                }
            }
            if (c == '\r') {
                if (!queuedOutput.isEmpty() && queuedOutput[queuedOutput.length() - 1] != '\n') {
                    queuedOutput += '\n';
                    normalizedOutput += '\n';
                }
                previousOutputWasCR = true;
                continue;
            }
            if (c == '\n' && previousOutputWasCR) {
                previousOutputWasCR = false;
                continue;
            }
            previousOutputWasCR = false;
            if (c == '\t') c = ' ';
            if (raw < 0x20 && c != '\n') continue;
            queuedOutput += c;
            normalizedOutput += c;
        }
        appendSessionLogUnlocked(normalizedOutput.c_str(), normalizedOutput.length());
    });
}

void queueSessionCommand(const String &command) {
    withClientLock([&]() { queuedCommand += command; });
}

String takeQueuedCommand() {
    String command;
    withClientLock([&]() {
        command = queuedCommand;
        queuedCommand = "";
    });
    return command;
}

String takeQueuedOutput() {
    String output;
    withClientLock([&]() {
        output = queuedOutput;
        queuedOutput = "";
    });
    return output;
}

size_t getQueuedOutputLength() {
    size_t length = 0;
    withClientLock([&]() { length = queuedOutput.length(); });
    return length;
}

bool isQueuedOutputBackedUp() { return getQueuedOutputLength() >= MAX_QUEUED_OUTPUT_BYTES; }

String getQueuedPromptPrefix() {
    String prompt;
    withClientLock([&]() { prompt = queuedPromptPrefix; });
    return prompt;
}

String getQueuedPromptContext() {
    String context;
    withClientLock([&]() { context = queuedPromptContext; });
    return context;
}

String getCurrentPromptPrefix() {
    String prompt = getQueuedPromptPrefix();
    if (prompt.isEmpty()) prompt = "> ";
    return prompt;
}

String getSessionStatus() {
    String status;
    withClientLock([&]() { status = sessionStatus; });
    return status;
}

bool isStopRequested() {
    bool stopRequested = false;
    withClientLock([&]() { stopRequested = stopSessionRequested; });
    return stopRequested;
}

void requestSessionStop() {
    withClientLock([&]() { stopSessionRequested = true; });
}

bool isSessionClosed() {
    bool closed = false;
    withClientLock([&]() { closed = sessionClosed; });
    return closed;
}

bool didSessionFail() {
    bool failed = false;
    withClientLock([&]() { failed = sessionFailed; });
    return failed;
}

bool isSessionConnecting() {
    bool connecting = false;
    withClientLock([&]() { connecting = sessionConnecting; });
    return connecting;
}

void resetClientScreen(const char *title) {
#ifdef FLUX_DELTA_BUILD
    if (auto *canvas = deltaUiCanvas(true, false)) {
        const auto c = delta::ui::palette(deltaUiAppearance().theme);
        String context = getQueuedPromptContext();
        String heading = String(title) + (context.isEmpty() ? "" : "  " + context);
        delta::typeface::clip(canvas, heading.c_str(), 4, 1, 232, c.muted, delta::typeface::Small);
        bool ready = false;
        withClientLock([&]() { ready = sessionReady; });
        const char *state = isSessionConnecting() ? "CONNECTING" : ready ? "CONNECTED" : "SESSION";
        delta::typeface::clip(canvas, state, 240, 1, 77, c.accent, delta::typeface::Small);
        delta::ui::box(canvas, 3, 15, 314, 1, c.line);
        deltaUiPresent();
        tft.setTextColor(c.text, c.background);
        tft.setTextSize(FP);
        tft.setCursor(TERMINAL_PAD_X, TERMINAL_PAD_Y);
        cursorY = tft.getCursorY();
        return;
    }
#endif
    (void)title;
    tft.fillScreen(fluxConfig.bgColor);
    tft.drawRect(0, 0, tftWidth, tftHeight, fluxConfig.priColor);
    tft.setTextColor(fluxConfig.priColor, fluxConfig.bgColor);
    tft.setTextSize(FP);
    tft.setCursor(TERMINAL_PAD_X, TERMINAL_PAD_Y);
    String context = getQueuedPromptContext();
    if (!context.isEmpty()) {
        tft.setTextColor(TFT_CYAN, fluxConfig.bgColor);
        tft.println(context);
        tft.setTextColor(fluxConfig.priColor, fluxConfig.bgColor);
        tft.setCursor(TERMINAL_PAD_X, tft.getCursorY());
    }
    cursorY = tft.getCursorY();
}

void resetCommandBufferToPrompt() { commandBuffer = getCurrentPromptPrefix(); }

size_t getCommandPromptLength() {
    String prompt = getCurrentPromptPrefix();
    if (commandBuffer.startsWith(prompt)) return prompt.length();
    if (commandBuffer.startsWith("> ")) return 2;
    return 0;
}

String getBufferedCommandInput() {
    size_t promptLength = getCommandPromptLength();
    if (commandBuffer.length() <= promptLength) return "";
    return commandBuffer.substring(promptLength);
}

void setTerminalLiveInput(const String &input) {
    withClientLock([&]() { terminalLiveInput = input; });
}

String getTerminalLiveInput() {
    String input;
    withClientLock([&]() { input = terminalLiveInput; });
    return input;
}

void syncCommandBufferFromRemote(const String &input) {
    setTerminalLiveInput(input);
    commandBuffer = getCurrentPromptPrefix() + input;
}

void setPendingEchoSuppression(const String &input) {
    withClientLock([&]() { pendingEchoSuppression = input; });
}

void clearTerminalPendingState() {
    withClientLock([&]() {
        pendingTerminalLine = "";
        terminalLiveInput = "";
        pendingEchoSuppression = "";
    });
}

String takePendingEchoSuppression() {
    String input;
    withClientLock([&]() {
        input = pendingEchoSuppression;
        pendingEchoSuppression = "";
    });
    return input;
}

String peekPendingEchoSuppression() {
    String input;
    withClientLock([&]() { input = pendingEchoSuppression; });
    return input;
}

String stripLeadingPromptPrefix(String line) {
    String promptPrefix = getCurrentPromptPrefix();
    if (promptPrefix.isEmpty()) return line;

    while (line.startsWith(promptPrefix)) {
        line.remove(0, promptPrefix.length());
        while (!line.isEmpty() && line[0] == ' ') line.remove(0, 1);
    }
    return line;
}

bool shouldSuppressEchoLine(String &line) {
    String suppressedEcho = peekPendingEchoSuppression();
    if (suppressedEcho.isEmpty()) return false;

    String trimmedLine = line;
    trimmedLine.trim();
    if (trimmedLine.isEmpty()) return false;

    String promptPrefix = getCurrentPromptPrefix();
    String prefixedEcho = promptPrefix + suppressedEcho;

    if (trimmedLine == suppressedEcho || trimmedLine == prefixedEcho) {
        takePendingEchoSuppression();
        line = "";
        return true;
    }

    if (trimmedLine.startsWith(prefixedEcho)) {
        takePendingEchoSuppression();
        line = trimmedLine.substring(prefixedEcho.length());
        line.trim();
        return line.isEmpty();
    }

    return false;
}

void renderPrompt(bool forceNewLine = false);

void ensureCursorOnFreshLine() {
    if (tft.getCursorX() > TERMINAL_PAD_X) {
        tft.println();
        tft.setCursor(TERMINAL_PAD_X, tft.getCursorY());
    }
}

void redrawCurrentCommandLine() {
    int lineHeight = max(1, FP * LH);
    int lineY = tft.getCursorY();
    if (lineY < TERMINAL_PAD_Y) lineY = TERMINAL_PAD_Y;

    tft.fillRect(TERMINAL_PAD_X, lineY, tftWidth - (TERMINAL_PAD_X * 2), lineHeight, fluxConfig.bgColor);
    tft.setCursor(TERMINAL_PAD_X, lineY);
    tft.setTextColor(TFT_GREEN, fluxConfig.bgColor);
    tft.print(commandBuffer);
    tft.setTextColor(fluxConfig.priColor, fluxConfig.bgColor);
    cursorY = tft.getCursorY();
}

bool trySyncLiveInputFromRemoteLine(const String &line, bool renderUpdatedPrompt) {
    String liveInput = getTerminalLiveInput();
    if (liveInput.isEmpty()) return false;

    String promptPrefix = getCurrentPromptPrefix();
    String payload = line;
    if (!promptPrefix.isEmpty() && line.startsWith(promptPrefix)) {
        payload = line.substring(promptPrefix.length());
    } else if (!line.startsWith(liveInput)) return false;

    payload.trim();
    if (payload.isEmpty() || !payload.startsWith(liveInput)) return false;

    bool awaiting = false;
    withClientLock([&]() { awaiting = awaitingTabDebugResponse; });
    if (awaiting && payload == liveInput) return true;

    syncCommandBufferFromRemote(payload);
    withClientLock([&]() { awaitingTabDebugResponse = false; });
    if (renderUpdatedPrompt) redrawCurrentCommandLine();
    return true;
}

bool isLikelyCompletionSuffix(const String &text) {
    if (text.isEmpty()) return false;
    for (size_t i = 0; i < text.length(); ++i) {
        char c = text[i];
        bool allowed =
            isAlphaNumeric(c) || c == '_' || c == '-' || c == '.' || c == '/' || c == '\\' || c == '~';
        if (!allowed) return false;
    }
    return true;
}

bool tryAppendLiveInputSuffixFromRemoteLine(const String &line, bool renderUpdatedPrompt) {
    String liveInput = getTerminalLiveInput();
    if (liveInput.isEmpty() || line.isEmpty()) return false;

    String suffix = line;
    suffix.trim();
    if (suffix.isEmpty()) return false;
    if (suffix.startsWith(liveInput)) return false;
    if (!isLikelyCompletionSuffix(suffix)) return false;

    bool awaiting = false;
    withClientLock([&]() { awaiting = awaitingTabDebugResponse; });
    if (!awaiting) return false;

    size_t overlap = 0;
    size_t maxOverlap = min(liveInput.length(), suffix.length());
    for (size_t candidate = maxOverlap; candidate > 0; --candidate) {
        if (liveInput.substring(liveInput.length() - candidate) == suffix.substring(0, candidate)) {
            overlap = candidate;
            break;
        }
    }

    String completed = liveInput;
    completed += suffix.substring(overlap);
    syncCommandBufferFromRemote(completed);
    withClientLock([&]() { awaitingTabDebugResponse = false; });
    if (renderUpdatedPrompt) redrawCurrentCommandLine();
    return true;
}

void renderPrompt(bool forceNewLine) {
    tft.setTextSize(FP);
    if (commandBuffer == "> ") {
        String promptPrefix = getQueuedPromptPrefix();
        if (!promptPrefix.isEmpty()) commandBuffer = promptPrefix;
    }
    if (forceNewLine) ensureCursorOnFreshLine();
    tft.setTextColor(TFT_GREEN, fluxConfig.bgColor);
    tft.print(commandBuffer);
    tft.setTextColor(fluxConfig.priColor, fluxConfig.bgColor);
    cursorY = tft.getCursorY();
}

bool parseShellPromptLine(const String &line, String &context, String &prompt) {
    String trimmed = line;
    trimmed.trim();
    if (trimmed.isEmpty()) return false;

    char last = trimmed[trimmed.length() - 1];
    if (last != '#' && last != '$' && last != '>') return false;

    int atPos = trimmed.indexOf('@');
    int colonPos = trimmed.lastIndexOf(':');
    if (atPos <= 0 || colonPos <= atPos || colonPos >= trimmed.length() - 1) return false;

    String userHost = trimmed.substring(0, colonPos);
    String location = trimmed.substring(colonPos + 1);
    location.trim();
    if (location.isEmpty()) return false;

    context = "usr: " + userHost + " on: " + location;
    prompt = location + " > ";
    return true;
}

void captureShellPromptLine(const String &line) {
    String context;
    String prompt;
    if (!parseShellPromptLine(line, context, prompt)) return;

    String previousPrompt = getQueuedPromptPrefix();
    withClientLock([&]() {
        queuedPromptContext = context;
        queuedPromptPrefix = prompt;
    });

    String liveInput = getTerminalLiveInput();
    if (liveInput.isEmpty()) {
        if (commandBuffer == previousPrompt || commandBuffer == "> ") commandBuffer = prompt;
    } else {
        commandBuffer = prompt + liveInput;
    }
}

int getTerminalCols() {
    int usableWidth = tftWidth - (TERMINAL_PAD_X * 2);
    int colWidth = max(1, FP * LW);
    return max(20, usableWidth / colWidth);
}

int getTerminalRows() {
#ifdef FLUX_DELTA_BUILD
    int usableHeight = tftHeight - TERMINAL_PAD_Y - 3;
#else
    int usableHeight = tftHeight - (TERMINAL_PAD_Y * 2);
    if (!getQueuedPromptContext().isEmpty()) usableHeight -= FP * LH;
#endif
    int rowHeight = max(1, FP * LH);
    return max(4, usableHeight / rowHeight);
}

void renderVisibleText(const String &title, const String &text, bool appendNewline) {
    ensureCursorOnFreshLine();
    tft.setTextColor(TFT_WHITE, fluxConfig.bgColor);
    for (size_t i = 0; i < text.length(); ++i) {
        if (tft.getCursorX() > tftWidth - TERMINAL_PAD_X - (FP * LW)) {
            tft.println();
            tft.setCursor(TERMINAL_PAD_X, tft.getCursorY());
        }
        tft.write(text[i]);
        if (tft.getCursorY() > tftHeight - 3 - (FP * LH)) {
            resetClientScreen(title.c_str());
            renderPrompt();
            tft.setTextColor(TFT_WHITE, fluxConfig.bgColor);
        }
        if (tft.getCursorX() == 0) tft.setCursor(TERMINAL_PAD_X, tft.getCursorY());
    }

    if (appendNewline) {
        tft.println();
        if (tft.getCursorX() == 0) tft.setCursor(TERMINAL_PAD_X, tft.getCursorY());
    }
}

void renderSessionOutput(const String &title, const String &output) {
    if (output.isEmpty()) return;

    for (size_t i = 0; i < output.length(); ++i) {
        char c = output[i];
        if (c == '\r') continue;
        if (c == '\n') {
            String context;
            String prompt;
            if (parseShellPromptLine(pendingTerminalLine, context, prompt)) {
                captureShellPromptLine(pendingTerminalLine);
                renderPrompt(true);
            } else if (!pendingTerminalLine.isEmpty()) {
                if (trySyncLiveInputFromRemoteLine(pendingTerminalLine, true)) {
                    pendingTerminalLine = "";
                    continue;
                }
                if (tryAppendLiveInputSuffixFromRemoteLine(pendingTerminalLine, true)) {
                    pendingTerminalLine = "";
                    continue;
                }
                String liveInput = getTerminalLiveInput();
                if (!liveInput.isEmpty() && pendingTerminalLine.endsWith(liveInput)) {
                    syncCommandBufferFromRemote(liveInput);
                    pendingTerminalLine = "";
                    continue;
                }
                String normalizedLine = pendingTerminalLine;
                if (shouldSuppressEchoLine(normalizedLine)) {
                    pendingTerminalLine = "";
                    continue;
                }
                normalizedLine = stripLeadingPromptPrefix(normalizedLine);
                if (!liveInput.isEmpty() && normalizedLine.startsWith(liveInput)) {
                    syncCommandBufferFromRemote(normalizedLine);
                    renderVisibleText(title, getCurrentPromptPrefix() + normalizedLine, true);
                    pendingTerminalLine = "";
                    continue;
                }
                if (!normalizedLine.isEmpty()) renderVisibleText(title, normalizedLine, true);
            }
            pendingTerminalLine = "";
        } else {
            pendingTerminalLine += c;
        }
    }

    String context;
    String prompt;
    if (parseShellPromptLine(pendingTerminalLine, context, prompt)) {
        captureShellPromptLine(pendingTerminalLine);
        pendingTerminalLine = "";
        renderPrompt(true);
    } else if (!pendingTerminalLine.isEmpty()) {
        if (trySyncLiveInputFromRemoteLine(pendingTerminalLine, true) ||
            tryAppendLiveInputSuffixFromRemoteLine(pendingTerminalLine, true)) {
            pendingTerminalLine = "";
        }
    }

    cursorY = tft.getCursorY();
    tft.setTextColor(fluxConfig.priColor, fluxConfig.bgColor);
}

void finishSessionUi(const String &title) {
    requestSessionStop();

    uint32_t started = millis();
    while (getClientTaskHandle() != nullptr && millis() - started < 3000) { vTaskDelay(pdMS_TO_TICKS(20)); }
    if (getClientTaskHandle() != nullptr) {
        // The worker retains its session and secret until its current library call returns.
        displayWarning("Stopping... waiting for terminal worker", false);
        // Keep the session's stopping screen until the worker has released its resources.
        while (getClientTaskHandle() != nullptr) { vTaskDelay(pdMS_TO_TICKS(20)); }
    }

    String status = getSessionStatus();
    if (status.isEmpty()) { status = (title == "SSH") ? "SSH session closed." : "Telnet session closed."; }

    appendSessionLog("\n[STATUS] " + status + "\n");
    withClientLock([&]() { closeSessionLogUnlocked(); });

    if (didSessionFail()) displayError(status, true);
    else displayWarning(status, true);

    returnToMenu = true;
}

template <typename Fn> bool sshStep(Fn &&call, int success, int again, const char *stage, uint32_t timeout = 15000) {
    uint32_t started = millis();
    while (true) {
        if (isStopRequested()) { markSessionClosed("SSH cancelled.", false); return false; }
        if (millis() - started >= timeout) { markSessionClosed(String(stage) + " timed out.", true); return false; }
        int rc = call();
        if (rc == success) return true;
        if (rc != again) { markSessionClosed(String(stage) + " failed.", true); return false; }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

bool sshFlushOutbound(ssh_channel channel, String &pending, uint32_t started) {
    if (pending.isEmpty()) return true;
    if (isStopRequested()) { markSessionClosed("SSH cancelled.", false); return false; }
    if (millis() - started >= 15000) { markSessionClosed("SSH write timed out.", true); return false; }
    int written = ssh_channel_write(channel, pending.c_str(), pending.length());
    if (written == SSH_AGAIN || written == 0) return true;
    if (written < 0 || static_cast<size_t>(written) > pending.length()) {
        markSessionClosed("SSH write error.", true); return false;
    }
    pending.remove(0, static_cast<unsigned>(written));
    return true;
}

void closeSshTransport(ssh_session &session, int &fd) {
    if (session) {
        // libssh 0.11: disconnect detaches externally supplied FD; free alone can close it.
        ssh_disconnect(session);
        ssh_free(session);
        session = nullptr;
    }
    if (fd >= 0) { close(fd); fd = -1; }
}

struct SshDnsRequest {
    String host;
    ip_addr_t address = {};
    std::atomic<bool> ready{false};
    bool ok = false;
};
void sshDnsResult(const char *, const ip_addr_t *address, void *context) {
    auto *owner = static_cast<std::shared_ptr<SshDnsRequest> *>(context);
    if (address) { (*owner)->address = *address; (*owner)->ok = true; }
    (*owner)->ready.store(true);
    delete owner;
}
void sshDnsStart(void *context) {
    auto *owner = static_cast<std::shared_ptr<SshDnsRequest> *>(context);
    ip_addr_t address;
    err_t rc = dns_gethostbyname_addrtype((*owner)->host.c_str(), &address, sshDnsResult, owner, LWIP_DNS_ADDRTYPE_IPV4);
    if (rc != ERR_INPROGRESS) sshDnsResult(nullptr, rc == ERR_OK ? &address : nullptr, owner);
}
int sshConnectSocket() {
    auto request = std::make_shared<SshDnsRequest>();
    request->host = sessionHost;
    auto *owner = new std::shared_ptr<SshDnsRequest>(request);
    if (tcpip_try_callback(sshDnsStart, owner) != ERR_OK) {
        delete owner; markSessionClosed("SSH DNS unavailable.", true); return -1;
    }
    if (!sshStep([&]() { return request->ready.load() ? (request->ok ? SSH_OK : SSH_ERROR) : SSH_AGAIN; },
                 SSH_OK, SSH_AGAIN, "SSH DNS", 5000)) return -1;
    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) { markSessionClosed("SSH socket failed.", true); return -1; }
    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_port = htons(sessionPort);
    address.sin_addr.s_addr = ip4_addr_get_u32(ip_2_ip4(&request->address));
    if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) { close(fd); markSessionClosed("SSH socket setup failed.", true); return -1; }
    int rc = connect(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address));
    if (rc < 0 && errno != EINPROGRESS) { close(fd); markSessionClosed("SSH server unavailable.", true); return -1; }
    if (rc < 0 && !sshStep([&]() {
        fd_set writeSet; FD_ZERO(&writeSet); FD_SET(fd, &writeSet);
        timeval timeout = {};
        int ready = select(fd + 1, nullptr, &writeSet, nullptr, &timeout);
        if (!ready) return SSH_AGAIN;
        int error = 0; socklen_t size = sizeof(error);
        return ready > 0 && getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &size) == 0 && !error ? SSH_OK : SSH_ERROR;
    }, SSH_OK, SSH_AGAIN, "SSH TCP connect")) { close(fd); return -1; }
    return fd;
}

bool verifySshServer(ssh_session session) {
    int known = ssh_session_is_known_server(session);
    if (known == SSH_KNOWN_HOSTS_OK) return true;
    if (known != SSH_KNOWN_HOSTS_UNKNOWN && known != SSH_KNOWN_HOSTS_NOT_FOUND) {
        markSessionClosed("SSH host key changed or verification failed.", true);
        return false;
    }
    ssh_key key = nullptr;
    unsigned char *hash = nullptr;
    size_t length = 0;
    char *fingerprint = nullptr;
    if (ssh_get_server_publickey(session, &key) == SSH_OK &&
        ssh_get_publickey_hash(key, SSH_PUBLICKEY_HASH_SHA256, &hash, &length) == SSH_OK)
        fingerprint = ssh_get_fingerprint_hash(SSH_PUBLICKEY_HASH_SHA256, hash, length);
    if (key) ssh_key_free(key);
    if (hash) ssh_clean_pubkey_hash(&hash);
    if (!fingerprint) { markSessionClosed("Cannot verify SSH fingerprint.", true); return false; }
    withClientLock([&]() { pendingHostFingerprint = fingerprint; hostTrustDecision = 0; });
    ssh_string_free_char(fingerprint);
    uint32_t started = millis();
    int decision = 0;
    while (!isStopRequested() && millis() - started < 60000) {
        withClientLock([&]() { decision = hostTrustDecision; });
        if (decision) break;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    withClientLock([&]() { pendingHostFingerprint = ""; });
    if (decision != 1 || isStopRequested()) {
        markSessionClosed("SSH host trust cancelled or timed out.", false); return false;
    }
    if (ssh_session_update_known_hosts(session) != SSH_OK) {
        markSessionClosed("Cannot save SSH host key; login cancelled.", true); return false;
    }
    return true;
}

void sshWorkerRun(void *pvParameters) {
    std::unique_ptr<char[]> buffer(new char[CLIENT_IO_BUFFER_SIZE]);
    String password;
    withClientLock([&]() { password = ssh_password; clearSshPassword(ssh_password); });
    PasswordGuard passwordGuard{password};
    unsigned int port = sessionPort;
    long timeoutSeconds = 15;
    int strictHostKeyChecking = 1;
    int socketFd = -1;
    String pendingOutbound;
    uint32_t outboundStarted = 0;
    ssh_session sshSession = nullptr;
    ssh_channel sshChannel = nullptr;
    bool stdoutPollingEnabled = true;
    bool stderrPollingEnabled = true;
    if (!buffer) {
        markSessionClosed("SSH buffer allocation failed.", true);
        goto SSH_EXIT;
    }

    markSessionConnecting();

    sshSession = ssh_new();
    if (sshSession == nullptr) {
        markSessionClosed("SSH session creation failed.", true);
        goto SSH_EXIT;
    }

    ssh_options_set(sshSession, SSH_OPTIONS_HOST, sessionHost.c_str());
    ssh_options_set(sshSession, SSH_OPTIONS_PORT, &port);
    ssh_options_set(sshSession, SSH_OPTIONS_USER, sessionUser.c_str());
    ssh_options_set(sshSession, SSH_OPTIONS_KNOWNHOSTS, SSH_USER_KNOWN_HOSTS_PATH);
    ssh_options_set(sshSession, SSH_OPTIONS_GLOBAL_KNOWNHOSTS, SSH_GLOBAL_KNOWN_HOSTS_PATH);
    ssh_options_set(sshSession, SSH_OPTIONS_STRICTHOSTKEYCHECK, &strictHostKeyChecking);
    ssh_options_set(sshSession, SSH_OPTIONS_TIMEOUT, &timeoutSeconds);
    ssh_set_blocking(sshSession, 0);

    if (!WiFi.isConnected()) {
        markSessionClosed("WiFi disconnected before SSH connect.", true);
        goto SSH_EXIT;
    }

    Serial.printf(
        "[SSHDBG] connect start host=%s port=%u user=%s\n",
        sessionHost.c_str(),
        sessionPort,
        sessionUser.c_str()
    );
    socketFd = sshConnectSocket();
    if (socketFd < 0) goto SSH_EXIT;
    if (ssh_options_set(sshSession, SSH_OPTIONS_FD, &socketFd) != SSH_OK) {
        markSessionClosed("SSH socket option failed.", true); goto SSH_EXIT;
    }
    if (!sshStep([&]() { return ssh_connect(sshSession); }, SSH_OK, SSH_AGAIN, "SSH connect")) goto SSH_EXIT;
    Serial.printf("[SSHDBG] connect ok\n");

    if (!verifySshServer(sshSession)) goto SSH_EXIT;
    if (!sshStep([&]() { return ssh_userauth_password(sshSession, nullptr, password.c_str()); },
                 SSH_AUTH_SUCCESS, SSH_AUTH_AGAIN, "SSH authentication")) goto SSH_EXIT;
    clearSshPassword(password);
    Serial.printf("[SSHDBG] auth ok\n");

    sshChannel = ssh_channel_new(sshSession);
    if (sshChannel == nullptr) {
        markSessionClosed("SSH channel open error.", true);
        goto SSH_EXIT;
    }
    if (!sshStep([&]() { return ssh_channel_open_session(sshChannel); }, SSH_OK, SSH_AGAIN, "SSH channel")) goto SSH_EXIT;
    Serial.printf("[SSHDBG] channel open ok\n");

    if (!sshStep([&]() { return ssh_channel_request_pty_size(sshChannel, "vt100", getTerminalCols(), getTerminalRows()); },
                 SSH_OK, SSH_AGAIN, "SSH PTY")) goto SSH_EXIT;
    Serial.printf("[SSHDBG] pty ok cols=%d rows=%d\n", getTerminalCols(), getTerminalRows());

    if (!sshStep([&]() { return ssh_channel_request_shell(sshChannel); }, SSH_OK, SSH_AGAIN, "SSH shell")) goto SSH_EXIT;
    Serial.printf("[SSHDBG] shell ok\n");

    markSessionReady();

    while (!isStopRequested()) {
        if (!WiFi.isConnected()) {
            markSessionClosed("WiFi disconnected during SSH session.", true);
            goto SSH_EXIT;
        }

        if (pendingOutbound.isEmpty()) {
            pendingOutbound = takeQueuedCommand();
            outboundStarted = millis();
        }
        if (sshChannel == nullptr || sshSession == nullptr) {
            Serial.printf("[SSHDBG] session invalid. channel=%p session=%p\n", sshChannel, sshSession);
            markSessionClosed("SSH session closed.", false);
            goto SSH_EXIT;
        }

        if (!ssh_channel_is_open(sshChannel) || ssh_channel_is_closed(sshChannel) ||
            ssh_channel_is_eof(sshChannel)) {
            Serial.printf(
                "[SSHDBG] channel not writable/open. open=%d closed=%d eof=%d\n",
                ssh_channel_is_open(sshChannel),
                ssh_channel_is_closed(sshChannel),
                ssh_channel_is_eof(sshChannel)
            );
            markSessionClosed("SSH channel closed.", false);
            goto SSH_EXIT;
        }

        if (!sshFlushOutbound(sshChannel, pendingOutbound, outboundStarted)) goto SSH_EXIT;

        if (isQueuedOutputBackedUp()) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        auto drainSshStream = [&](int isStderr) -> bool {
            if (isStderr == 0 && !stdoutPollingEnabled) return true;
            if (isStderr != 0 && !stderrPollingEnabled) return true;

            int available = ssh_channel_poll(sshChannel, isStderr);
            if (available == SSH_ERROR) {
                const char *sshError = ssh_get_error(sshSession);
                bool channelEnded = ssh_channel_is_eof(sshChannel) || ssh_channel_is_closed(sshChannel);
                if (!channelEnded && (sshError == nullptr || sshError[0] == '\0')) {
                    if (isStderr == 0) {
                        Serial.printf(
                            "[SSHDBG] disabling stdout poll after rc=%d with empty error\n", available
                        );
                        stdoutPollingEnabled = false;
                    } else {
                        Serial.printf(
                            "[SSHDBG] disabling stderr poll after rc=%d with empty error\n", available
                        );
                        stderrPollingEnabled = false;
                    }
                    return true;
                }
                Serial.printf(
                    "[SSHDBG] poll rc=%d stderr=%d err='%s'\n", available, isStderr, sshError ? sshError : ""
                );
                markSessionClosed("SSH poll error.", true);
                return false;
            }

            if (available <= 0) return true;

            int maxReadSize = static_cast<int>(CLIENT_IO_BUFFER_SIZE - 1);
            int readSize = (available < maxReadSize) ? available : maxReadSize;
            int nbytes = ssh_channel_read(sshChannel, buffer.get(), readSize, isStderr);
            Serial.printf("[SSHDBG] read rc=%d stderr=%d\n", nbytes, isStderr);
            if (nbytes > 0) {
                buffer[nbytes] = '\0';
                Serial.printf(
                    "[SSHDBG] append begin len=%d queued=%u freeHeap=%u\n",
                    nbytes,
                    static_cast<unsigned>(getQueuedOutputLength()),
                    static_cast<unsigned>(ESP.getFreeHeap())
                );
                appendSessionOutput(buffer.get(), nbytes);
                Serial.printf(
                    "[SSHDBG] append end queued=%u freeHeap=%u\n",
                    static_cast<unsigned>(getQueuedOutputLength()),
                    static_cast<unsigned>(ESP.getFreeHeap())
                );
                return true;
            }

            if (nbytes == SSH_AGAIN) return true;

            if (nbytes == 0) {
                if (ssh_channel_is_eof(sshChannel) || ssh_channel_is_closed(sshChannel)) {
                    markSessionClosed("SSH session closed.", false);
                    return false;
                }
                return true;
            }

            if (ssh_channel_is_eof(sshChannel) || ssh_channel_is_closed(sshChannel)) {
                Serial.printf(
                    "[SSHDBG] read end-of-channel stderr=%d err='%s'\n", isStderr, ssh_get_error(sshSession)
                );
                markSessionClosed("SSH session closed.", false);
            } else {
                Serial.printf(
                    "[SSHDBG] read error stderr=%d err='%s'\n", isStderr, ssh_get_error(sshSession)
                );
                markSessionClosed("SSH read error.", true);
            }
            return false;
        };

        if (!drainSshStream(0)) goto SSH_EXIT;
        if (getQueuedOutputLength() > 0) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        if (!isQueuedOutputBackedUp() && !drainSshStream(1)) goto SSH_EXIT;

        if (ssh_channel_is_eof(sshChannel) && ssh_channel_poll(sshChannel, 0) <= 0 &&
            ssh_channel_poll(sshChannel, 1) <= 0) {
            markSessionClosed("SSH session closed.", false);
            goto SSH_EXIT;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    markSessionClosed("SSH session closed.", false);

SSH_EXIT:
    if (sshChannel != nullptr) {
        ssh_channel_close(sshChannel);
        ssh_channel_free(sshChannel);
        sshChannel = nullptr;
    }
    closeSshTransport(sshSession, socketFd);

    clearSshPassword(password);
}

void sshWorkerTask(void *pvParameters) {
    sshWorkerRun(pvParameters); // Unwind password and buffer owners before deleting the FreeRTOS task.
    setClientTaskHandle(nullptr);
    vTaskDelete(nullptr);
}

void telnetWorkerTask(void *pvParameters) {
    std::unique_ptr<char[]> buffer(new char[CLIENT_IO_BUFFER_SIZE]);
    struct sockaddr_in dest_addr = {};
    struct timeval timeout = {.tv_sec = 0, .tv_usec = 100000};

    if (!buffer) {
        markSessionClosed("Telnet buffer allocation failed.", true);
        goto TELNET_EXIT;
    }

    markSessionConnecting();

    dest_addr.sin_addr.s_addr = inet_addr(sessionHost.c_str());
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(sessionPort);

    telnetSock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (telnetSock < 0) {
        markSessionClosed("Unable to create socket.", true);
        goto TELNET_EXIT;
    }

    setsockopt(telnetSock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    if (connect(telnetSock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) != 0) {
        markSessionClosed("Socket connection failed.", true);
        goto TELNET_EXIT;
    }

    markSessionReady();

    while (!isStopRequested()) {
        if (!WiFi.isConnected()) {
            markSessionClosed("WiFi disconnected during Telnet session.", true);
            goto TELNET_EXIT;
        }

        String outbound = takeQueuedCommand();
        if (!outbound.isEmpty()) { send(telnetSock, outbound.c_str(), outbound.length(), 0); }

        int len = recv(telnetSock, buffer.get(), CLIENT_IO_BUFFER_SIZE - 1, 0);
        if (len > 0) {
            buffer[len] = '\0';

            if ((uint8_t)buffer[0] == 0xFF) {
                // Skip inline telnet negotiation sequences for the simple client UI.
            } else {
                appendSessionOutput(buffer.get(), len);
            }
        } else if (len == 0) {
            markSessionClosed("Telnet session closed.", false);
            goto TELNET_EXIT;
        } else if (errno != EWOULDBLOCK && errno != EAGAIN) {
            markSessionClosed("Telnet receive error.", true);
            goto TELNET_EXIT;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    markSessionClosed("Telnet session closed.", false);

TELNET_EXIT:
    if (telnetSock >= 0) {
        close(telnetSock);
        telnetSock = -1;
    }

    setClientTaskHandle(nullptr);
    vTaskDelete(nullptr);
}

void runSessionUiLoop(const String &title) {
    resetCommandBufferToPrompt();
    resetClientScreen(title.c_str());

    displayTextLine("Connecting...");
    bool trustShown = false;
    while (isSessionConnecting() && !isSessionClosed() && !returnToMenu) {
        if (check(EscPress)) {
            finishSessionUi(title);
            return;
        }
        String fingerprint;
        withClientLock([&]() { fingerprint = pendingHostFingerprint; });
        if (!fingerprint.isEmpty()) {
            if (!trustShown) {
                drawMainBorderWithTitle("Unknown SSH host");
                tft.setTextSize(1);
                tft.setCursor(6, 35);
                tft.setTextWrap(true);
                tft.println(sessionHost + ":" + String(sessionPort));
                tft.println("Verify this fingerprint:");
                tft.println(fingerprint);
                tft.println("Encoder: Trust and connect");
                tft.println("Back: Reject");
                SelPress = false;
                trustShown = true;
            }
            if (check(SelPress)) withClientLock([&]() { hostTrustDecision = 1; });
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    if (returnToMenu || isSessionClosed()) {
        finishSessionUi(title);
        return;
    }

    resetClientScreen(title.c_str());
    renderPrompt();

    while (!returnToMenu && !isSessionClosed()) {
        String output = takeQueuedOutput();
        if (!output.isEmpty()) { renderSessionOutput(title, output); }
        bool isDelPressed = false;

#ifdef HAS_KEYBOARD
        keyStroke key = _getKeyPress();
        isDelPressed = key.del;
        if (key.pressed) {
            unsigned long currentMillis = millis();
            if (currentMillis - lastKeyPressMillis >= debounceDelay) {
                lastKeyPressMillis = currentMillis;

                bool sendTab = false;
                for (auto c : key.word) {
                    uint8_t raw = static_cast<uint8_t>(c);
                    if (raw == KEY_TAB) {
                        sendTab = true;
                        continue;
                    }
                    commandBuffer += c;
                    String liveInput = getTerminalLiveInput();
                    if (!liveInput.isEmpty()) {
                        liveInput += c;
                        setTerminalLiveInput(liveInput);
                        queueSessionCommand(String(c));
                    }
                    tft.setTextColor(TFT_GREEN, fluxConfig.bgColor);
                    tft.print(c);
                }

                if (sendTab) {
                    String pendingInput = getBufferedCommandInput();
                    String liveInput = getTerminalLiveInput();
                    bool remoteLineAlreadyActive = !liveInput.isEmpty();
                    if (!remoteLineAlreadyActive) setTerminalLiveInput(pendingInput);
                    withClientLock([&]() { awaitingTabDebugResponse = true; });
                    if (!remoteLineAlreadyActive && !pendingInput.isEmpty())
                        queueSessionCommand(pendingInput);
                    queueSessionCommand(String('\t'));
                }

                if (key.del) EscPress = false;
                if (key.del) {
                    size_t promptLength = getCommandPromptLength();
                    String liveInput = getTerminalLiveInput();
                    bool remoteLineAlreadyActive = !liveInput.isEmpty();
                    if (commandBuffer.length() > promptLength) {
                        commandBuffer.remove(commandBuffer.length() - 1);
                        if (remoteLineAlreadyActive) {
                            setTerminalLiveInput(getBufferedCommandInput());
                            queueSessionCommand(String(char(0x7f)));
                        }
                        tft.setCursor(tft.getCursorX() - 6, tft.getCursorY());
                        tft.print(" ");
                        tft.setCursor(tft.getCursorX() - 6, tft.getCursorY());
                    } else {
                        if (remoteLineAlreadyActive) {
                            setTerminalLiveInput("");
                            queueSessionCommand(String(char(0x7f)));
                        }
                    }
                } else if (key.enter) {
                    String input = getBufferedCommandInput();
                    String liveInput = getTerminalLiveInput();
                    String loggedCommand = !liveInput.isEmpty() ? liveInput : input;
                    appendSessionCommandToLog(loggedCommand);
                    if (input == "cls" || input == "clear") {
                        clearTerminalPendingState();
                        resetClientScreen(title.c_str());
                    } else if (!liveInput.isEmpty()) {
                        setPendingEchoSuppression(liveInput);
                        queueSessionCommand("\r");
                    } else {
                        setPendingEchoSuppression(input);
                        queueSessionCommand(input + "\r");
                    }
                    setTerminalLiveInput("");
                    resetCommandBufferToPrompt();
                    ensureCursorOnFreshLine();
                }
            }
        }
#else
        if (check(SelPress)) {
            String message = keyboard("", 76, title + " Command:");

            if (message == "cls" || message == "clear") {
                appendSessionCommandToLog(message);
                resetClientScreen(title.c_str());
            } else {
                tft.setTextSize(FP);
                appendSessionCommandToLog(message);
                queueSessionCommand(message + "\r");
                tft.setTextColor(TFT_GREEN, fluxConfig.bgColor);
                tft.println("> " + message);
                tft.setTextColor(fluxConfig.priColor, fluxConfig.bgColor);
            }
            resetCommandBufferToPrompt();
            renderPrompt();
            SelPress = false; // Resets button state to avoid repeated prompts
        }
#endif

        if (check(EscPress) && isDelPressed == false) {
            if (isSessionConnecting()) { displayWarning("Closing session...", false); }
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    finishSessionUi(title);
}
} // namespace

char *stringTochar(const String &s) {
    static char arr[128];
    memset(arr, 0, sizeof(arr));
    s.toCharArray(arr, sizeof(arr));
    return arr;
}

void ssh_setup(String host) {
    if (!initClientMutex()) {
        displayError("SSH mutex creation failed.", true);
        returnToMenu = true;
        return;
    }
    if (getClientTaskHandle()) { displayWarning("Previous terminal is still stopping", true); return; }

    resetClientScreen("SSH");
    if (host != "") {
        ssh_host = host;
    } else {
        ssh_host = keyboard("", 100, "SSH HOST (IP or Hostname)");
        if (ssh_host == "\x1B") return;
    }

    ssh_port = num_keyboard("22", 5, "SSH PORT");
    if (ssh_port == "\x1B") return;
    ssh_user = keyboard("", 64, "SSH USER");
    if (ssh_user == "\x1B") return;
    uint16_t port;
    if (!sshValidPort(ssh_port.c_str(), port)) { displayError("SSH port must be 1-65535", true); return; }
    ssh_connect_profile(ssh_host, port, ssh_user);
}

void ssh_connect_profile(String host, uint16_t port, String user) {
    if (!initClientMutex()) { displayError("SSH mutex creation failed", true); return; }
    if (getClientTaskHandle()) { displayWarning("Previous terminal is still stopping", true); return; }
    clearSshPassword(ssh_password);
    PasswordGuard guard{ssh_password};
    host.trim(); user.trim();
    if (!port || !sshValidEndpoint(host.c_str(), user.c_str())) { displayError("SSH host/user required; invalid endpoint", true); return; }
    if (!WiFi.isConnected() && !wifiConnectMenu()) return;
    if (!WiFi.isConnected()) return;
    ssh_password = keyboard("", 76, "SSH PASSWORD", true);
    if (ssh_password == "\x1B") return;
    sessionHost = host; // Keep hostname identity for known_hosts, even when DNS returns a different IP.
    sessionUser = user;
    sessionPort = port;
    resetClientState(ClientProtocol::SSH);
    TaskHandle_t workerHandle = nullptr;
    markSessionConnecting();
    // Creation and publication share the lock with the worker's initial credential transfer.
    withClientLock([&]() {
        xTaskCreate(sshWorkerTask, "SSH Task", SSH_TASK_STACK_SIZE, nullptr, 1, &workerHandle);
        clientTaskHandle = workerHandle;
    });

    if (workerHandle == nullptr) {
        displayError("SSH Task creation failed.", true);
        returnToMenu = true;
        return;
    }

    startSessionLog(ClientProtocol::SSH);
    runSessionUiLoop("SSH");
}

void ssh_loop(void *pvParameters) { sshWorkerTask(pvParameters); }

void telnet_loop() { telnetWorkerTask(nullptr); }

void telnet_setup() {
    if (!wifiConnected) wifiConnectMenu();
    if (!initClientMutex()) {
        displayError("Telnet mutex creation failed.", true);
        returnToMenu = true;
        return;
    }
    if (getClientTaskHandle()) { displayWarning("Previous terminal is still stopping", true); return; }

    resetClientScreen("TELNET");
    Serial.begin(115200);
    Serial.println("Starting Telnet Setup");

    telnet_server_string = keyboard("", 76, "TELNET_SERVER");
    if (telnet_server_string == "\x1B") return;
    telnet_port_string = num_keyboard("23", 5, "TELNET PORT");
    if (telnet_port_string == "\x1B") return;
    telnet_server_port = telnet_port_string.toInt();

    IPAddress resolvedIp;
    if (WiFi.hostByName(telnet_server_string.c_str(), resolvedIp)) {
        sessionHost = resolvedIp.toString();
    } else {
        displayError("Failed to resolve hostname.", true);
        returnToMenu = true;
        return;
    }

    sessionPort = static_cast<uint16_t>(telnet_server_port);
    resetClientState(ClientProtocol::Telnet);
    TaskHandle_t workerHandle = nullptr;
    markSessionConnecting();
    withClientLock([&]() {
        xTaskCreate(telnetWorkerTask, "Telnet Task", 4096, nullptr, 1, &workerHandle);
        clientTaskHandle = workerHandle;
    });

    if (workerHandle == nullptr) {
        displayError("Telnet Task creation failed.", true);
        returnToMenu = true;
        return;
    }

    startSessionLog(ClientProtocol::Telnet);
    runSessionUiLoop("TELNET");
}
#endif
