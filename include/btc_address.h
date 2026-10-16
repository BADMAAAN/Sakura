#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace sakura {
using Hash256 = bool (*)(const uint8_t *, size_t, uint8_t *);

// Mainnet P2PKH/P2SH Base58Check. Other address encodings fail closed.
inline bool validBitcoinAddress(const char *address, Hash256 sha256) {
    if (!address || !sha256) return false;
    const size_t length = std::strlen(address);
    if (length < 26 || length > 35) return false;
    const char *alphabet = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
    uint8_t decoded[25] = {};
    for (size_t i = 0; i < length; ++i) {
        const char *digit = std::strchr(alphabet, address[i]);
        if (!digit) return false;
        unsigned carry = unsigned(digit - alphabet);
        for (int j = 24; j >= 0; --j) {
            carry += unsigned(decoded[j]) * 58;
            decoded[j] = uint8_t(carry);
            carry >>= 8;
        }
        if (carry) return false;
    }
    size_t leadingOnes = 0, leadingZeros = 0;
    while (leadingOnes < length && address[leadingOnes] == '1') ++leadingOnes;
    while (leadingZeros < sizeof(decoded) && decoded[leadingZeros] == 0) ++leadingZeros;
    if (leadingOnes != leadingZeros || (decoded[0] != 0 && decoded[0] != 5)) return false;
    uint8_t first[32], second[32];
    return sha256(decoded, 21, first) && sha256(first, 32, second) &&
           std::memcmp(decoded + 21, second, 4) == 0;
}
} // namespace sakura
