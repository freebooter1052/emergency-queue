#pragma once

#include <string>

constexpr int ESI_WEIGHT = 100;
constexpr int DETERIORATION_WEIGHT = 30;
constexpr int WAIT_BLOCK_MINUTES = 5;
constexpr int WAIT_BLOCK_WEIGHT = 10;

constexpr int MIN_ESI = 1;
constexpr int MAX_ESI = 5;
constexpr int MIN_DETERIORATION = 0;
constexpr int MAX_DETERIORATION = 4;
constexpr int NUM_ZONES = 5;

struct Patient {
    int id = 0;
    std::string name;
    int esi = MAX_ESI;
    int deteriorationLevel = 0;
    int waitingMinutes = 0;
    int zone = 1;
    int arrivalOrder = 0;
    int priorityScore = 0;
};

inline int calculatePriorityScore(const Patient& patient) {
    return (patient.esi - 1) * ESI_WEIGHT
         - patient.deteriorationLevel * DETERIORATION_WEIGHT
         - (patient.waitingMinutes / WAIT_BLOCK_MINUTES) * WAIT_BLOCK_WEIGHT;
}

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
