// tests.cpp
// Unit tests for IndexedMinHeap and SegmentTree. Not part of the demo program.
//
// Build:  g++ -std=c++17 -Wall -Wextra -O2 -o triage_tests tests.cpp
// Run:    ./triage_tests
//
// Randomised tests use a fixed seed, so every run performs the same operations.

// At -O2, GCC inlines the recursive SegmentTree::updateNode for the tree sizes
// used below and cannot prove the recursion depth is bounded, so it reports
// -Warray-bounds on paths that never run. Building with -D_GLIBCXX_ASSERTIONS
// (bounds-checked std::vector) passes every test, so the warning is silenced here only.
#pragma GCC diagnostic ignored "-Warray-bounds"

#include <algorithm>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "IndexedMinHeap.h"
#include "Patient.h"
#include "SegmentTree.h"

int passed = 0;
int failed = 0;

void check(bool condition, const std::string& description) {
    if (condition) {
        ++passed;
    } else {
        ++failed;
        std::cout << "  FAIL: " << description << "\n";
    }
}

Patient makePatient(int id, int esi, int deterioration, int zone, int arrival, int wait = 0) {
    Patient p;
    p.id = id;
    p.name = "P" + std::to_string(id);
    p.esi = esi;
    p.deteriorationLevel = deterioration;
    p.zone = zone;
    p.arrivalOrder = arrival;
    p.waitingMinutes = wait;
    return p;
}

// Brute-force reference: the patient that should be extracted next.
int bestIdByScan(const std::vector<Patient>& patients) {
    const Patient* best = &patients[0];
    for (const Patient& p : patients) {
        if (isHigherPriority(p, *best)) best = &p;
    }
    return best->id;
}

void testPriorityFormula() {
    std::cout << "Priority formula\n";
    Patient p = makePatient(1, 3, 1, 1, 1, 43);
    // (3-1)*100 - 1*30 - (43/5)*10 = 200 - 30 - 80 = 90
    check(calculatePriorityScore(p) == 90, "ESI 3, deterioration 1, wait 43 gives 90");
    Patient a = makePatient(1, 3, 0, 1, 1);
    Patient b = makePatient(2, 3, 0, 1, 2);
    a.priorityScore = b.priorityScore = 200;
    check(isHigherPriority(a, b) && !isHigherPriority(b, a), "tie broken by earlier arrival");
}

void testHeapBasics() {
    std::cout << "Heap insert / duplicate / invalid / extract\n";
    IndexedMinHeap heap;
    check(heap.isEmpty() && heap.size() == 0, "new heap is empty");

    Patient dummy;
    check(!heap.extractMin(dummy), "extractMin on empty heap returns false");

    check(heap.insert(makePatient(10, 3, 0, 2, 1)), "insert 10");
    check(heap.insert(makePatient(20, 1, 0, 1, 2)), "insert 20");
    check(heap.insert(makePatient(30, 5, 0, 4, 3)), "insert 30");
    check(heap.insert(makePatient(40, 2, 2, 3, 4)), "insert 40");
    check(heap.isValid(), "heap valid after inserts");
    check(heap.size() == 4, "size is 4");

    check(!heap.insert(makePatient(20, 4, 0, 1, 5)), "duplicate ID 20 rejected");
    check(!heap.insert(makePatient(50, 0, 0, 1, 5)), "ESI 0 rejected");
    check(!heap.insert(makePatient(50, 6, 0, 1, 5)), "ESI 6 rejected");
    check(!heap.insert(makePatient(50, 3, 5, 1, 5)), "deterioration 5 rejected");
    check(!heap.insert(makePatient(50, 3, -1, 1, 5)), "deterioration -1 rejected");
    check(!heap.insert(makePatient(50, 3, 0, 0, 5)), "zone 0 rejected");
    check(!heap.insert(makePatient(50, 3, 0, 6, 5)), "zone 6 rejected");
    check(!heap.insert(makePatient(0, 3, 0, 1, 5)), "ID 0 rejected");
    check(heap.size() == 4 && heap.isValid(), "rejected inserts changed nothing");

    // Scores: 20 -> 0, 40 -> 100 - 60 = 40, 10 -> 200, 30 -> 400
    std::vector<int> expectedOrder = {20, 40, 10, 30};
    std::vector<Patient> sorted = heap.getAllPatientsSorted();
    bool sortedOk = sorted.size() == 4;
    for (size_t i = 0; sortedOk && i < 4; ++i) sortedOk = sorted[i].id == expectedOrder[i];
    check(sortedOk, "getAllPatientsSorted order is 20, 40, 10, 30");
    check(heap.isValid() && heap.size() == 4, "sorted display did not modify heap");

    for (int expectedId : expectedOrder) {
        Patient out;
        check(heap.extractMin(out) && out.id == expectedId,
              "extractMin returns " + std::to_string(expectedId));
        check(!heap.contains(expectedId), "extracted ID removed from index map");
        check(heap.isValid(), "heap valid after extract");
    }
    check(heap.isEmpty(), "heap empty after extracting all");
    check(heap.insert(makePatient(20, 2, 0, 1, 9)), "ID can be reused after the patient left");
}

void testHeapUpdates() {
    std::cout << "Heap priority updates and time advancement\n";
    IndexedMinHeap heap;
    heap.insert(makePatient(1, 2, 0, 1, 1));  // 100
    heap.insert(makePatient(2, 3, 0, 2, 2));  // 200
    heap.insert(makePatient(3, 4, 0, 3, 3));  // 300
    heap.insert(makePatient(4, 5, 0, 4, 4));  // 400

    // Deterioration moves patient 4 to the front.
    check(heap.updatePatient(4, 2, 4), "update patient 4");  // 100 - 120 = -20
    Patient p;
    heap.getPatient(4, p);
    check(p.priorityScore == -20, "patient 4 score recalculated to -20");
    check(heap.isValid(), "heap valid after decrease");
    check(heap.getAllPatientsSorted()[0].id == 4, "patient 4 now first");

    // Improvement moves patient 1 to the back.
    check(heap.updatePatient(1, 5, 0), "update patient 1");  // 400, the largest score
    check(heap.isValid(), "heap valid after increase");
    std::vector<Patient> sorted = heap.getAllPatientsSorted();
    check(sorted.back().id == 1, "patient 1 now last");

    check(!heap.updatePatient(99, 3, 0), "update of unknown ID rejected");
    check(!heap.updatePatient(2, 7, 0), "update with ESI 7 rejected");
    check(!heap.updatePatient(2, 3, 9), "update with deterioration 9 rejected");
    heap.getPatient(2, p);
    check(p.esi == 3 && p.deteriorationLevel == 0, "rejected update changed nothing");

    check(heap.advanceWaitingTime(12), "advance 12 minutes");
    heap.getPatient(2, p);
    check(p.waitingMinutes == 12 && p.priorityScore == 200 - 20, "patient 2 aged: wait 12, score 180");
    check(heap.isValid(), "heap valid after advancing time");
    check(!heap.advanceWaitingTime(-5), "negative time rejected");
}

// Random mix of insert / update / extract / advance, checked after every step
// against a plain vector scanned by brute force.
void testHeapRandomised() {
    std::cout << "Heap randomised comparison with brute force (2000 operations)\n";
    std::mt19937 rng(12345);
    IndexedMinHeap heap;
    std::vector<Patient> reference;
    int nextId = 1;
    int arrival = 1;
    bool allValid = true;
    bool allExtractsMatch = true;
    bool allLookupsMatch = true;

    for (int step = 0; step < 2000; ++step) {
        int operation = static_cast<int>(rng() % 10);
        if (operation < 4 || reference.empty()) {
            Patient p = makePatient(nextId++, 1 + static_cast<int>(rng() % 5),
                                    static_cast<int>(rng() % 5), 1 + static_cast<int>(rng() % 5),
                                    arrival++);
            heap.insert(p);
            p.priorityScore = calculatePriorityScore(p);
            reference.push_back(p);
        } else if (operation < 7) {
            Patient& target = reference[rng() % reference.size()];
            int newEsi = 1 + static_cast<int>(rng() % 5);
            int newDet = static_cast<int>(rng() % 5);
            heap.updatePatient(target.id, newEsi, newDet);
            target.esi = newEsi;
            target.deteriorationLevel = newDet;
            target.priorityScore = calculatePriorityScore(target);
        } else if (operation < 9) {
            int expectedId = bestIdByScan(reference);
            Patient out;
            heap.extractMin(out);
            if (out.id != expectedId) allExtractsMatch = false;
            reference.erase(std::find_if(reference.begin(), reference.end(),
                                         [&](const Patient& r) { return r.id == expectedId; }));
        } else {
            int minutes = 1 + static_cast<int>(rng() % 9);
            heap.advanceWaitingTime(minutes);
            for (Patient& r : reference) {
                r.waitingMinutes += minutes;
                r.priorityScore = calculatePriorityScore(r);
            }
        }

        if (!heap.isValid() || heap.size() != static_cast<int>(reference.size())) allValid = false;
        for (const Patient& r : reference) {
            Patient fromHeap;
            if (!heap.getPatient(r.id, fromHeap) || fromHeap.priorityScore != r.priorityScore ||
                fromHeap.waitingMinutes != r.waitingMinutes) {
                allLookupsMatch = false;
            }
        }
    }
    check(allValid, "heap property, index map and size valid after every operation");
    check(allExtractsMatch, "every extractMin matched the brute-force minimum");
    check(allLookupsMatch, "every getPatient matched the reference record");
}

void testSegmentTree() {
    std::cout << "Segment tree build / query / update\n";
    std::vector<ZoneStats> zones(5);
    int counts[5] = {1, 4, 1, 1, 0};
    int waits[5] = {70, 150, 70, 40, 0};
    int caps[5] = {2, 3, 3, 2, 2};
    for (int i = 0; i < 5; ++i) zones[i] = ZoneStats{counts[i], waits[i], caps[i]};

    SegmentTree tree(5);
    check(tree.build(zones), "build with 5 zones");
    ZoneStats r = tree.query(1, 3);  // UI zones 2..4
    check(r.patientCount == 6 && r.totalWaitingTime == 260 && r.totalCapacity == 8,
          "zones 2-4: 6 patients, 260 min, capacity 8");
    r = tree.query(4, 4);  // UI zone 5
    check(r.patientCount == 0 && r.totalWaitingTime == 0 && r.totalCapacity == 2,
          "zone 5 empty with capacity 2");
    r = tree.query(0, 4);
    check(r.patientCount == 7 && r.totalWaitingTime == 330 && r.totalCapacity == 12,
          "all zones: 7 patients, 330 min, capacity 12");

    check(tree.update(2, ZoneStats{3, 90, 3}), "point update zone index 2");
    r = tree.query(1, 3);
    check(r.patientCount == 8 && r.totalWaitingTime == 280, "range reflects point update");

    check(!tree.update(5, ZoneStats{}), "update out of range rejected");
    check(!tree.update(-1, ZoneStats{}), "update with negative index rejected");
    r = tree.query(3, 1);
    check(r.patientCount == 0 && r.totalCapacity == 0, "reversed range returns empty stats");

    std::cout << "Segment tree randomised comparison with brute force\n";
    std::mt19937 rng(777);
    bool allMatch = true;
    for (int size = 1; size <= 9; ++size) {
        std::vector<ZoneStats> values(size);
        for (ZoneStats& v : values) {
            v = ZoneStats{static_cast<int>(rng() % 10), static_cast<int>(rng() % 200),
                          static_cast<int>(rng() % 6)};
        }
        SegmentTree randomTree(size);
        randomTree.build(values);
        for (int round = 0; round < 200; ++round) {
            int index = static_cast<int>(rng() % size);
            values[index] = ZoneStats{static_cast<int>(rng() % 10), static_cast<int>(rng() % 200),
                                      static_cast<int>(rng() % 6)};
            randomTree.update(index, values[index]);
            for (int left = 0; left < size; ++left) {
                for (int right = left; right < size; ++right) {
                    ZoneStats expected;
                    for (int i = left; i <= right; ++i) expected = combineStats(expected, values[i]);
                    ZoneStats actual = randomTree.query(left, right);
                    if (actual.patientCount != expected.patientCount ||
                        actual.totalWaitingTime != expected.totalWaitingTime ||
                        actual.totalCapacity != expected.totalCapacity) {
                        allMatch = false;
                    }
                }
            }
        }
    }
    check(allMatch, "every range query matched brute-force sums (sizes 1-9)");
}

int main() {
    testPriorityFormula();
    testHeapBasics();
    testHeapUpdates();
    testHeapRandomised();
    testSegmentTree();
    std::cout << "\nPassed: " << passed << "   Failed: " << failed << "\n";
    return failed == 0 ? 0 : 1;
}
