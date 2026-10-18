#pragma once

// IndexedMinHeap.h
// A binary min-heap of patients, plus a map from patient ID to heap index.
//
// Array layout (0-based):
//   parent(i) = (i - 1) / 2,  leftChild(i) = 2i + 1,  rightChild(i) = 2i + 2
//
// HEAP PROPERTY: no child has higher priority than its parent, i.e.
//   isHigherPriority(heap[child], heap[parent]) is false for every child.
// So heap[0] is always the next patient to attend.
//
// INDEX MAP: indexOf[patientId] == position of that patient in the heap array.
// It lets us find any patient in O(1) instead of scanning the array in O(n),
// which is what makes updatePatient() O(log n).

#include <unordered_map>
#include <utility>
#include <vector>

#include "Patient.h"

class IndexedMinHeap {
public:
    // Adds a new patient. O(log n).
    // Returns false (and changes nothing) for a duplicate ID or invalid fields.
    bool insert(Patient patient) {
        if (patient.id <= 0 || contains(patient.id) || !isValidESI(patient.esi) ||
            !isValidDeterioration(patient.deteriorationLevel) || !isValidZone(patient.zone) ||
            patient.waitingMinutes < 0) {
            return false;
        }
        patient.priorityScore = calculatePriorityScore(patient);

        // Place the patient in the first free leaf, then move it up while it
        // beats its parent. Everything else already satisfied the heap property.
        swapsInLastOperation = 0;
        heap.push_back(patient);
        int newIndex = size() - 1;
        indexOf[patient.id] = newIndex;
        siftUp(newIndex);
        return true;
    }

    // Removes the highest-priority patient (the root) into removedPatient. O(log n).
    // Returns false if the heap is empty.
    bool extractMin(Patient& removedPatient) {
        if (heap.empty()) {
            return false;
        }
        removedPatient = heap[0];

        // Move the last leaf into the root position, drop the old root from the
        // end of the array, then sift the new root down to restore the heap property.
        swapPatients(0, size() - 1);
        heap.pop_back();
        indexOf.erase(removedPatient.id);
        swapsInLastOperation = 0;  // count only the sift swaps below
        if (!heap.empty()) {
            siftDown(0);
        }
        return true;
    }

    // Removes ANY waiting patient by ID (e.g. "left without being seen"). O(log n).
    // Same idea as extractMin, but the hole can be anywhere in the array:
    // move the last leaf into the hole, then repair. Unlike extractMin, the moved
    // element may have to go UP (it came from a different subtree), so we use
    // repairPosition(), not just siftDown().
    bool removePatient(int patientId, Patient& removedPatient) {
        auto found = indexOf.find(patientId);
        if (found == indexOf.end()) {
            return false;
        }
        int index = found->second;
        removedPatient = heap[index];

        swapPatients(index, size() - 1);
        heap.pop_back();
        indexOf.erase(patientId);
        swapsInLastOperation = 0;  // count only the repair swaps below
        if (index < size()) {  // false when the removed patient was the last leaf
            repairPosition(index);
        }
        return true;
    }

    // Changes a waiting patient's ESI and deterioration level. O(log n):
    // O(1) average map lookup + one sift up or down.
    // Waiting time is left unchanged. Returns false for unknown ID or invalid values.
    bool updatePatient(int patientId, int newESI, int newDeteriorationLevel) {
        auto found = indexOf.find(patientId);
        if (found == indexOf.end() || !isValidESI(newESI) ||
            !isValidDeterioration(newDeteriorationLevel)) {
            return false;
        }
        swapsInLastOperation = 0;
        int index = found->second;
        heap[index].esi = newESI;
        heap[index].deteriorationLevel = newDeteriorationLevel;
        heap[index].priorityScore = calculatePriorityScore(heap[index]);

        // Only this one node's key changed, so only its own path can be wrong.
        repairPosition(index);
        return true;
    }

    // Adds `minutes` of waiting time to EVERY waiting patient and recomputes
    // every score. O(n): n score updates + one bottom-up heap rebuild.
    // (Repairing each patient one by one would cost O(n log n) instead.)
    bool advanceWaitingTime(int minutes) {
        if (minutes < 0) {
            return false;
        }
        swapsInLastOperation = 0;
        for (Patient& patient : heap) {
            patient.waitingMinutes += minutes;
            patient.priorityScore = calculatePriorityScore(patient);
        }
        // Many keys changed at once, so rebuild the heap property for the whole array.
        buildHeap();
        return true;
    }

    // Copies the patient with this ID into result. O(1) average.
    bool getPatient(int patientId, Patient& result) const {
        auto found = indexOf.find(patientId);
        if (found == indexOf.end()) {
            return false;
        }
        result = heap[found->second];
        return true;
    }

    bool contains(int patientId) const {
        return indexOf.count(patientId) > 0;
    }

    // Patients in internal heap-array order (NOT fully sorted). O(n) copy.
    std::vector<Patient> getAllPatients() const {
        return heap;
    }

    // Patients in attend order, produced by HEAP SORT on a copy. O(n log n).
    // The copy is popped n times with extractMin; the real heap is not modified.
    std::vector<Patient> getAllPatientsSorted() const {
        IndexedMinHeap copy = *this;  // O(n) copy of the array and the map
        std::vector<Patient> ordered;
        ordered.reserve(heap.size());
        Patient next;
        while (copy.extractMin(next)) {
            ordered.push_back(next);
        }
        return ordered;
    }

    // Current array index of a patient, or -1 if not waiting. O(1) average.
    // Used to print the ID -> index map.
    int indexOfPatient(int patientId) const {
        auto found = indexOf.find(patientId);
        return found == indexOf.end() ? -1 : found->second;
    }

    // Height of the tree = number of levels below the root = floor(log2 n).
    // A single siftUp or siftDown can never make more swaps than this.
    int height() const {
        int levels = 0;
        for (int n = size(); n > 1; n /= 2) {
            ++levels;
        }
        return levels;
    }

    // Number of swaps made by the most recent insert / extractMin / removePatient /
    // updatePatient / advanceWaitingTime. Lets us SEE that the work is O(log n).
    int lastOperationSwaps() const {
        return swapsInLastOperation;
    }

    bool isEmpty() const {
        return heap.empty();
    }

    int size() const {
        return static_cast<int>(heap.size());
    }

    // Verification helper used by the tests and the demo's consistency checks. O(n).
    // Checks the heap property, the cached scores and the ID -> index map.
    bool isValid() const {
        if (indexOf.size() != heap.size()) {
            return false;
        }
        for (int i = 0; i < size(); ++i) {
            auto found = indexOf.find(heap[i].id);
            if (found == indexOf.end() || found->second != i) {
                return false;
            }
            if (heap[i].priorityScore != calculatePriorityScore(heap[i])) {
                return false;
            }
            if (i > 0 && isHigherPriority(heap[i], heap[parentOf(i)])) {
                return false;
            }
        }
        return true;
    }

private:
    std::vector<Patient> heap;             // the binary heap stored as an array
    std::unordered_map<int, int> indexOf;  // patient ID -> index in `heap`
    int swapsInLastOperation = 0;          // instrumentation only, see lastOperationSwaps()

    static int parentOf(int index) { return (index - 1) / 2; }
    static int leftChildOf(int index) { return 2 * index + 1; }
    static int rightChildOf(int index) { return 2 * index + 2; }

    // Swaps two heap slots AND updates the index map for both patients.
    // Every movement in the heap goes through here, so the map never goes stale.
    void swapPatients(int i, int j) {
        if (i != j) {
            ++swapsInLastOperation;
        }
        std::swap(heap[i], heap[j]);
        indexOf[heap[i].id] = i;
        indexOf[heap[j].id] = j;
    }

    // Moves the node at `index` up while it has higher priority than its parent.
    // At most one swap per level: O(log n).
    void siftUp(int index) {
        while (index > 0) {
            int parent = parentOf(index);
            if (!isHigherPriority(heap[index], heap[parent])) {
                break;  // parent comes first: heap property holds on this path
            }
            swapPatients(index, parent);
            index = parent;
        }
    }

    // Moves the node at `index` down, swapping with its higher-priority child,
    // until neither child beats it. At most one swap per level: O(log n).
    void siftDown(int index) {
        int count = size();
        while (true) {
            int best = index;
            int left = leftChildOf(index);
            int right = rightChildOf(index);
            if (left < count && isHigherPriority(heap[left], heap[best])) {
                best = left;
            }
            if (right < count && isHigherPriority(heap[right], heap[best])) {
                best = right;
            }
            if (best == index) {
                break;  // node beats both children: heap property holds below it
            }
            swapPatients(index, best);
            index = best;
        }
    }

    // Restores the heap property after the key at `index` changed in either direction.
    // If the node now beats its parent it can only need to move up; otherwise it
    // can only need to move down. O(log n).
    void repairPosition(int index) {
        if (index > 0 && isHigherPriority(heap[index], heap[parentOf(index)])) {
            siftUp(index);
        } else {
            siftDown(index);
        }
    }

    // Bottom-up heap construction (the classic "heapify" loop).
    // Leaves are already valid heaps; sift down every internal node from the
    // last one to the root. Total cost O(n).
    void buildHeap() {
        for (int i = size() / 2 - 1; i >= 0; --i) {
            siftDown(i);
        }
    }
};
