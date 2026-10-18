#pragma once

// Patient.h
// Patient record and the demonstration priority formula.
//
// IMPORTANT: the formula below is a teaching example for dynamic priorities.
// It is NOT a validated medical triage formula.

#include <string>

// ---- Demonstration weights (change these to experiment with the formula) ----
constexpr int ESI_WEIGHT = 100;           // points added per ESI level above 1
constexpr int DETERIORATION_WEIGHT = 30;  // points removed per deterioration level
constexpr int WAIT_BLOCK_MINUTES = 5;     // waiting time counts in 5-minute blocks
constexpr int WAIT_BLOCK_WEIGHT = 10;     // points removed per completed block

// ---- Valid input ranges ----
constexpr int MIN_ESI = 1;
constexpr int MAX_ESI = 5;
constexpr int MIN_DETERIORATION = 0;
constexpr int MAX_DETERIORATION = 4;
constexpr int NUM_ZONES = 5;  // zones are numbered 1..NUM_ZONES in the user interface

struct Patient {
    int id = 0;
    std::string name;
    int esi = MAX_ESI;           // 1 = most urgent, 5 = least urgent
    int deteriorationLevel = 0;  // 0 = stable, 4 = deteriorating fastest (demo scale)
    int waitingMinutes = 0;      // simulated minutes spent waiting
    int zone = 1;                // department zone, 1..NUM_ZONES
    int arrivalOrder = 0;        // 1 for the first arrival, 2 for the next, ...
    int priorityScore = 0;       // cached result of calculatePriorityScore()
};

// Lower score = seen earlier.
//   priorityScore = (esi - 1) * 100 - deteriorationLevel * 30 - (waitingMinutes / 5) * 10
// waitingMinutes / 5 is integer division, so only completed 5-minute blocks count.
inline int calculatePriorityScore(const Patient& patient) {
    return (patient.esi - 1) * ESI_WEIGHT
         - patient.deteriorationLevel * DETERIORATION_WEIGHT
         - (patient.waitingMinutes / WAIT_BLOCK_MINUTES) * WAIT_BLOCK_WEIGHT;
}

// Returns true if patient a should be attended before patient b.
// This is the single comparison rule used by the heap.
// Equal scores are broken by arrival order: the earlier arrival goes first.
inline bool isHigherPriority(const Patient& a, const Patient& b) {
    if (a.priorityScore != b.priorityScore) {
        return a.priorityScore < b.priorityScore;
    }
    return a.arrivalOrder < b.arrivalOrder;
}

inline bool isValidESI(int esi) {
    return esi >= MIN_ESI && esi <= MAX_ESI;
}

inline bool isValidDeterioration(int level) {
    return level >= MIN_DETERIORATION && level <= MAX_DETERIORATION;
}

inline bool isValidZone(int zone) {
    return zone >= 1 && zone <= NUM_ZONES;
}
