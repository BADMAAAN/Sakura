#ifndef FLUX_BLE_SERVICE_H
#define FLUX_BLE_SERVICE_H
#if !defined(LITE_VERSION)
#include <NimBLEServer.h>

class FluxBLEService {
protected:
    NimBLEService *pService = nullptr;
    uint16_t mtu = 23; // default MTU size
public:
    virtual ~FluxBLEService() = default;
    virtual void setup(NimBLEServer *pServer) = 0;
    virtual void end() = 0;
    void setMTU(int new_mtu) { mtu = new_mtu; }
};
#endif
#endif // FLUX_BLE_SERVICE_H
