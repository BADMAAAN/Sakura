#pragma once

// All roles resolve to compiled artwork for the physical FLUX target.
bool drawFluxEmbeddedBranding(const char *filename);
uint8_t fluxPrivateImageCount();
bool drawFluxPrivateBranding(uint8_t index);
