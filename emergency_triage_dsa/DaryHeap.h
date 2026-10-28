#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "HeapKey.h"

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

    std::uint64_t touches() const { return touched; }

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
