#pragma once

#if defined(SAKURA_BUILD)
    #define FIRMWARE_NAME "SAKURA"
    #define FIRMWARE_VERSION FLUX_VERSION
#elif defined(FLUX_BUILD)
    #define FIRMWARE_NAME "FLUX"
    #define FIRMWARE_VERSION FLUX_VERSION
#else
    #define FIRMWARE_NAME "FLUX"
    #define FIRMWARE_VERSION FLUX_VERSION
#endif

// Compatibility for source files that still use the original Bruce symbol.
#ifndef BRUCE_VERSION
    #define BRUCE_VERSION FLUX_VERSION
#endif
