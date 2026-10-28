#pragma once

#include <algorithm>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "HeapKey.h"
#include "Patient.h"

// Triage-style operation streams for the heap benchmark.
//
// One stream is generated per (workload, seed) and replayed unchanged on every heap, so
// all structures see exactly the same inserts, deteriorations and extractions. The
// generator keeps a reference std::set while generating, so each ExtractMin records the
// ID that MUST come out; the benchmark checks every heap against it.

enum class OpType { Insert, DecreaseKey, ExtractMin };

struct Op {
    OpType type;
    int id;
    HeapKey key;
    int expectedId;
};

// A phase covers `fraction` of the operations. Within it:
//   P(decrease-key) = decreaseRatio                      (same for every phase)
//   P(insert)       = (1 - decreaseRatio) * insertShare
//   P(extract-min)  = (1 - decreaseRatio) * (1 - insertShare)
// `severeArrivals` skews the ESI mix toward ESI 1-2, as in a mass-casualty event.
struct Phase {
    double fraction;
    double insertShare;
    bool severeArrivals;
};

struct WorkloadSpec {
    std::string name;
    int preload = 10000;
    int operations = 300000;
    double decreaseRatio = 0.3;
    int maxScoreDrop = 120;   // a deterioration lowers the score by 1..maxScoreDrop points
    std::vector<Phase> phases;
    unsigned seed = 1;
};

struct Workload {
    std::vector<Op> preload;
    std::vector<Op> operations;
    int maxId = 0;
    int insertCount = 0;
    int decreaseCount = 0;
    int extractCount = 0;
    int peakQueueSize = 0;
};

inline WorkloadSpec steadyWorkload(double decreaseRatio) {
    WorkloadSpec spec;
    spec.name = "steady arrivals";
    spec.decreaseRatio = decreaseRatio;
    spec.phases = {{1.0, 0.5, false}};
    return spec;
}

// Normal flow, then a sudden arrival surge, then the department works the backlog down.
inline WorkloadSpec burstWorkload(double decreaseRatio) {
    WorkloadSpec spec;
    spec.name = "mass-casualty burst";
    spec.decreaseRatio = decreaseRatio;
    spec.phases = {{0.4, 0.5, false}, {0.2, 0.85, true}, {0.4, 0.2, false}};
    return spec;
}

inline Workload generateWorkload(const WorkloadSpec& spec) {
    std::mt19937 rng(spec.seed);
    auto uniform = [&](int low, int high) {
        return std::uniform_int_distribution<int>(low, high)(rng);
    };
    auto chance = [&](double p) { return std::uniform_real_distribution<double>(0, 1)(rng) < p; };

    Workload workload;
    int totalInserts = spec.preload + spec.operations;
    std::vector<int> scoreOf(totalInserts, 0);
    std::vector<int> orderOf(totalInserts, 0);
    std::vector<int> slotOf(totalInserts, -1);
    std::vector<int> alive;
    std::set<std::pair<HeapKey, int>> reference;
    int nextId = 0;

    auto drawEsi = [&](bool severe) {
        // Cumulative percentages for ESI 1..5.
        static const int ordinary[5] = {3, 15, 55, 85, 100};
        static const int surge[5] = {20, 55, 80, 93, 100};
        int roll = uniform(1, 100);
        const int* table = severe ? surge : ordinary;
        for (int esi = 1; esi <= 5; ++esi) {
            if (roll <= table[esi - 1]) return esi;
        }
        return 5;
    };

    auto emitInsert = [&](std::vector<Op>& out, bool severe) {
        int id = nextId++;
        int deterioration = chance(0.2) ? uniform(1, 3) : 0;
        scoreOf[id] = (drawEsi(severe) - 1) * ESI_WEIGHT - deterioration * DETERIORATION_WEIGHT;
        orderOf[id] = id + 1;
        HeapKey key = makeHeapKey(scoreOf[id], orderOf[id]);
        reference.insert({key, id});
        slotOf[id] = static_cast<int>(alive.size());
        alive.push_back(id);
        out.push_back(Op{OpType::Insert, id, key, -1});
        ++workload.insertCount;
        workload.peakQueueSize = std::max(workload.peakQueueSize, static_cast<int>(alive.size()));
    };

    auto emitDecrease = [&](std::vector<Op>& out) {
        int id = alive[uniform(0, static_cast<int>(alive.size()) - 1)];
        HeapKey oldKey = makeHeapKey(scoreOf[id], orderOf[id]);
        scoreOf[id] -= uniform(1, spec.maxScoreDrop);
        HeapKey newKey = makeHeapKey(scoreOf[id], orderOf[id]);
        reference.erase({oldKey, id});
        reference.insert({newKey, id});
        out.push_back(Op{OpType::DecreaseKey, id, newKey, -1});
        ++workload.decreaseCount;
    };

    auto emitExtract = [&](std::vector<Op>& out) {
        int id = reference.begin()->second;
        reference.erase(reference.begin());
        int slot = slotOf[id];
        alive[slot] = alive.back();
        slotOf[alive[slot]] = slot;
        alive.pop_back();
        slotOf[id] = -1;
        out.push_back(Op{OpType::ExtractMin, id, 0, id});
        ++workload.extractCount;
    };

    for (int i = 0; i < spec.preload; ++i) {
        emitInsert(workload.preload, false);
    }
    workload.insertCount = 0;

    int issued = 0;
    for (const Phase& phase : spec.phases) {
        int phaseOps = static_cast<int>(phase.fraction * spec.operations + 0.5);
        for (int i = 0; i < phaseOps && issued < spec.operations; ++i, ++issued) {
            bool insertOp = chance((1 - spec.decreaseRatio) * phase.insertShare);
            bool decreaseOp = !insertOp && chance(spec.decreaseRatio /
                                                  (1 - (1 - spec.decreaseRatio) * phase.insertShare));
            if (alive.empty() || insertOp) {
                emitInsert(workload.operations, phase.severeArrivals);
            } else if (decreaseOp) {
                emitDecrease(workload.operations);
            } else {
                emitExtract(workload.operations);
            }
        }
    }
    while (issued < spec.operations) {
        emitInsert(workload.operations, false);
        ++issued;
    }
    workload.maxId = nextId;
    return workload;
}
