#pragma GCC diagnostic ignored "-Warray-bounds"

#include <algorithm>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include <map>
#include <set>

#include "BedMatching.h"
#include "BucketSegmentTree.h"
#include "DaryHeap.h"
#include "FibonacciHeap.h"
#include "IndexedMinHeap.h"
#include "Patient.h"
#include "SegmentTree.h"
#include "Workload.h"

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
    heap.insert(makePatient(1, 2, 0, 1, 1));
    heap.insert(makePatient(2, 3, 0, 2, 2));
    heap.insert(makePatient(3, 4, 0, 3, 3));
    heap.insert(makePatient(4, 5, 0, 4, 4));

    check(heap.updatePatient(4, 2, 4), "update patient 4");
    Patient p;
    heap.getPatient(4, p);
    check(p.priorityScore == -20, "patient 4 score recalculated to -20");
    check(heap.isValid(), "heap valid after decrease");
    check(heap.getAllPatientsSorted()[0].id == 4, "patient 4 now first");

    check(heap.updatePatient(1, 5, 0), "update patient 1");
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

void testHeapRemove() {
    std::cout << "Heap removePatient (delete from the middle)\n";
    IndexedMinHeap heap;

    heap.insert(makePatient(1, 1, 0, 1, 1));
    heap.insert(makePatient(2, 4, 0, 1, 2));
    heap.insert(makePatient(3, 2, 0, 1, 3));
    heap.insert(makePatient(4, 5, 0, 1, 4));
    heap.insert(makePatient(5, 5, 0, 1, 5));
    heap.insert(makePatient(6, 3, 0, 1, 6));
    check(heap.indexOfPatient(4) == 3 && heap.indexOfPatient(6) == 5, "known starting layout");

    Patient out;
    check(heap.removePatient(4, out) && out.id == 4, "remove D from the middle");
    check(heap.indexOfPatient(6) == 1 && heap.indexOfPatient(2) == 3, "moved leaf F sifted UP to [1]");
    check(heap.lastOperationSwaps() == 1, "sift-up removal made 1 swap");
    check(heap.isValid() && !heap.contains(4), "heap valid, D gone from map");

    check(heap.removePatient(1, out) && out.id == 1, "remove root A by ID");
    check(heap.indexOfPatient(3) == 0 && heap.indexOfPatient(5) == 2, "moved leaf E sifted DOWN to [2]");
    check(heap.isValid(), "heap valid after sift-down removal");

    int lastId = heap.getAllPatients().back().id;
    check(heap.removePatient(lastId, out) && heap.lastOperationSwaps() == 0, "remove last leaf, 0 swaps");
    check(heap.isValid() && heap.size() == 3, "heap valid with 3 patients");

    check(!heap.removePatient(99, out), "remove unknown ID rejected");
    check(heap.size() == 3, "failed remove changed nothing");
}

void testHeapTransferAndRank() {
    std::cout << "Heap transferZone and rankOf\n";
    IndexedMinHeap heap;
    heap.insert(makePatient(1, 2, 0, 1, 1));
    heap.insert(makePatient(2, 3, 0, 2, 2));
    heap.insert(makePatient(3, 3, 0, 2, 3));
    heap.insert(makePatient(4, 5, 0, 4, 4));

    std::vector<Patient> before = heap.getAllPatients();
    check(heap.transferZone(3, 5), "transfer patient 3 to zone 5");
    Patient p;
    heap.getPatient(3, p);
    check(p.zone == 5, "patient 3 now in zone 5");
    std::vector<Patient> after = heap.getAllPatients();
    bool sameLayout = before.size() == after.size();
    for (size_t i = 0; sameLayout && i < before.size(); ++i) sameLayout = before[i].id == after[i].id;
    check(sameLayout && heap.lastOperationSwaps() == 0, "transfer did not move anyone in the heap");
    check(heap.isValid(), "heap valid after transfer");

    check(!heap.transferZone(99, 2), "transfer of unknown ID rejected");
    check(!heap.transferZone(2, 0) && !heap.transferZone(2, 6), "transfer to invalid zone rejected");
    heap.getPatient(2, p);
    check(p.zone == 2, "rejected transfer changed nothing");

    std::vector<Patient> sorted = heap.getAllPatientsSorted();
    bool ranksMatch = true;
    for (size_t i = 0; i < sorted.size(); ++i) {
        if (heap.rankOf(sorted[i].id) != static_cast<int>(i) + 1) ranksMatch = false;
    }
    check(ranksMatch, "rankOf matches position in heap-sort order (including the tie 2 vs 3)");
    check(heap.rankOf(99) == -1, "rankOf unknown ID is -1");
}

void testHeapRandomised() {
    std::cout << "Heap randomised comparison with brute force (2000 operations)\n";
    bool allSwapsWithinHeight = true;
    bool allSortedMatch = true;
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
        } else if (operation < 6) {
            Patient& target = reference[rng() % reference.size()];
            int newEsi = 1 + static_cast<int>(rng() % 5);
            int newDet = static_cast<int>(rng() % 5);
            heap.updatePatient(target.id, newEsi, newDet);
            target.esi = newEsi;
            target.deteriorationLevel = newDet;
            target.priorityScore = calculatePriorityScore(target);
        } else if (operation < 7) {

            size_t position = rng() % reference.size();
            Patient out;
            if (!heap.removePatient(reference[position].id, out) || out.id != reference[position].id) {
                allExtractsMatch = false;
            }
            reference.erase(reference.begin() + static_cast<long>(position));
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

        if (operation < 9 && heap.lastOperationSwaps() > heap.height()) allSwapsWithinHeight = false;
        if (step % 50 == 0) {
            std::vector<Patient> expected = reference;
            std::sort(expected.begin(), expected.end(), isHigherPriority);
            std::vector<Patient> actual = heap.getAllPatientsSorted();
            bool same = expected.size() == actual.size();
            for (size_t i = 0; same && i < expected.size(); ++i) same = expected[i].id == actual[i].id;
            if (!same || !heap.isValid()) allSortedMatch = false;
        }
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
    check(allSwapsWithinHeight, "every single-patient operation made at most height swaps");
    check(allSortedMatch, "heap-sort display matched std::sort and left the heap intact");
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
    ZoneStats r = tree.query(1, 3);
    check(r.patientCount == 6 && r.totalWaitingTime == 260 && r.totalCapacity == 8,
          "zones 2-4: 6 patients, 260 min, capacity 8");
    r = tree.query(4, 4);
    check(r.patientCount == 0 && r.totalWaitingTime == 0 && r.totalCapacity == 2,
          "zone 5 empty with capacity 2");
    r = tree.query(0, 4);
    check(r.patientCount == 7 && r.totalWaitingTime == 330 && r.totalCapacity == 12,
          "all zones: 7 patients, 330 min, capacity 12");

    check(r.busiestZone == 1 && r.busiestLoad == 1, "all zones: busiest is index 1 (zone 2), load +1");
    r = tree.query(2, 4);
    check(r.busiestZone == 3 && r.busiestLoad == -1, "zones 3-5: busiest is index 3 (zone 4), load -1");
    r = tree.query(4, 4);
    check(r.busiestZone == 4 && r.busiestLoad == -2, "single zone is its own busiest zone");

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

                    int bestZone = left;
                    for (int i = left + 1; i <= right; ++i) {
                        if (values[i].patientCount - values[i].totalCapacity >
                            values[bestZone].patientCount - values[bestZone].totalCapacity) {
                            bestZone = i;
                        }
                    }
                    ZoneStats actual = randomTree.query(left, right);
                    if (actual.patientCount != expected.patientCount ||
                        actual.totalWaitingTime != expected.totalWaitingTime ||
                        actual.totalCapacity != expected.totalCapacity ||
                        actual.busiestZone != bestZone) {
                        allMatch = false;
                    }
                }
            }
        }
    }
    check(allMatch, "every range query matched brute-force sums and busiest zone (sizes 1-9)");
}

template <class Heap>
bool randomisedHeapRun(Heap& heap, unsigned seed, int steps, int idSpace) {
    std::mt19937 rng(seed);
    std::set<std::pair<HeapKey, int>> reference;
    std::map<int, HeapKey> keyOf;
    bool ok = true;
    for (int step = 0; step < steps && ok; ++step) {
        int roll = static_cast<int>(rng() % 10);
        if (roll < 4 || keyOf.empty()) {
            int id = static_cast<int>(rng() % idSpace);
            HeapKey key = makeHeapKey(static_cast<int>(rng() % 600) - 100, id + 1);
            bool expected = keyOf.count(id) == 0;
            ok = ok && heap.insert(id, key) == expected;
            if (expected) {
                keyOf[id] = key;
                reference.insert({key, id});
            }
        } else if (roll < 7) {
            auto it = keyOf.begin();
            std::advance(it, static_cast<long>(rng() % keyOf.size()));
            HeapKey newKey = it->second + static_cast<HeapKey>(static_cast<int>(rng() % 5000) - 1000) *
                                              HEAP_KEY_SCALE / 8;
            bool expected = newKey < it->second;
            ok = ok && heap.decreaseKey(it->first, newKey) == expected;
            if (expected) {
                reference.erase({it->second, it->first});
                reference.insert({newKey, it->first});
                it->second = newKey;
            }
        } else {
            int id;
            HeapKey key;
            ok = ok && heap.extractMin(id, key);
            ok = ok && id == reference.begin()->second && key == reference.begin()->first;
            keyOf.erase(reference.begin()->second);
            reference.erase(reference.begin());
        }
        ok = ok && heap.size() == static_cast<int>(reference.size()) && heap.isValid();
        int peekId;
        HeapKey peekKey;
        if (!reference.empty()) {
            ok = ok && heap.peekMin(peekId, peekKey) && peekId == reference.begin()->second;
        }
    }
    int id;
    HeapKey key;
    while (!reference.empty() && ok) {
        ok = heap.extractMin(id, key) && id == reference.begin()->second;
        reference.erase(reference.begin());
    }
    return ok && heap.isEmpty() && !heap.extractMin(id, key);
}

void testDaryHeaps() {
    std::cout << "d-ary heap (D = 2, 3, 4, 8) against a reference set\n";
    DaryHeap<2> binary(50);
    check(binary.insert(7, makeHeapKey(10, 1)) && !binary.insert(7, makeHeapKey(5, 2)),
          "duplicate ID rejected");
    check(!binary.insert(-1, 0) && !binary.insert(50, 0), "out-of-range ID rejected");
    check(!binary.decreaseKey(7, makeHeapKey(10, 1)), "decreaseKey with an equal key rejected");
    check(!binary.decreaseKey(7, makeHeapKey(20, 1)), "decreaseKey with a larger key rejected");
    check(!binary.decreaseKey(8, makeHeapKey(1, 1)), "decreaseKey of an absent ID rejected");
    check(binary.decreaseKey(7, makeHeapKey(-5, 1)) && binary.contains(7), "decreaseKey accepted");

    DaryHeap<2> h2(40);
    DaryHeap<3> h3(40);
    DaryHeap<4> h4(40);
    DaryHeap<8> h8(40);
    check(randomisedHeapRun(h2, 11, 4000, 40), "binary heap: 4000 random operations match the reference");
    check(randomisedHeapRun(h3, 12, 4000, 40), "3-ary heap: 4000 random operations match the reference");
    check(randomisedHeapRun(h4, 13, 4000, 40), "4-ary heap: 4000 random operations match the reference");
    check(randomisedHeapRun(h8, 14, 4000, 40), "8-ary heap: 4000 random operations match the reference");
    check(h4.touches() > 0 && h4.memoryBytes() > 0, "work and memory counters report something");
}

void testFibonacciHeap() {
    std::cout << "Fibonacci heap (cut, cascading cut, consolidate) against a reference set\n";
    FibonacciHeap heap(100);
    check(heap.isEmpty() && heap.isValid(), "new heap is empty and valid");
    int id;
    HeapKey key;
    check(!heap.extractMin(id, key) && !heap.peekMin(id, key), "empty heap: extract and peek fail");

    for (int i = 0; i < 64; ++i) heap.insert(i, makeHeapKey(100 + i, i + 1));
    check(heap.rootCount() == 64, "64 inserts leave 64 roots (insert is lazy)");
    check(!heap.insert(5, makeHeapKey(1, 1)), "duplicate ID rejected");
    check(heap.extractMin(id, key) && id == 0, "first extract returns the smallest key");
    check(heap.rootCount() < 64 && heap.isValid(), "extract consolidated the root list");

    for (int i = 0; i < 20; ++i) {
        heap.extractMin(id, key);
    }
    bool allDecreased = true;
    for (int i = 63; i >= 40; --i) {
        if (heap.contains(i)) allDecreased &= heap.decreaseKey(i, makeHeapKey(-i, i + 1));
        allDecreased &= heap.isValid();
    }
    check(allDecreased, "decreasing many nested nodes keeps the heap valid");
    check(heap.peekMin(id, key) && id == 63, "smallest decreased key is now the minimum");
    check(!heap.decreaseKey(63, makeHeapKey(-63, 64)) && !heap.decreaseKey(99, 0),
          "equal key and absent ID rejected");

    FibonacciHeap r1(40), r2(40);
    check(randomisedHeapRun(r1, 21, 4000, 40), "4000 random operations (small ID space) match the reference");
    check(randomisedHeapRun(r2, 22, 4000, 40), "second seed matches the reference");
    FibonacciHeap big(3000);
    check(randomisedHeapRun(big, 23, 12000, 3000), "12000 operations with up to 3000 patients match");
    check(big.memoryBytes() > 0 && big.touches() > 0, "work and memory counters report something");
}

void testHeapsAgreeOnWorkloads() {
    std::cout << "All heaps replay the generated triage workloads identically\n";
    bool allMatch = true;
    for (double ratio : {0.0, 0.5, 0.9}) {
        for (WorkloadSpec spec : {steadyWorkload(ratio), burstWorkload(ratio)}) {
            spec.preload = 300;
            spec.operations = 4000;
            spec.seed = 99;
            Workload w = generateWorkload(spec);
            DaryHeap<2> binary(w.maxId);
            DaryHeap<4> quad(w.maxId);
            FibonacciHeap fib(w.maxId);
            auto replay = [&](auto& heap) {
                bool ok = true;
                auto apply = [&](const Op& op) {
                    if (op.type == OpType::Insert) return heap.insert(op.id, op.key);
                    if (op.type == OpType::DecreaseKey) return heap.decreaseKey(op.id, op.key);
                    int id;
                    HeapKey key;
                    return heap.extractMin(id, key) && id == op.expectedId;
                };
                for (const Op& op : w.preload) ok &= apply(op);
                for (const Op& op : w.operations) ok &= apply(op);
                return ok && heap.isValid();
            };
            allMatch = allMatch && replay(binary) && replay(quad) && replay(fib);
            Workload again = generateWorkload(spec);
            allMatch = allMatch && again.operations.size() == w.operations.size() &&
                       again.operations.back().key == w.operations.back().key;
        }
    }
    check(allMatch, "binary, 4-ary and Fibonacci follow the reference order; the generator is repeatable");

    WorkloadSpec burst = burstWorkload(0.3);
    burst.preload = 500;
    burst.operations = 6000;
    Workload w = generateWorkload(burst);
    WorkloadSpec steady = steadyWorkload(0.3);
    steady.preload = 500;
    steady.operations = 6000;
    Workload s = generateWorkload(steady);
    check(w.peakQueueSize > s.peakQueueSize, "the burst phase builds a larger queue than steady arrivals");
    double decreaseShare = static_cast<double>(s.decreaseCount) / 6000;
    check(decreaseShare > 0.25 && decreaseShare < 0.35, "steady workload has about the requested 30% decrease-keys");
}

void testBucketSegmentTree() {
    std::cout << "Zone x ESI segment tree with lazy waiting time\n";
    BucketSegmentTree tree(5, 5);
    tree.addPatient(1, 2, 0);
    tree.addPatient(1, 2, 10);
    tree.addPatient(2, 2, 20);
    tree.addPatient(2, 2, 40);
    tree.addPatient(3, 4, 5);
    check(!tree.addPatient(0, 2, 0) && !tree.addPatient(1, 6, 0), "invalid zone / ESI rejected");

    BucketStats stats = tree.query(1, 2, 2, 2);
    WaitSummary at60 = summarizeWaits(stats, 60);
    check(at60.count == 4 && at60.totalWait == 60 + 50 + 40 + 20, "ESI 2, zones 1-2 at minute 60: 4 patients, 170 min");
    check(at60.longestWait == 60 && at60.shortestWait == 20, "longest 60 min, shortest 20 min");
    WaitSummary at100 = summarizeWaits(stats, 100);
    check(at100.totalWait == 170 + 4 * 40, "advancing the clock by 40 raised the total by 4 * 40 with no tree update");
    check(at100.averageWait == 82.5, "average wait is total / count = 82.5");

    check(tree.query(1, 5, 1, 5).count == 5, "all zones, all ESI: 5 patients");
    check(tree.query(3, 3, 2, 2).count == 0 && summarizeWaits(tree.query(3, 3, 2, 2), 50).averageWait == 0,
          "empty bucket gives count 0 and no division by zero");
    check(tree.query(2, 1, 1, 5).count == 0 && tree.query(1, 5, 0, 5).count == 0, "invalid ranges return empty stats");

    check(tree.removePatient(1, 2, 0), "remove the oldest ESI-2 patient");
    check(summarizeWaits(tree.query(1, 2, 2, 2), 60).longestWait == 50, "longest wait recomputed from the multiset");
    check(!tree.removePatient(1, 2, 0), "removing a patient who is not there is rejected");
    check(tree.isConsistent(), "tree consistent after updates");

    tree.addPatient(4, 3, 15);
    tree.addPatient(4, 3, 15);
    tree.removePatient(4, 3, 15);
    check(tree.query(4, 4, 3, 3).count == 1, "two patients with equal arrival time: removing one leaves one");

    std::mt19937 rng(5150);
    BucketSegmentTree random(5, 5);
    std::vector<BucketRecord> records;
    bool allMatch = true;
    for (int step = 0; step < 4000; ++step) {
        if (records.empty() || rng() % 3 != 0) {
            BucketRecord r{1 + static_cast<int>(rng() % 5), 1 + static_cast<int>(rng() % 5),
                           static_cast<int>(rng() % 500)};
            records.push_back(r);
            random.addPatient(r.zone, r.esi, r.arrival);
        } else {
            size_t pick = rng() % records.size();
            random.removePatient(records[pick].zone, records[pick].esi, records[pick].arrival);
            records.erase(records.begin() + static_cast<long>(pick));
        }
        int a = 1 + static_cast<int>(rng() % 5), b = 1 + static_cast<int>(rng() % 5);
        int c = 1 + static_cast<int>(rng() % 5), d = 1 + static_cast<int>(rng() % 5);
        BucketStats got = random.query(std::min(a, b), std::max(a, b), std::min(c, d), std::max(c, d));
        BucketStats want = linearScanStats(records, std::min(a, b), std::max(a, b), std::min(c, d), std::max(c, d));
        if (got.count != want.count || got.sumArrival != want.sumArrival ||
            got.oldestArrival != want.oldestArrival || got.newestArrival != want.newestArrival) {
            allMatch = false;
        }
    }
    check(allMatch && random.isConsistent(), "4000 random updates: every range query equals the linear scan");

    random.resetNodeCount();
    random.query(1, 5, 1, 1);
    check(random.nodesVisited() <= 4 * 6, "one-ESI range query visits only a handful of nodes (O(log k))");
}

void testLazyAging() {
    std::cout << "Lazy aging keys\n";
    std::mt19937 rng(808);
    bool sameOrderAtEveryClock = true;
    bool lazyMatchesContinuous = true;
    struct Entry { int key; int base; int arrival; };
    std::vector<Entry> entries;
    for (int i = 0; i < 200; ++i) {
        int esi = 1 + static_cast<int>(rng() % 5), det = static_cast<int>(rng() % 5);
        int arrival = static_cast<int>(rng() % 300);
        entries.push_back({lazyAgingKey(esi, det, arrival), (esi - 1) * ESI_WEIGHT - det * DETERIORATION_WEIGHT, arrival});
    }
    for (int now : {300, 450, 1000, 5000}) {
        std::vector<std::pair<int, int>> byLazy, byContinuous;
        for (size_t i = 0; i < entries.size(); ++i) {
            int continuous = entries[i].base - 2 * (now - entries[i].arrival);
            if (lazyEffectiveScore(entries[i].key, now) != continuous) lazyMatchesContinuous = false;
            byLazy.push_back({entries[i].key, static_cast<int>(i)});
            byContinuous.push_back({continuous, static_cast<int>(i)});
        }
        std::sort(byLazy.begin(), byLazy.end());
        std::sort(byContinuous.begin(), byContinuous.end());
        for (size_t i = 0; i < byLazy.size(); ++i) {
            if (byLazy[i].second != byContinuous[i].second) sameOrderAtEveryClock = false;
        }
    }
    check(lazyMatchesContinuous, "lazy key minus 2 * now equals the continuous aged score");
    check(sameOrderAtEveryClock, "ordering by the fixed key equals ordering by the aged score at every clock time");
}

void testStarvation() {
    std::cout << "Starvation: aging lets a semi-urgent patient overtake a stream of critical arrivals\n";
    IndexedMinHeap heap;
    heap.insert(makePatient(1, 4, 0, 1, 1));
    int nextId = 2;
    int arrival = 2;
    int servedMinute = -1;
    for (int minute = 1; minute <= 600 && servedMinute < 0; ++minute) {
        heap.advanceWaitingTime(1);
        if (minute % 5 == 0) {
            heap.insert(makePatient(nextId++, 2, 0, 1, arrival++));
            Patient next;
            heap.extractMin(next);
            if (next.id == 1) servedMinute = minute;
        }
    }
    check(servedMinute > 0, "the ESI-4 patient is eventually attended despite constant ESI-2 arrivals");
    check(servedMinute <= 125, "...after about (300 - 100) / 2 = 100 extra minutes of waiting, not never");
}

bool kuhnTry(int u, const std::vector<std::vector<int>>& adj, std::vector<int>& matchRight,
             std::vector<char>& seen) {
    for (int v : adj[u]) {
        if (seen[v]) continue;
        seen[v] = 1;
        if (matchRight[v] < 0 || kuhnTry(matchRight[v], adj, matchRight, seen)) {
            matchRight[v] = u;
            return true;
        }
    }
    return false;
}

int kuhnMatching(const std::vector<std::vector<int>>& adj, int rightCount) {
    std::vector<int> matchRight(rightCount, -1);
    int size = 0;
    for (int u = 0; u < static_cast<int>(adj.size()); ++u) {
        std::vector<char> seen(rightCount, 0);
        if (kuhnTry(u, adj, matchRight, seen)) ++size;
    }
    return size;
}

void testHopcroftKarp() {
    std::cout << "Hopcroft-Karp bed matching\n";
    HopcroftKarp small(2, 2);
    small.addEdge(0, 0);
    small.addEdge(0, 1);
    small.addEdge(1, 0);
    check(small.run(true) == 2 && small.isValidMatching(), "augmenting path re-routes patient 0 to bed 1");
    check(small.bedOfPatient(1) == 0 && small.bedOfPatient(0) == 1, "patient 1 holds bed 0, patient 0 holds bed 1");

    HopcroftKarp none(3, 3);
    check(none.run() == 0 && none.isValidMatching(), "no edges: empty matching");
    HopcroftKarp crowded(4, 2);
    for (int u = 0; u < 4; ++u) { crowded.addEdge(u, 0); crowded.addEdge(u, 1); }
    check(crowded.run() == 2 && crowded.isValidMatching(), "4 patients, 2 beds: matching size 2");
    check(crowded.bedOfPatient(0) >= 0 && crowded.bedOfPatient(1) >= 0 && crowded.bedOfPatient(2) < 0,
          "greedy start keeps the first (most urgent) patients matched");
    HopcroftKarp invalid(2, 2);
    invalid.addEdge(5, 0);
    invalid.addEdge(0, 9);
    check(invalid.run() == 0, "out-of-range edges ignored");

    std::mt19937 rng(31337);
    bool sizesMatch = true, valid = true, initIndependent = true;
    for (int round = 0; round < 300; ++round) {
        int leftCount = 1 + static_cast<int>(rng() % 25), rightCount = 1 + static_cast<int>(rng() % 25);
        int density = 1 + static_cast<int>(rng() % 6);
        std::vector<std::vector<int>> adj(leftCount);
        HopcroftKarp hk(leftCount, rightCount), hk2(leftCount, rightCount);
        for (int u = 0; u < leftCount; ++u) {
            for (int v = 0; v < rightCount; ++v) {
                if (static_cast<int>(rng() % 10) < density) {
                    adj[u].push_back(v);
                    hk.addEdge(u, v);
                    hk2.addEdge(u, v);
                }
            }
        }
        int expected = kuhnMatching(adj, rightCount);
        int withGreedy = hk.run(true);
        int withoutGreedy = hk2.run(false);
        if (withGreedy != expected) sizesMatch = false;
        if (withoutGreedy != expected) initIndependent = false;
        if (!hk.isValidMatching() || !hk2.isValidMatching()) valid = false;
    }
    check(sizesMatch, "300 random graphs: matching size equals Kuhn's maximum matching");
    check(initIndependent, "same size without the greedy start");
    check(valid, "every matching uses only real edges and is consistent from both sides");

    int n = 2000;
    HopcroftKarp big(n, n);
    for (int u = 0; u < n; ++u) {
        for (int k = 0; k < 4; ++k) big.addEdge(u, static_cast<int>(rng() % n));
    }
    int size = big.run(false);
    check(big.isValidMatching() && size > n / 2, "2000x2000 sparse random graph matched validly");
    check(big.phases() <= 2 * static_cast<int>(std::sqrt(2.0 * n)), "number of phases is within 2 * sqrt(V)");
}

int main() {
    testPriorityFormula();
    testHeapBasics();
    testHeapUpdates();
    testHeapRemove();
    testHeapTransferAndRank();
    testHeapRandomised();
    testSegmentTree();
    testDaryHeaps();
    testFibonacciHeap();
    testHeapsAgreeOnWorkloads();
    testBucketSegmentTree();
    testLazyAging();
    testStarvation();
    testHopcroftKarp();
    std::cout << "\nPassed: " << passed << "   Failed: " << failed << "\n";
    return failed == 0 ? 0 : 1;
}
