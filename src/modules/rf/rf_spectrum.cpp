#include "rf_spectrum.h"
#include "protocols/rf_config.h"
#include "protocols/rf_decoder.h"
#include "rf_utils.h"
#include "structs.h"
#ifdef FLUX_DELTA_BUILD
#include "core/delta_ui.h"
#include <delta/instruments.h>
#include <delta/rf_graph.h>
#endif

static uint16_t spectrumBg() {
#ifdef FLUX_DELTA_BUILD
    return delta::ui::palette(deltaUiAppearance().theme).background;
#else
    return fluxConfig.bgColor;
#endif
}
static uint16_t spectrumAccent() {
#ifdef FLUX_DELTA_BUILD
    return delta::ui::palette(deltaUiAppearance().theme).accent;
#else
    return fluxConfig.priColor;
#endif
}
static uint16_t spectrumGrid() {
#ifdef FLUX_DELTA_BUILD
    return delta::ui::palette(deltaUiAppearance().theme).line;
#else
    return TFT_DARKGREY;
#endif
}

static bool spectrum_rmt_rx_done_callback(
    rmt_channel_t *channel, const rmt_rx_done_event_data_t *edata, void *user_data
) {
    BaseType_t high_task_wakeup = pdFALSE;
    QueueHandle_t receive_queue = (QueueHandle_t)user_data;
    // send the received RMT symbols to the parser task
    xQueueSendFromISR(receive_queue, edata, &high_task_wakeup);
    return high_task_wakeup == pdTRUE;
}

void draw_tf_spectrum_grid() {
#ifdef FLUX_DELTA_BUILD
    if(uint16_t*p=deltaUiCanvas(false,false)) {
        const delta::ui::Palette colors=delta::ui::palette(deltaUiAppearance().theme);
        char title[64];snprintf(title,sizeof(title),"RF / Pulse timing   %.2f MHz",fluxConfigPins.rfFreq);
        delta::ui::box(p,0,0,320,20,colors.background);
        delta::ui::textClip(p,title,9,2,301,colors.text);
        delta::ui::box(p,9,18,302,1,colors.line);
        deltaUiPresent(0,0,320,20);
    } else {
#endif
    tft.setTextSize(1);
    tft.setCursor(3, 2);
    tft.setTextColor(spectrumAccent(), spectrumBg());
    tft.printf(" RF - Spectrum (%.2f Mhz)", fluxConfigPins.rfFreq);
#ifdef FLUX_DELTA_BUILD
    }
#endif
    tft.fillRect(0, 20, tftWidth, tftHeight - 20, spectrumBg());
    tft.drawFastHLine(0, 20 + tftHeight / 2, tftWidth, spectrumGrid());
    tft.drawFastVLine((1 * tftWidth) / 4, 20, tftHeight - 20, spectrumGrid());
    tft.drawFastVLine((2 * tftWidth) / 4, 20, tftHeight - 20, spectrumGrid());
    tft.drawFastVLine((3 * tftWidth) / 4, 20, tftHeight - 20, spectrumGrid());
}

void rf_spectrum() {
    tft.fillScreen(spectrumBg());
    draw_tf_spectrum_grid();
    if (fluxConfigPins.rfModule == M5_RF_MODULE) {
        RfRxSession rx;
        if (!rx.begin()) {
            deinitRfModule();
            return;
        }

        std::vector<int> durations;
        while (1) {
            if (rx.poll(durations)) {
                draw_tf_spectrum_grid();
                for (size_t i = 0; i < durations.size(); i++) {
                    int pulse = abs(durations[i]);
                    int lineHeight = map(pulse, 0, SIGNAL_STRENGTH_THRESHOLD, 0, tftHeight / 2);
                    int lineX =
                        map(i, 0, durations.size() > 1 ? durations.size() - 1 : 1, 0, tftWidth - 1);
                    int startY = constrain(20 + tftHeight / 2 - lineHeight / 2, 20, 20 + tftHeight);
                    int endY = constrain(20 + tftHeight / 2 + lineHeight / 2, 20, 20 + tftHeight);
                    tft.drawLine(lineX, startY, lineX, endY, spectrumAccent());
                }
                RF_DBG("m5 spectrum: durations=%u", (unsigned)durations.size());
            }

            if (check(EscPress)) { break; }
            if (setMHZMenu()) {
                rx.end();
                rx.begin();
                draw_tf_spectrum_grid();
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        rx.end();
        returnToMenu = true;
        deinitRfModule();
        return;
    }

    rmt_channel_handle_t rx_ch = NULL;
    rx_ch = setup_rf_rx();
    if (rx_ch == NULL) return;
    ESP_LOGI("RMT_SPECTRUM", "register RX done callback");
    QueueHandle_t receive_queue = xQueueCreate(1, sizeof(rmt_rx_done_event_data_t));
    assert(receive_queue);
    rmt_rx_event_callbacks_t cbs = {
        .on_recv_done = spectrum_rmt_rx_done_callback,
    };
    ESP_ERROR_CHECK(rmt_rx_register_event_callbacks(rx_ch, &cbs, receive_queue));
    ESP_ERROR_CHECK(rmt_enable(rx_ch));
    rmt_receive_config_t receive_config = {
        .signal_range_min_ns = 3000,     // 6us minimum signal duration
        .signal_range_max_ns = 12000000, // 24ms maximum signal duration
    };
    rmt_symbol_word_t item[64];
    rmt_rx_done_event_data_t rx_data;
    ESP_ERROR_CHECK(rmt_receive(rx_ch, item, sizeof(item), &receive_config));

    size_t rx_size = 0;
    while (1) {
        rmt_symbol_word_t *rx_items = NULL;
        if (xQueueReceive(receive_queue, &rx_data, 0) == pdPASS) {
            rx_size = rx_data.num_symbols;
            rx_items = rx_data.received_symbols;
        }
        if (rx_size != 0) {
            // Draw grid and info
            draw_tf_spectrum_grid();
            // Draw waveform based on signal strength
            for (size_t i = 0; i < rx_size; i++) {
                int lineHeight =
                    map(rx_items[i].duration0 + rx_items[i].duration1,
                        0,
                        SIGNAL_STRENGTH_THRESHOLD,
                        0,
                        tftHeight / 2);
                int lineX = map(i, 0, rx_size > 1 ? rx_size - 1 : 1, 0, tftWidth - 1);
                int startY = constrain(20 + tftHeight / 2 - lineHeight / 2, 20, 20 + tftHeight);
                int endY = constrain(20 + tftHeight / 2 + lineHeight / 2, 20, 20 + tftHeight);
                tft.drawLine(lineX, startY, lineX, endY, spectrumAccent());
            }

            ESP_ERROR_CHECK(rmt_receive(rx_ch, item, sizeof(item), &receive_config));
            rx_size = 0;
        }
        // Checks to leave while
        if (check(EscPress)) { break; }
        if (setMHZMenu()) yield();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    returnToMenu = true;
    rmt_disable(rx_ch);
    rmt_del_channel(rx_ch);
    vQueueDelete(receive_queue);
    deinitRfModule();
}

#define TIME_DIVIDER (tftWidth / 10)
//@Pirata
void rf_SquareWave() {
    if (!initRfModule("rx", fluxConfigPins.rfFreq)) return;

    RfRxSession rx;
    if (!rx.begin()) {
        deinitRfModule();
        return;
    }
    int line_w = 0;
    int line_h = 15;
    std::vector<int> durations;
PRINT:
    tft.drawPixel(0, 0, 0);
    tft.fillScreen(spectrumBg());
    tft.setTextSize(1);
    tft.setCursor(3, 2);
    tft.printf("  RF - SquareWave (%.2f Mhz)", fluxConfigPins.rfFreq);

    while (1) {
        if (rx.poll(durations)) {
            // Draw the captured square wave (HIGH width then LOW width per pair).
            for (size_t i = 0; i + 1 < durations.size(); i += 2) {
                int high = abs(durations[i]);
                int low = abs(durations[i + 1]);
                if (high == 0) break;

                if (high > 20000) high = 20000;
                if (low > 20000) low = 20000;
                if (line_w + (high + low) / TIME_DIVIDER > tftWidth) {
                    line_w = 10;
                    line_h += 10;
                }
                if (line_h > tftHeight) {
                    line_h = 15;
                    tft.fillRect(0, 12, tftWidth, tftHeight, spectrumBg());
                }
                tft.drawFastVLine(line_w, line_h, 6, spectrumAccent());
                tft.drawFastHLine(line_w, line_h, high / TIME_DIVIDER, spectrumAccent());

                tft.drawFastVLine(line_w + high / TIME_DIVIDER, line_h, 6, spectrumAccent());
                tft.drawFastHLine(
                    line_w + high / TIME_DIVIDER, line_h + 6, low / TIME_DIVIDER, spectrumAccent()
                );
                line_w += (high + low) / TIME_DIVIDER;
            }
        }
        // Checks to leave while
        if (check(EscPress)) { break; }
        if (setMHZMenu()) goto PRINT;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    rx.end();
    returnToMenu = true;
}

void rf_CC1101_rssi() {
#if !defined(LITE_VERSION)
    if (fluxConfigPins.rfModule != CC1101_SPI_MODULE) {
        displayError("only for CC1101 module", true);
        return;
    }
    int graph_size = tftWidth - 20;
    std::vector<int> signal(graph_size, -95);
    const size_t freq_count = sizeof(subghz_frequency_list) / sizeof(float);
    std::vector<int> bar_size(freq_count, 0);
#ifdef FLUX_DELTA_BUILD
    const bool deltaGraphActive = deltaUiCanvas(false, false) != nullptr;
    std::vector<int16_t> measuredSignal(graph_size, delta::ui::RssiUnmeasured);
    std::vector<int16_t> measuredBins(freq_count, delta::ui::RssiUnmeasured);
    auto presentRssi = [&]() {
        if (!deltaGraphActive) return;
        if (uint16_t *frame = deltaUiCanvas(true, false)) {
            if (fluxConfigPins.rfFxdFreq)
                delta::ui::rfGraph(frame, deltaUiAppearance(), measuredSignal.data(), nullptr,
                                   measuredSignal.size(), fluxConfigPins.rfFreq);
            else {
                const int first = range_limits[fluxConfigPins.rfScanRange][0];
                const int count = range_limits[fluxConfigPins.rfScanRange][1] - first + 1;
                delta::ui::rfGraph(frame, deltaUiAppearance(), measuredBins.data(),
                                   subghz_frequency_list + first, count);
            }
            deltaUiPresent();
        }
    };
#endif
    int max_bar_size = tftHeight - 20 /*bottom margin*/ - 20 /*top margin*/;
    bool redraw = true;
    const int min_value = map(-70, -95, -20, 0, max_bar_size);
    while (1) {
        if (redraw) {
            redraw = false;
            tft.drawPixel(0, 0, 0);
#ifdef FLUX_DELTA_BUILD
            if (!deltaGraphActive) {
#endif
            tft.fillScreen(spectrumBg());
            tft.setTextSize(1);
            tft.setTextColor(spectrumAccent(), spectrumBg());
            tft.setCursor(3, 2);
#ifdef FLUX_DELTA_BUILD
            }
#endif
            // Fixed frequency sees a dot running grafic, showing RSSI over time
            if (fluxConfigPins.rfFxdFreq) {
                if (!initRfModule("rx", fluxConfigPins.rfFreq))
                    displayError("Error setting frequency", true);
#ifdef FLUX_DELTA_BUILD
                if (!deltaGraphActive) {
#endif
                tft.printf(" RF - RSSI spectrum (%.2f Mhz)", fluxConfigPins.rfFreq);
                tft.drawFastVLine(20, 20, tftHeight, spectrumAccent());
                tft.drawString("-95", 0, (tftHeight - 120) + 95);
                tft.drawString("-80", 0, (tftHeight - 120) + 80);
                tft.drawString("-65", 0, (tftHeight - 120) + 65);
                tft.drawString("-50", 0, (tftHeight - 120) + 50);
                tft.drawString("-35", 0, (tftHeight - 120) + 35);
                tft.drawString("-20", 0, (tftHeight - 120) + 20);
#ifdef FLUX_DELTA_BUILD
                }
                std::fill(measuredSignal.begin(), measuredSignal.end(), delta::ui::RssiUnmeasured);
#endif
                // resets signal array
                std::fill(signal.begin(), signal.end(), -95);
            }
            // Range Scan Sees a bargraph simillar to NRF24 grafic, using RSSI across frequencies
            else {
                if (!initRfModule("rx", fluxConfigPins.rfFreq)) displayError("Error starting module", true);
#ifdef FLUX_DELTA_BUILD
                if (!deltaGraphActive) {
#endif
                tft.printf(" RF - RSSI spectrum (%s)", subghz_frequency_ranges[fluxConfigPins.rfScanRange]);
                tft.drawFastHLine(0, tftHeight - 20, tftWidth, spectrumAccent());
                char buf[7];
                float var = subghz_frequency_list[range_limits[fluxConfigPins.rfScanRange][0]];
                snprintf(buf, sizeof(buf), "%.3f", var);
                tft.drawString(buf, 2, tftHeight - 10);
                var = subghz_frequency_list[range_limits[fluxConfigPins.rfScanRange][1]];
                snprintf(buf, sizeof(buf), "%.3f", var);
                tft.drawRightString(buf, tftWidth - 2, tftHeight - 10);
                int range = range_limits[fluxConfigPins.rfScanRange][1] -
                            range_limits[fluxConfigPins.rfScanRange][0] + 1;
                int space = tftWidth / range;
                for (int i = 0; i < range; i++) {
                    tft.drawFastVLine(space * i, tftHeight - 20, 5, spectrumAccent());
                }
#ifdef FLUX_DELTA_BUILD
                }
                std::fill(measuredBins.begin(), measuredBins.end(), delta::ui::RssiUnmeasured);
#endif
                std::fill(bar_size.begin(), bar_size.end(), 0);
#ifdef FLUX_DELTA_BUILD
                std::fill(measuredBins.begin(), measuredBins.end(), delta::ui::RssiUnmeasured);
#endif
            }
#ifdef FLUX_DELTA_BUILD
            presentRssi();
#endif
        }

        // draw dot graph for fixed frequency
        if (fluxConfigPins.rfFxdFreq) {
            int rssi = ELECHOUSE_cc1101.getRssi();
            tft.drawPixel(0, 0, 0); // To make sure CC1101 shared with TFT works properly
            const int base_y = tftHeight - 120;
            int prev = signal[0];
            for (int i = 1; i < graph_size; i++) {
                if (EscPress || SelPress) break;
                const int x0 = 20 + (i - 1);
                const int x1 = 20 + i;
                const int curr = signal[i];
                // erase old segment between previous and current points
#ifdef FLUX_DELTA_BUILD
                if (!deltaGraphActive)
#endif
                    tft.drawLine(x0, base_y - prev, x1, base_y - curr, spectrumBg());
                const int next_val = (i == graph_size - 1) ? rssi : signal[i + 1];
                // shift buffer left by one
                signal[i - 1] = curr;
                if (i == graph_size - 1) signal[i] = rssi;
                // draw updated segment using new values
#ifdef FLUX_DELTA_BUILD
                if (!deltaGraphActive)
#endif
                    tft.drawLine(x0, base_y - curr, x1, base_y - next_val, spectrumAccent());
                prev = curr;
            }
#ifdef FLUX_DELTA_BUILD
            if (deltaGraphActive) {
                for (size_t i = 1; i < measuredSignal.size(); ++i) measuredSignal[i - 1] = measuredSignal[i];
                if (!measuredSignal.empty()) measuredSignal.back() = int16_t(rssi);
                presentRssi();
            } else
#endif
                tft.drawFastVLine(20, 20, tftHeight, spectrumAccent());
            vTaskDelay(pdMS_TO_TICKS(75));
        }
        // draw a bargraph similar to nrf24 across the range
        else {
            int range = range_limits[fluxConfigPins.rfScanRange][1] -
                        range_limits[fluxConfigPins.rfScanRange][0] + 1;

            int space = tftWidth / range;
            int max_idx = 0;
            for (int i = 0; i < range; i++) {
                if (EscPress || SelPress) break;
                setMHZ(subghz_frequency_list[range_limits[fluxConfigPins.rfScanRange][0] + i]);
                vTaskDelay(pdMS_TO_TICKS(5));
                int rssi = ELECHOUSE_cc1101.getRssi();
                tft.drawPixel(0, 0, 0); // To make sure CC1101 shared with TFT works properly
#ifdef FLUX_DELTA_BUILD
                measuredBins[i] = int16_t(rssi);
#endif
                int size = map(rssi, -95, -20, 0, max_bar_size);
                if (size > bar_size[i]) bar_size[i] = size;
                else bar_size[i] = bar_size[i] - (bar_size[i] - size) / 2; // slow down decrease
#ifdef FLUX_DELTA_BUILD
                if (!deltaGraphActive) {
#endif
                tft.fillRect(
                    i * space, tftHeight - 20 - bar_size[i], space - 2, bar_size[i], spectrumAccent()
                );
                tft.fillRect(i * space, 20, space, max_bar_size - bar_size[i], spectrumBg());
#ifdef FLUX_DELTA_BUILD
                }
#endif
                if (bar_size[i] > bar_size[max_idx] && bar_size[i] > min_value) max_idx = i;
            }
#ifdef FLUX_DELTA_BUILD
            if (deltaGraphActive) presentRssi();
            else
#endif
            if (bar_size[max_idx] > min_value) {
                char buf[7];
                float var = subghz_frequency_list[range_limits[fluxConfigPins.rfScanRange][0] + max_idx];
                snprintf(buf, sizeof(buf), "%.2f", var);
                tft.drawCentreString("Max=      ", tftWidth / 2, tftHeight - 10);
                tft.drawCentreString("Max=" + String(buf), tftWidth / 2, tftHeight - 10);
            }
        }
        if (check(EscPress)) { break; }
        if (check(SelPress)) {
            deinitRfModule();
            rf_range_selection(fluxConfigPins.rfFreq);
            redraw = true;
        }
    }
    deinitRfModule();
#else
    displayError("Not available on Launcher version");
#endif
}
