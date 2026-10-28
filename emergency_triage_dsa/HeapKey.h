#pragma once

#include <cstdint>

using HeapKey = long long;

constexpr HeapKey HEAP_KEY_SCALE = 1LL << 24;

inline HeapKey makeHeapKey(int score, int arrivalOrder) {
    return static_cast<HeapKey>(score) * HEAP_KEY_SCALE + arrivalOrder;
}
