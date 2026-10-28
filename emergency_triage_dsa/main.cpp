#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "BedMatching.h"
#include "IndexedMinHeap.h"
#include "Patient.h"
#include "SegmentTree.h"

const int ZONE_CAPACITY[NUM_ZONES] = {2, 2, 3, 2, 2};

const char* const TEST_DATA_FILE = "test_patients.txt";

const int MAX_MINUTES_PER_STEP = 1440;

struct TriageSimulation {
    IndexedMinHeap waitingQueue;
    SegmentTree zoneTree;
    int nextArrivalOrder = 1;

    TriageSimulation() : zoneTree(NUM_ZONES) {
        std::vector<ZoneStats> initialZones(NUM_ZONES);
        for (int i = 0; i < NUM_ZONES; ++i) {
            initialZones[i].totalCapacity = ZONE_CAPACITY[i];
        }
        zoneTree.build(initialZones);
    }
};

int toTreeIndex(int zone) {
    return zone - 1;
}

void adjustZone(TriageSimulation& sim, int zone, int patientChange, int waitingTimeChange) {
    ZoneStats stats = sim.zoneTree.getZone(toTreeIndex(zone));
    stats.patientCount += patientChange;
    stats.totalWaitingTime += waitingTimeChange;
    sim.zoneTree.update(toTreeIndex(zone), stats);
}

int queuePosition(const TriageSimulation& sim, int patientId) {
    return sim.waitingQueue.rankOf(patientId);
}

std::string heapWorkNote(const TriageSimulation& sim) {
    return "[heap: " + std::to_string(sim.waitingQueue.lastOperationSwaps()) +
           " swap(s), tree height " + std::to_string(sim.waitingQueue.height()) + "]";
}

const int MIN_NAME_COLUMN_WIDTH = 14;

void printPatientRow(int position, const Patient& p, int nameWidth) {
    std::cout << "  " << std::right << std::setw(3) << position << " | "
              << std::setw(10) << p.id << " | "
              << std::left << std::setw(nameWidth) << p.name << " | "
              << std::right << std::setw(3) << p.esi << " | "
              << std::setw(13) << p.deteriorationLevel << " | "
              << std::setw(10) << p.waitingMinutes << " | "
              << std::setw(4) << p.zone << " | "
              << std::setw(14) << p.priorityScore << "\n";
}

void printQueue(const TriageSimulation& sim) {
    std::vector<Patient> ordered = sim.waitingQueue.getAllPatientsSorted();
    std::cout << "  Waiting queue in attend order (sorted copy of the heap, O(n log n)):\n";
    if (ordered.empty()) {
        std::cout << "  (the waiting queue is empty)\n";
        return;
    }
    int nameWidth = MIN_NAME_COLUMN_WIDTH;
    for (const Patient& p : ordered) {
        nameWidth = std::max(nameWidth, static_cast<int>(p.name.size()));
    }
    std::cout << "  Pos | Patient ID | " << std::left << std::setw(nameWidth) << "Name"
              << " | ESI | Deterioration | Wait (min) | Zone | Priority Score\n";
    std::cout << "  ----+------------+" << std::string(nameWidth + 2, '-')
              << "+-----+---------------+------------+------+---------------\n";
    for (size_t i = 0; i < ordered.size(); ++i) {
        printPatientRow(static_cast<int>(i) + 1, ordered[i], nameWidth);
    }
}

void printBedAssignments(const TriageSimulation& sim) {
    std::vector<Patient> patients = sim.waitingQueue.getAllPatientsSorted();
    const std::vector<int> bedZones = {1, 2, 3, 4, 5};
    HopcroftKarp matching(static_cast<int>(patients.size()), static_cast<int>(bedZones.size()));

    for (int patientIndex = 0; patientIndex < static_cast<int>(patients.size()); ++patientIndex) {
        for (int bedIndex = 0; bedIndex < static_cast<int>(bedZones.size()); ++bedIndex) {
            if (patients[patientIndex].zone == bedZones[bedIndex]) {
                matching.addEdge(patientIndex, bedIndex);
            }
        }
    }

    int matched = matching.run(true);
    std::cout << "  Available beds are zone-compatible: " << bedZones.size()
              << " bed(s), one in each zone. Hopcroft-Karp matching: " << matched
              << "/" << patients.size() << " patient(s) assigned, "
              << matching.phases() << " augmenting phase(s).\n";
    std::cout << "  Patient-to-bed assignments (patients are considered in priority order):\n";
    for (int patientIndex = 0; patientIndex < static_cast<int>(patients.size()); ++patientIndex) {
        int bedIndex = matching.bedOfPatient(patientIndex);
        std::cout << "    Patient " << patients[patientIndex].id << " (" << patients[patientIndex].name
                  << ", zone " << patients[patientIndex].zone << "): ";
        if (bedIndex == -1) {
            std::cout << "WAITING - no compatible bed available\n";
        } else {
            std::cout << "Bed B" << bedIndex + 1 << " (zone " << bedZones[bedIndex] << ")\n";
        }
    }
    std::cout << "  Matching validity: " << (matching.isValidMatching() ? "OK" : "FAILED") << "\n";
}

void printHeapTree(const TriageSimulation& sim) {
    std::vector<Patient> raw = sim.waitingQueue.getAllPatients();
    int count = static_cast<int>(raw.size());
    std::cout << "  Internal heap array drawn as a tree, written as [index] patientID(score):\n";
    if (count == 0) {
        std::cout << "    (empty)\n";
        return;
    }
    int levelStart = 0;
    int levelSize = 1;
    for (int level = 0; levelStart < count; ++level) {
        std::cout << "    Level " << level << ":";
        for (int i = levelStart; i < std::min(levelStart + levelSize, count); ++i) {
            std::cout << "  [" << i << "] " << raw[i].id << "(" << raw[i].priorityScore << ")";
        }
        std::cout << "\n";
        levelStart += levelSize;
        levelSize *= 2;
    }
    std::cout << "    Children of [i] are [2i+1] and [2i+2]. Each parent comes before its children,\n"
              << "    but the array is not fully sorted.\n";

    std::vector<int> ids;
    for (const Patient& p : raw) {
        ids.push_back(p.id);
    }
    std::sort(ids.begin(), ids.end());
    std::cout << "  ID -> index map:";
    for (int id : ids) {
        std::cout << "  " << id << "->" << sim.waitingQueue.indexOfPatient(id);
    }
    std::cout << "\n";
}

void printZoneRange(const TriageSimulation& sim, int fromZone, int toZone) {
    ZoneStats stats = sim.zoneTree.query(toTreeIndex(fromZone), toTreeIndex(toZone));

    if (fromZone == toZone) {
        std::cout << "  Zone " << fromZone;
    } else {
        std::cout << "  Zones " << fromZone << " to " << toZone;
    }
    std::cout << " (segment tree range query, O(log k)):\n";
    std::cout << "    Waiting patients     : " << stats.patientCount << "\n";
    std::cout << "    Total capacity       : " << stats.totalCapacity << "\n";
    if (stats.patientCount <= stats.totalCapacity) {
        std::cout << "    Available capacity   : " << stats.totalCapacity - stats.patientCount << "\n";
    } else {
        std::cout << "    Available capacity   : 0 (OVER CAPACITY by "
                  << stats.patientCount - stats.totalCapacity << ")\n";
    }
    std::cout << "    Total waiting time   : " << stats.totalWaitingTime << " min\n";
    if (stats.patientCount == 0) {
        std::cout << "    Average waiting time : n/a (no waiting patients)\n";
    } else {
        double averageWait = static_cast<double>(stats.totalWaitingTime) / stats.patientCount;
        std::cout << "    Average waiting time : " << std::fixed << std::setprecision(2)
                  << averageWait << " min\n";
        std::cout.unsetf(std::ios::floatfield);
    }
    if (fromZone != toZone && stats.busiestZone != -1) {
        ZoneStats busiest = sim.zoneTree.getZone(stats.busiestZone);
        std::cout << "    Most crowded zone    : zone " << stats.busiestZone + 1 << " ("
                  << busiest.patientCount << " waiting / " << busiest.totalCapacity
                  << " capacity, load " << (stats.busiestLoad > 0 ? "+" : "")
                  << stats.busiestLoad << ")\n";
    }
}

void printConsistencyCheck(const TriageSimulation& sim) {
    bool heapOk = sim.waitingQueue.isValid();

    std::vector<int> expectedCount(NUM_ZONES, 0);
    std::vector<int> expectedWaiting(NUM_ZONES, 0);
    for (const Patient& p : sim.waitingQueue.getAllPatients()) {
        expectedCount[toTreeIndex(p.zone)] += 1;
        expectedWaiting[toTreeIndex(p.zone)] += p.waitingMinutes;
    }
    bool zonesOk = true;
    for (int i = 0; i < NUM_ZONES; ++i) {
        ZoneStats stats = sim.zoneTree.getZone(i);
        if (stats.patientCount != expectedCount[i] || stats.totalWaitingTime != expectedWaiting[i] ||
            stats.totalCapacity != ZONE_CAPACITY[i]) {
            zonesOk = false;
        }
    }
    std::cout << "  [check] heap property + ID->index map: " << (heapOk ? "OK" : "FAILED")
              << " | zone statistics match waiting queue: " << (zonesOk ? "OK" : "FAILED") << "\n";
}

bool admitPatient(TriageSimulation& sim, int id, const std::string& name, int esi,
                  int deteriorationLevel, int zone) {
    if (id <= 0) {
        std::cout << "  Error: patient ID must be a positive whole number.\n";
        return false;
    }
    if (sim.waitingQueue.contains(id)) {
        std::cout << "  Error: patient ID " << id << " is already waiting. Duplicate rejected.\n";
        return false;
    }
    if (name.empty()) {
        std::cout << "  Error: patient name cannot be empty.\n";
        return false;
    }
    if (!isValidESI(esi)) {
        std::cout << "  Error: ESI must be between " << MIN_ESI << " and " << MAX_ESI << ".\n";
        return false;
    }
    if (!isValidDeterioration(deteriorationLevel)) {
        std::cout << "  Error: deterioration level must be between " << MIN_DETERIORATION
                  << " and " << MAX_DETERIORATION << ".\n";
        return false;
    }
    if (!isValidZone(zone)) {
        std::cout << "  Error: zone must be between 1 and " << NUM_ZONES << ".\n";
        return false;
    }

    Patient patient;
    patient.id = id;
    patient.name = name;
    patient.esi = esi;
    patient.deteriorationLevel = deteriorationLevel;
    patient.waitingMinutes = 0;
    patient.zone = zone;
    patient.arrivalOrder = sim.nextArrivalOrder;

    if (!sim.waitingQueue.insert(patient)) {
        std::cout << "  Error: the patient could not be inserted.\n";
        return false;
    }
    sim.nextArrivalOrder++;
    adjustZone(sim, zone, +1, 0);

    Patient stored;
    sim.waitingQueue.getPatient(id, stored);
    std::cout << "  Added patient " << id << " (" << name << "), ESI " << esi
              << ", deterioration " << deteriorationLevel << ", zone " << zone
              << " -> priority score " << stored.priorityScore << "  " << heapWorkNote(sim) << "\n";
    return true;
}

bool updateConditionAndReport(TriageSimulation& sim, int id, int newESI, int newDeteriorationLevel) {
    Patient before;
    if (!sim.waitingQueue.getPatient(id, before)) {
        std::cout << "  Error: no waiting patient has ID " << id << ".\n";
        return false;
    }
    if (!isValidESI(newESI)) {
        std::cout << "  Error: ESI must be between " << MIN_ESI << " and " << MAX_ESI << ".\n";
        return false;
    }
    if (!isValidDeterioration(newDeteriorationLevel)) {
        std::cout << "  Error: deterioration level must be between " << MIN_DETERIORATION
                  << " and " << MAX_DETERIORATION << ".\n";
        return false;
    }

    int oldPosition = queuePosition(sim, id);
    sim.waitingQueue.updatePatient(id, newESI, newDeteriorationLevel);
    Patient after;
    sim.waitingQueue.getPatient(id, after);
    int newPosition = queuePosition(sim, id);

    std::cout << "  Patient " << id << " (" << after.name << ")\n";
    std::cout << "    ESI           : " << before.esi << " -> " << after.esi << "\n";
    std::cout << "    Deterioration : " << before.deteriorationLevel << " -> "
              << after.deteriorationLevel << "\n";
    std::cout << "    Waiting time  : " << after.waitingMinutes << " min (unchanged)\n";
    std::cout << "    Priority score: " << before.priorityScore << " -> " << after.priorityScore;
    if (after.priorityScore < before.priorityScore) {
        std::cout << "  (lower score, so the heap repaired upward with sift-up)\n";
    } else if (after.priorityScore > before.priorityScore) {
        std::cout << "  (higher score, so the heap repaired downward with sift-down)\n";
    } else {
        std::cout << "  (score unchanged, no movement needed)\n";
    }
    std::cout << "    Heap work     : " << heapWorkNote(sim) << "\n";
    std::cout << "    Queue position: " << oldPosition << " -> " << newPosition;
    if (newPosition < oldPosition) {
        std::cout << "  (moved UP the queue)\n";
    } else if (newPosition > oldPosition) {
        std::cout << "  (moved DOWN the queue)\n";
    } else {
        std::cout << "  (position unchanged)\n";
    }
    return true;
}

bool advanceTimeAndReport(TriageSimulation& sim, int minutes) {
    if (minutes < 1 || minutes > MAX_MINUTES_PER_STEP) {
        std::cout << "  Error: minutes must be between 1 and " << MAX_MINUTES_PER_STEP << ".\n";
        return false;
    }

    std::map<int, int> oldScore;
    for (const Patient& p : sim.waitingQueue.getAllPatients()) {
        oldScore[p.id] = p.priorityScore;
    }

    sim.waitingQueue.advanceWaitingTime(minutes);

    for (int zone = 1; zone <= NUM_ZONES; ++zone) {
        int patientsInZone = sim.zoneTree.getZone(toTreeIndex(zone)).patientCount;
        adjustZone(sim, zone, 0, minutes * patientsInZone);
    }

    std::cout << "  Advanced simulated time by " << minutes << " min for "
              << sim.waitingQueue.size() << " waiting patient(s).\n";
    for (const Patient& p : sim.waitingQueue.getAllPatientsSorted()) {
        std::cout << "    Patient " << p.id << ": wait now " << p.waitingMinutes
                  << " min, score " << oldScore[p.id] << " -> " << p.priorityScore << "\n";
    }
    std::cout << "  Cost: all n scores changed, so the heap did O(n) work (n updates + one\n"
              << "  bottom-up rebuild, " << sim.waitingQueue.lastOperationSwaps()
              << " swap(s) this time) and the segment tree did " << NUM_ZONES
              << " point updates, O(k log k).\n";
    return true;
}

bool attendNextAndReport(TriageSimulation& sim) {
    Patient next;
    if (!sim.waitingQueue.extractMin(next)) {
        std::cout << "  The waiting queue is empty. There is no patient to attend.\n";
        return false;
    }

    adjustZone(sim, next.zone, -1, -next.waitingMinutes);

    std::cout << "  Attending patient " << next.id << " (" << next.name << ")\n";
    std::cout << "    ESI " << next.esi << ", deterioration " << next.deteriorationLevel
              << ", waited " << next.waitingMinutes << " min, zone " << next.zone
              << ", priority score " << next.priorityScore << "\n";
    std::cout << "  Removed from the heap and from zone " << next.zone << "'s statistics. "
              << sim.waitingQueue.size() << " patient(s) still waiting.  " << heapWorkNote(sim) << "\n";
    return true;
}

bool removePatientAndReport(TriageSimulation& sim, int id) {
    if (!sim.waitingQueue.contains(id)) {
        std::cout << "  Error: no waiting patient has ID " << id << ".\n";
        return false;
    }

    int holeIndex = sim.waitingQueue.indexOfPatient(id);
    int lastLeafId = sim.waitingQueue.getAllPatients().back().id;

    Patient removed;
    sim.waitingQueue.removePatient(id, removed);
    adjustZone(sim, removed.zone, -1, -removed.waitingMinutes);

    std::cout << "  Patient " << removed.id << " (" << removed.name << ", score "
              << removed.priorityScore << ") left without being seen. Hole at index ["
              << holeIndex << "].\n";
    if (lastLeafId != removed.id) {
        int finalIndex = sim.waitingQueue.indexOfPatient(lastLeafId);
        std::cout << "  Last leaf " << lastLeafId << " filled the hole and ";
        if (finalIndex < holeIndex) {
            std::cout << "moved UP to [" << finalIndex << "] (sift-up).\n";
        } else if (finalIndex > holeIndex) {
            std::cout << "moved DOWN to [" << finalIndex << "] (sift-down).\n";
        } else {
            std::cout << "stayed at [" << finalIndex << "].\n";
        }
    }
    std::cout << "  Removed from the heap and from zone " << removed.zone << "'s statistics. "
              << sim.waitingQueue.size() << " patient(s) still waiting.  " << heapWorkNote(sim) << "\n";
    return true;
}

bool transferZoneAndReport(TriageSimulation& sim, int id, int newZone) {
    Patient before;
    if (!sim.waitingQueue.getPatient(id, before)) {
        std::cout << "  Error: no waiting patient has ID " << id << ".\n";
        return false;
    }
    if (!isValidZone(newZone)) {
        std::cout << "  Error: zone must be between 1 and " << NUM_ZONES << ".\n";
        return false;
    }
    if (newZone == before.zone) {
        std::cout << "  Patient " << id << " is already in zone " << newZone << ". Nothing changed.\n";
        return false;
    }

    sim.waitingQueue.transferZone(id, newZone);
    adjustZone(sim, before.zone, -1, -before.waitingMinutes);
    adjustZone(sim, newZone, +1, before.waitingMinutes);

    std::cout << "  Patient " << id << " (" << before.name << ") moved from zone " << before.zone
              << " to zone " << newZone << ", keeping " << before.waitingMinutes << " min of waiting time.\n";
    std::cout << "  Priority score " << before.priorityScore
              << " and queue position are unchanged (zone is not part of the formula).  "
              << heapWorkNote(sim) << "\n";
    std::cout << "  Segment tree: 2 point updates (zone " << before.zone << " -1, zone " << newZone
              << " +1), O(log k) each.\n";
    return true;
}

bool readInt(const std::string& prompt, int& value) {
    while (true) {
        std::cout << prompt;
        std::string line;
        if (!std::getline(std::cin, line)) {
            return false;
        }
        std::stringstream parser(line);
        int number;
        char extra;
        if (parser >> number && !(parser >> extra)) {
            value = number;
            return true;
        }
        std::cout << "  Please enter a whole number.\n";
    }
}

bool readLine(const std::string& prompt, std::string& text) {
    std::cout << prompt;
    if (!std::getline(std::cin, text)) {
        return false;
    }
    size_t first = text.find_first_not_of(" \t\r");
    size_t last = text.find_last_not_of(" \t\r");
    text = (first == std::string::npos) ? "" : text.substr(first, last - first + 1);
    return true;
}

std::vector<std::string> splitCsvLine(const std::string& line) {
    std::vector<std::string> fields;
    std::stringstream stream(line);
    std::string field;
    while (std::getline(stream, field, ',')) {
        size_t first = field.find_first_not_of(" \t\r");
        size_t last = field.find_last_not_of(" \t\r");
        fields.push_back(first == std::string::npos ? "" : field.substr(first, last - first + 1));
    }
    return fields;
}

bool parseWholeNumber(const std::string& text, int& value) {
    std::stringstream parser(text);
    char extra;
    return (parser >> value) && !(parser >> extra);
}

void loadPatientsFromFile(TriageSimulation& sim, const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        std::cout << "  Error: cannot open '" << path
                  << "'. Run the program from the folder that contains the file.\n";
        return;
    }
    std::cout << "  Loading test data from '" << path << "'...\n";
    std::string line;
    int lineNumber = 0, added = 0, rejected = 0;
    while (std::getline(file, line)) {
        ++lineNumber;
        size_t firstChar = line.find_first_not_of(" \t\r");
        if (firstChar == std::string::npos || line[firstChar] == '#') {
            continue;
        }
        std::vector<std::string> fields = splitCsvLine(line);
        int id, esi, deterioration, zone;
        std::cout << "  Line " << lineNumber << ": ";
        if (fields.size() != 5 || !parseWholeNumber(fields[0], id) ||
            !parseWholeNumber(fields[2], esi) || !parseWholeNumber(fields[3], deterioration) ||
            !parseWholeNumber(fields[4], zone)) {
            std::cout << "skipped, expected 'id, name, esi, deterioration, zone'.\n";
            ++rejected;
            continue;
        }
        if (admitPatient(sim, id, fields[1], esi, deterioration, zone)) {
            ++added;
        } else {
            ++rejected;
        }
    }
    std::cout << "  Finished: " << added << " patient(s) added, " << rejected << " line(s) rejected.\n";
    printConsistencyCheck(sim);
}

void handleAddPatient(TriageSimulation& sim) {
    int mode;
    std::cout << "  1) Enter one patient manually\n"
              << "  2) Load test data set from " << TEST_DATA_FILE << "\n"
              << "  3) Load patients from another file\n";
    if (!readInt("Choose (1-3): ", mode)) return;
    if (mode == 2) {
        loadPatientsFromFile(sim, TEST_DATA_FILE);
        return;
    }
    if (mode == 3) {
        std::string path;
        if (!readLine("File path: ", path)) return;
        loadPatientsFromFile(sim, path);
        return;
    }
    if (mode != 1) {
        std::cout << "  Invalid choice.\n";
        return;
    }

    int id, esi, deterioration, zone;
    std::string name;
    if (!readInt("Patient ID: ", id)) return;
    if (sim.waitingQueue.contains(id)) {
        std::cout << "  Error: patient ID " << id << " is already waiting. Duplicate rejected.\n";
        return;
    }
    if (!readLine("Patient name: ", name)) return;
    if (!readInt("ESI category (1-5): ", esi)) return;
    if (!readInt("Deterioration level (0-4): ", deterioration)) return;
    if (!readInt("Zone (1-5): ", zone)) return;
    admitPatient(sim, id, name, esi, deterioration, zone);
}

void handleUpdateCondition(TriageSimulation& sim) {
    int id, esi, deterioration;
    if (!readInt("Patient ID: ", id)) return;
    if (!readInt("New ESI category (1-5): ", esi)) return;
    if (!readInt("New deterioration level (0-4): ", deterioration)) return;
    updateConditionAndReport(sim, id, esi, deterioration);
}

void handleAdvanceTime(TriageSimulation& sim) {
    int minutes;
    if (!readInt("Minutes passed: ", minutes)) return;
    advanceTimeAndReport(sim, minutes);
}

void handleRemovePatient(TriageSimulation& sim) {
    int id;
    if (!readInt("Patient ID who left: ", id)) return;
    removePatientAndReport(sim, id);
}

void handleTransferZone(TriageSimulation& sim) {
    int id, zone;
    if (!readInt("Patient ID: ", id)) return;
    if (!readInt("New zone (1-5): ", zone)) return;
    transferZoneAndReport(sim, id, zone);
}

void handleBedAssignments(const TriageSimulation& sim) {
    printBedAssignments(sim);
}

void handleZoneQuery(TriageSimulation& sim) {
    int fromZone, toZone;
    if (!readInt("From zone (1-5): ", fromZone)) return;
    if (!readInt("To zone (1-5): ", toZone)) return;
    if (!isValidZone(fromZone) || !isValidZone(toZone) || fromZone > toZone) {
        std::cout << "  Error: enter zones between 1 and " << NUM_ZONES
                  << " with the first zone not greater than the second.\n";
        return;
    }
    printZoneRange(sim, fromZone, toZone);
}

void printStepHeading(const std::string& title) {
    std::cout << "\n------------------------------------------------------------------\n"
              << title
              << "\n------------------------------------------------------------------\n";
}

void runDemonstration() {
    TriageSimulation sim;

    std::cout << "\n=============== PREDEFINED TRIAGE DEMONSTRATION ===============\n";
    std::cout << "All patients are fictional. The priority formula is a teaching example,\n"
              << "not a validated medical triage formula. Lower score = attended earlier.\n";
    std::cout << "Zone capacities:";
    for (int zone = 1; zone <= NUM_ZONES; ++zone) {
        std::cout << "  zone " << zone << " = " << ZONE_CAPACITY[zone - 1];
    }
    std::cout << "\n";

    printStepHeading("STEP 1: Patients arrive in two waves (heap insert, O(log n) each)");
    std::cout << "First wave arrives at minute 0:\n";
    admitPatient(sim, 101, "Asha Menon", 3, 0, 2);
    admitPatient(sim, 102, "Ben Carter", 4, 0, 3);
    admitPatient(sim, 103, "Chen Li", 2, 0, 1);
    std::cout << "\n30 minutes pass before the second wave arrives:\n";
    advanceTimeAndReport(sim, 30);
    std::cout << "\nSecond wave arrives at minute 30:\n";
    admitPatient(sim, 104, "Diya Rao", 3, 0, 2);
    admitPatient(sim, 105, "Ethan Brooks", 5, 0, 4);
    admitPatient(sim, 106, "Farah Khan", 3, 1, 2);
    admitPatient(sim, 107, "Gabriel Ortiz", 4, 0, 3);
    printConsistencyCheck(sim);

    printStepHeading("STEP 2: Initial queue order");
    printQueue(sim);
    std::cout << "\n";
    printHeapTree(sim);
    printConsistencyCheck(sim);

    printStepHeading("STEP 3: Gabriel Ortiz deteriorates (priority update, O(log n))");
    std::cout << "Re-triaged from ESI 4 to ESI 2 with deterioration level 3.\n"
              << "The index map finds him in O(1); then only his path is repaired.\n";
    updateConditionAndReport(sim, 107, 2, 3);
    std::cout << "\n";
    printHeapTree(sim);
    printConsistencyCheck(sim);

    printStepHeading("STEP 4: Chen Li stabilises (sift-down and the tie-break rule)");
    std::cout << "Re-triaged from ESI 2 to ESI 3, deterioration stays 0.\n";
    updateConditionAndReport(sim, 103, 3, 0);
    Patient asha, chen;
    sim.waitingQueue.getPatient(101, asha);
    sim.waitingQueue.getPatient(103, chen);
    std::cout << "  Tie-break: " << asha.name << " (score " << asha.priorityScore << ", arrival "
              << asha.arrivalOrder << ") vs " << chen.name << " (score " << chen.priorityScore
              << ", arrival " << chen.arrivalOrder << ").\n"
              << "  With equal scores, the earlier arrival is attended first.\n\n";
    printQueue(sim);
    printConsistencyCheck(sim);

    printStepHeading("STEP 5: Waiting-time aging (advance time, O(n) for all patients)");
    advanceTimeAndReport(sim, 40);
    std::cout << "\n";
    printQueue(sim);
    std::cout << "\nNote: everyone aged by the same 40 minutes, so all scores dropped by the\n"
              << "same 80 points and the order among these patients did not change. Aging\n"
              << "matters when long-waiting patients are compared with NEW arrivals:\n\n";
    admitPatient(sim, 108, "Hana Sato", 3, 0, 2);
    Patient ben, hana;
    sim.waitingQueue.getPatient(102, ben);
    sim.waitingQueue.getPatient(108, hana);
    std::cout << "  " << ben.name << ": ESI " << ben.esi << ", waited " << ben.waitingMinutes
              << " min, score " << ben.priorityScore << " (position " << queuePosition(sim, 102)
              << ")\n";
    std::cout << "  " << hana.name << ": ESI " << hana.esi << ", waited " << hana.waitingMinutes
              << " min, score " << hana.priorityScore << " (position " << queuePosition(sim, 108)
              << ")\n";
    std::cout << "  Because his score was kept up to date, " << ben.name
              << " (less urgent ESI) is not overtaken by the new arrival.\n\n";
    printQueue(sim);
    printConsistencyCheck(sim);

    printStepHeading("STEP 6: Attend the highest-priority patient (extractMin, O(log n))");
    std::cout << "Zone 3 before:\n";
    printZoneRange(sim, 3, 3);
    std::cout << "\n";
    attendNextAndReport(sim);
    std::cout << "\nZone 3 after (patient count and waiting-time sum reduced):\n";
    printZoneRange(sim, 3, 3);
    std::cout << "\n";
    printQueue(sim);
    printConsistencyCheck(sim);

    printStepHeading("STEP 7: Patients leave without being seen (removePatient, O(log n))");
    std::cout << "Removing from the MIDDLE of the heap: the last leaf fills the hole, then it may\n"
              << "need to move DOWN or UP. extractMin only ever needs to move down.\n\n";
    printHeapTree(sim);
    std::cout << "\n";
    removePatientAndReport(sim, 104);
    std::cout << "\n";
    printHeapTree(sim);
    std::cout << "\n";
    removePatientAndReport(sim, 105);
    std::cout << "\n";
    printHeapTree(sim);
    printConsistencyCheck(sim);

    printStepHeading("STEP 8: Zone statistics with segment tree range queries");
    printZoneRange(sim, 2, 4);
    std::cout << "\n";
    printZoneRange(sim, 2, 2);
    std::cout << "\n";
    printZoneRange(sim, 5, 5);
    std::cout << "\n";
    printZoneRange(sim, 1, 5);
    printConsistencyCheck(sim);

    printStepHeading("STEP 9: Relieve the crowded zone (zone transfer, O(log k) tree work)");
    std::cout << "Zone 2 is over capacity and zone 5 is empty, so Hana Sato is moved.\n"
              << "The heap does not reorder: only her zone field changes (O(1) via the map).\n\n";
    transferZoneAndReport(sim, 108, 5);
    std::cout << "\n";
    printZoneRange(sim, 2, 2);
    std::cout << "\n";
    printZoneRange(sim, 1, 5);
    printConsistencyCheck(sim);

    printStepHeading("STEP 10: Assign available beds (Hopcroft-Karp maximum matching)");
    printBedAssignments(sim);

    std::cout << "\n=============== DEMONSTRATION COMPLETE ===============\n";
}

void printMenu() {
    std::cout << "\n========== EMERGENCY TRIAGE DSA ==========\n\n"
              << "1. Add a patient (manually or load test data file)\n"
              << "2. Update patient condition\n"
              << "3. Advance simulated time\n"
              << "4. Attend to the highest-priority patient\n"
              << "5. Display waiting queue\n"
              << "6. Query zone statistics\n"
              << "7. Run predefined demonstration\n"
              << "8. Remove a patient (left without being seen)\n"
              << "9. Show heap as a tree + ID->index map\n"
              << "10. Transfer a patient to another zone\n"
              << "11. Assign available beds (Hopcroft-Karp matching)\n"
              << "0. Exit\n\n";
}

int main(int argc, char* argv[]) {
    if (argc > 1) {
        if (std::string(argv[1]) == "--demo") {
            runDemonstration();
            return 0;
        }
        std::cout << "Usage: triage_demo [--demo]\n";
        return 1;
    }

    TriageSimulation sim;
    while (true) {
        printMenu();
        int choice;
        if (!readInt("Enter your choice: ", choice)) {
            std::cout << "\nEnd of input. Exiting.\n";
            return 0;
        }
        std::cout << "\n";
        switch (choice) {
            case 1: handleAddPatient(sim); break;
            case 2: handleUpdateCondition(sim); break;
            case 3: handleAdvanceTime(sim); break;
            case 4: attendNextAndReport(sim); break;
            case 5: printQueue(sim); break;
            case 6: handleZoneQuery(sim); break;
            case 7: runDemonstration(); break;
            case 8: handleRemovePatient(sim); break;
            case 9: printHeapTree(sim); break;
            case 10: handleTransferZone(sim); break;
            case 11: handleBedAssignments(sim); break;
            case 0: std::cout << "Goodbye.\n"; return 0;
            default: std::cout << "  Invalid choice. Enter a number from 0 to 11.\n"; break;
        }
    }
}
