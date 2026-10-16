#ifdef FLUX_DELTA_BUILD
// Firmware links one copy of the shared assets, regardless of the number of UI adapters.
#define DELTA_ARTWORK_IMPLEMENTATION
#include <delta/artwork.h>
#endif
