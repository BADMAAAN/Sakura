#pragma once
#include <stdint.h>
#include <stddef.h>

namespace delta {
struct QuickAction {
    uint8_t kind = 1;
    uint32_t id = 0;
    QuickAction(uint8_t k = 1, uint32_t i = 0) : kind(k), id(i) {}
    bool operator==(const QuickAction &other) const { return kind == other.kind && id == other.id; }
};
struct QuickAccess {
    static const unsigned Capacity = 24, RecentCapacity = 5;
    QuickAction pins[Capacity], recent[RecentCapacity];
    uint8_t count = 0, recentCount = 0;
    bool history = true;
    int find(QuickAction action) const {
        for (unsigned i = 0; i < count; ++i) if (pins[i] == action) return int(i);
        return -1;
    }
    bool pin(QuickAction action, bool value) {
        const int position = find(action);
        if (value) {
            if (position >= 0) return true;
            if (count == Capacity) return false;
            pins[count++] = action;
        } else if (position >= 0) {
            for (unsigned i = unsigned(position); i + 1 < count; ++i) pins[i] = pins[i + 1];
            --count;
        }
        return true;
    }
    void record(QuickAction action) {
        if (!history || (action.kind != 1 && action.kind != 2)) return;
        unsigned position = 0;
        while (position < recentCount && !(recent[position] == action)) ++position;
        if (position == RecentCapacity) --position;
        for (unsigned i = position; i > 0; --i) recent[i] = recent[i - 1];
        recent[0] = action;
        if (recentCount < RecentCapacity && position == recentCount) ++recentCount;
    }
};
static const size_t QuickBytes = 6 + QuickAccess::Capacity * 5;
inline bool validQuickAction(QuickAction action) {
    return (action.kind == 1 && action.id < 15) ||
           (action.kind == 2 && action.id >= 1 && action.id <= 4) ||
           ((action.kind == 3 || action.kind == 4) && action.id != 0);
}
inline size_t encodeQuickAccess(const QuickAccess &state, uint8_t *bytes) {
    bytes[0] = 'S'; bytes[1] = 'Q'; bytes[2] = 1; bytes[3] = state.count;
    bytes[4] = state.history ? 1 : 0; bytes[5] = 0;
    for (unsigned i = 0; i < state.count; ++i) {
        bytes[6 + i * 5] = state.pins[i].kind;
        for (unsigned b = 0; b < 4; ++b) bytes[7 + i * 5 + b] = uint8_t(state.pins[i].id >> (8 * b));
    }
    return 6 + state.count * 5;
}
inline bool decodeQuickAccess(const uint8_t *bytes, size_t size, QuickAccess &state) {
    if (size < 6 || bytes[0] != 'S' || bytes[1] != 'Q' || bytes[2] != 1 ||
        bytes[3] > QuickAccess::Capacity || bytes[4] > 1 || bytes[5] != 0 || size != 6 + bytes[3] * 5) return false;
    QuickAccess decoded;
    decoded.history = bytes[4] != 0;
    for (unsigned i = 0; i < bytes[3]; ++i) {
        uint32_t id = 0;
        for (unsigned b = 0; b < 4; ++b) id |= uint32_t(bytes[7 + i * 5 + b]) << (8 * b);
        const QuickAction action(bytes[6 + i * 5], id);
        if (!validQuickAction(action) || decoded.find(action) >= 0) return false;
        decoded.pin(action, true);
    }
    state = decoded;
    return true;
}
}
