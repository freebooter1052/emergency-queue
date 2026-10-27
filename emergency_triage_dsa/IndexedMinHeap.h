#pragma once

#include <unordered_map>
#include <utility>
#include <vector>

#include "Patient.h"

class IndexedMinHeap {
public:

    bool insert(Patient patient) {
        if (patient.id <= 0 || contains(patient.id) || !isValidESI(patient.esi) ||
            !isValidDeterioration(patient.deteriorationLevel) || !isValidZone(patient.zone) ||
            patient.waitingMinutes < 0) {
            return false;
        }
        patient.priorityScore = calculatePriorityScore(patient);

        swapsInLastOperation = 0;
        heap.push_back(patient);
        int newIndex = size() - 1;
        indexOf[patient.id] = newIndex;
        siftUp(newIndex);
        return true;
    }

    bool extractMin(Patient& removedPatient) {
        if (heap.empty()) {
            return false;
        }
        removedPatient = heap[0];

        swapPatients(0, size() - 1);
        heap.pop_back();
        indexOf.erase(removedPatient.id);
        swapsInLastOperation = 0;
        if (!heap.empty()) {
            siftDown(0);
        }
        return true;
    }

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
        swapsInLastOperation = 0;
        if (index < size()) {
            repairPosition(index);
        }
        return true;
    }

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

        repairPosition(index);
        return true;
    }

    bool transferZone(int patientId, int newZone) {
        auto found = indexOf.find(patientId);
        if (found == indexOf.end() || !isValidZone(newZone)) {
            return false;
        }
        swapsInLastOperation = 0;
        heap[found->second].zone = newZone;
        return true;
    }

    bool advanceWaitingTime(int minutes) {
        if (minutes < 0) {
            return false;
        }
        swapsInLastOperation = 0;
        for (Patient& patient : heap) {
            patient.waitingMinutes += minutes;
            patient.priorityScore = calculatePriorityScore(patient);
        }

        buildHeap();
        return true;
    }

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

    std::vector<Patient> getAllPatients() const {
        return heap;
    }

    std::vector<Patient> getAllPatientsSorted() const {
        IndexedMinHeap copy = *this;
        std::vector<Patient> ordered;
        ordered.reserve(heap.size());
        Patient next;
        while (copy.extractMin(next)) {
            ordered.push_back(next);
        }
        return ordered;
    }

    int rankOf(int patientId) const {
        auto found = indexOf.find(patientId);
        if (found == indexOf.end()) {
            return -1;
        }
        const Patient& target = heap[found->second];
        int rank = 1;
        for (const Patient& other : heap) {
            if (isHigherPriority(other, target)) {
                ++rank;
            }
        }
        return rank;
    }

    int indexOfPatient(int patientId) const {
        auto found = indexOf.find(patientId);
        return found == indexOf.end() ? -1 : found->second;
    }

    int height() const {
        int levels = 0;
        for (int n = size(); n > 1; n /= 2) {
            ++levels;
        }
        return levels;
    }

    int lastOperationSwaps() const {
        return swapsInLastOperation;
    }

    bool isEmpty() const {
        return heap.empty();
    }

    int size() const {
        return static_cast<int>(heap.size());
    }

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
    std::vector<Patient> heap;
    std::unordered_map<int, int> indexOf;
    int swapsInLastOperation = 0;

    static int parentOf(int index) { return (index - 1) / 2; }
    static int leftChildOf(int index) { return 2 * index + 1; }
    static int rightChildOf(int index) { return 2 * index + 2; }

    void swapPatients(int i, int j) {
        if (i != j) {
            ++swapsInLastOperation;
        }
        std::swap(heap[i], heap[j]);
        indexOf[heap[i].id] = i;
        indexOf[heap[j].id] = j;
    }

    void siftUp(int index) {
        while (index > 0) {
            int parent = parentOf(index);
            if (!isHigherPriority(heap[index], heap[parent])) {
                break;
            }
            swapPatients(index, parent);
            index = parent;
        }
    }

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
                break;
            }
            swapPatients(index, best);
            index = best;
        }
    }

    void repairPosition(int index) {
        if (index > 0 && isHigherPriority(heap[index], heap[parentOf(index)])) {
            siftUp(index);
        } else {
            siftDown(index);
        }
    }

    void buildHeap() {
        for (int i = size() / 2 - 1; i >= 0; --i) {
            siftDown(i);
        }
    }
};
