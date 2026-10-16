#include <globals.h>
#include <JPEGDecoder.h>
#include "flux_branding.h"
#ifdef FLUX_EMBEDDED_BRANDING
#include "flux_branding_data.h"

static bool drawAsset(const FluxImageAsset &asset) {
    if (!asset.pixels || asset.data_size == 0 || asset.width == 0 || asset.height == 0) return false;
    if (asset.pixel_count != static_cast<uint32_t>(asset.width) * asset.height) return false;
    if (asset.width > tft.width() || asset.height > tft.height()) return false;
    if (!JpegDec.decodeArray(asset.pixels, asset.data_size)) return false;
    if (JpegDec.width != asset.width || JpegDec.height != asset.height) {
        JpegDec.abort();
        return false;
    }

    const int x = (tft.width() - asset.width) / 2;
    const int y = (tft.height() - asset.height) / 2;

    // Decode every MCU into a complete off-screen frame. The panel sees one
    // finished image, never the decoder's individual 16x16 blocks.
    TFT_eSprite frame(tft.native());
    frame.setColorDepth(16);
    if (!frame.createSprite(asset.width, asset.height)) {
        JpegDec.abort();
        return false;
    }
    frame.fillSprite(TFT_BLACK);
    frame.setSwapBytes(true);

    const uint16_t mcuW = JpegDec.MCUWidth;
    const uint16_t mcuH = JpegDec.MCUHeight;
    bool complete = true;
    while (JpegDec.read()) {
        const uint16_t px = JpegDec.MCUx * mcuW;
        const uint16_t py = JpegDec.MCUy * mcuH;
        if (px >= asset.width || py >= asset.height) {
            complete = false;
            JpegDec.abort();
            break;
        }
        const uint16_t blockW = (px + mcuW <= asset.width) ? mcuW : asset.width - px;
        const uint16_t blockH = (py + mcuH <= asset.height) ? mcuH : asset.height - py;
        frame.pushImage(px, py, blockW, blockH, JpegDec.pImage);
    }

    if (complete) {
        tft.fillScreen(TFT_BLACK);
        frame.pushSprite(x, y);
    }
    frame.deleteSprite();
    if (!complete) return false;
    return true;
}

bool drawFluxEmbeddedBranding(const char *filename) {
    (void)filename;
    if (FLUX_PRIVATE_IMAGE_COUNT == 0) return false;
    return drawAsset(flux_private_screen[0]);
}

uint8_t fluxPrivateImageCount() { return FLUX_PRIVATE_IMAGE_COUNT; }

bool drawFluxPrivateBranding(uint8_t index) {
    if (index >= FLUX_PRIVATE_IMAGE_COUNT) return false;
    return drawAsset(flux_private_screen[index]);
}
#else
uint8_t fluxPrivateImageCount() { return 0; }
bool drawFluxPrivateBranding(uint8_t) { return false; }
#endif
