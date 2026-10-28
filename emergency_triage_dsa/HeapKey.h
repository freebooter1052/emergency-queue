#pragma once

#include <cstdint>

// Lean (id, key) priority-queue interface shared by DaryHeap and FibonacciHeap.
// The benchmark uses these instead of IndexedMinHeap so that no heap pays for copying
// Patient strings. IDs are dense integers 0..maxId-1 so every heap can use a plain
// vector as its ID -> position/handle map.
//
// key = score * HEAP_KEY_SCALE + arrivalOrder, so a smaller key is more urgent and the
// arrival-order tie-break of isHigherPriority() is preserved. Keys are unique.
using HeapKey = long long;

constexpr HeapKey HEAP_KEY_SCALE = 1LL << 24;

inline HeapKey makeHeapKey(int score, int arrivalOrder) {
    return static_cast<HeapKey>(score) * HEAP_KEY_SCALE + arrivalOrder;
}
