#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "HeapKey.h"

// Array-based d-ary min-heap with an ID -> index position map.
// D = 2 is the binary-heap baseline from the evaluation plan.
//
//   parent(i) = (i - 1) / D       first child(i) = D * i + 1
//
// Bigger D means a shallower tree (decrease-key / insert walk log_D n levels) but
// extract-min must scan D children per level, so it costs O(D log_D n).
template <int D>
class DaryHeap {
    static_assert(D >= 2, "a d-ary heap needs at least 2 children per node");

public:
    explicit DaryHeap(int maxId) : position(maxId, ABSENT) {}

    bool insert(int id, HeapKey key) {
        if (id < 0 || id >= static_cast<int>(position.size()) || position[id] != ABSENT) {
            return false;
        }
        heap.push_back(Entry{id, key});
        position[id] = static_cast<int>(heap.size()) - 1;
        siftUp(static_cast<int>(heap.size()) - 1);
        return true;
    }

    // Only a strictly smaller key is accepted (a deterioration moves a patient toward the front).
    bool decreaseKey(int id, HeapKey newKey) {
        if (!contains(id) || newKey >= heap[position[id]].key) {
            return false;
        }
        heap[position[id]].key = newKey;
        siftUp(position[id]);
        return true;
    }

    bool extractMin(int& id, HeapKey& key) {
        if (heap.empty()) {
            return false;
        }
        id = heap[0].id;
        key = heap[0].key;
        position[id] = ABSENT;
        Entry last = heap.back();
        heap.pop_back();
        if (!heap.empty()) {
            heap[0] = last;
            position[last.id] = 0;
            siftDown(0);
        }
        return true;
    }

    bool contains(int id) const {
        return id >= 0 && id < static_cast<int>(position.size()) && position[id] != ABSENT;
    }

    bool peekMin(int& id, HeapKey& key) const {
        if (heap.empty()) {
            return false;
        }
        id = heap[0].id;
        key = heap[0].key;
        return true;
    }

    int size() const { return static_cast<int>(heap.size()); }
    bool isEmpty() const { return heap.empty(); }

    // Proxy for memory traffic: heap entries read or written. Hardware cache misses
    // need an external profiler; see README section 12.
    std::uint64_t touches() const { return touched; }

    // Bytes currently reserved: entry array capacity plus the ID -> index map.
    std::size_t memoryBytes() const {
        return heap.capacity() * sizeof(Entry) + position.capacity() * sizeof(int);
    }

    bool isValid() const {
        for (int i = 0; i < size(); ++i) {
            if (position[heap[i].id] != i) return false;
            if (i > 0 && heap[i].key < heap[(i - 1) / D].key) return false;
        }
        int present = 0;
        for (int p : position) present += (p != ABSENT);
        return present == size();
    }

private:
    static constexpr int ABSENT = -1;

    struct Entry {
        int id;
        HeapKey key;
    };

    std::vector<Entry> heap;
    std::vector<int> position;
    std::uint64_t touched = 0;

    void place(int index, const Entry& entry) {
        heap[index] = entry;
        position[entry.id] = index;
        ++touched;
    }

    // Moves a "hole" upward instead of swapping, so each level costs one write.
    void siftUp(int index) {
        Entry moving = heap[index];
        ++touched;
        while (index > 0) {
            int parent = (index - 1) / D;
            ++touched;
            if (!(moving.key < heap[parent].key)) {
                break;
            }
            place(index, heap[parent]);
            index = parent;
        }
        place(index, moving);
    }

    void siftDown(int index) {
        Entry moving = heap[index];
        ++touched;
        int count = size();
        while (true) {
            int first = D * index + 1;
            if (first >= count) {
                break;
            }
            int last = first + D < count ? first + D : count;
            int best = first;
            for (int child = first + 1; child < last; ++child) {
                if (heap[child].key < heap[best].key) best = child;
            }
            touched += static_cast<std::uint64_t>(last - first);
            if (!(heap[best].key < moving.key)) {
                break;
            }
            place(index, heap[best]);
            index = best;
        }
        place(index, moving);
    }
};
