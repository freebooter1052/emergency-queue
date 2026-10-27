# Emergency Department Dynamic Patient Triage Engine
### Priority Heap + Segment Tree (C++17 DSA demonstrator)

> **Educational simulation only.** All patients are fictional. The priority formula is a
> made-up teaching example for dynamic priorities. It is **not** a validated medical
> triage formula and must not be used for real triage decisions.

---

## 1. Objective and problem

An emergency department has to keep answering two questions while things change all the time:

1. **Who should be attended next?** Patients arrive, their condition gets worse or better,
   and the longer they wait the more urgent they become.
2. **How busy is each part of the department?** For example: how many patients are waiting
   in zones 2 to 4, and is that more than those zones can hold?

This project answers them with two classic data structures:

| Question | Data structure | Key operations |
|---|---|---|
| Next patient, with priorities that change | **Indexed binary min-heap** (`IndexedMinHeap.h`) | insert, extractMin, update priority, remove by ID |
| Statistics for a range of zones | **Segment tree** (`SegmentTree.h`) | point update, range query (sums + most crowded zone) |

---

## 2. Files

```text
simple_triage_dsa/
├── main.cpp           Menu, predefined demo, and the code that keeps heap + tree in sync
├── Patient.h          Patient struct, priority formula, tie-break rule, input validation
├── IndexedMinHeap.h   The binary min-heap + patient-ID -> index map (written by hand)
├── SegmentTree.h      Segment tree over the 5 zones (count, waiting time, capacity)
├── README.md          This file
├── PROFESSOR_REVIEW.md  Review suggestions, what was changed and how to explain it
├── test_patients.txt  Test data set loaded by menu option 1 (editable)
├── sample_output.txt  Real output of `triage_demo --demo` (captured, not typed)
└── tests.cpp          Extra: unit tests for the heap and segment tree
```

`tests.cpp` was added beyond the required file list so the checks can be rerun. It is
not needed to build or run the demo.

--- 

## 3. Build and run on Windows

You need a GCC compiler (for example MinGW-w64). No other libraries are needed.

```powershell
cd simple_triage_dsa

# Build the program
g++ -std=c++17 -Wall -Wextra -O2 -static -o triage_demo.exe main.cpp

# Interactive menu
.\triage_demo.exe

# Predefined demonstration (same output every run)
.\triage_demo.exe --demo

# Optional: build and run the unit tests
g++ -std=c++17 -Wall -Wextra -O2 -static -o triage_tests.exe tests.cpp
.\triage_tests.exe
```

`-static` avoids a Windows problem where a different `libstdc++-6.dll` earlier in PATH (for example the one shipped with Git for Windows) makes file-reading programs crash. It is harmless otherwise.

On Linux or macOS use the same `g++` commands without `-static` and without `.exe`, then run `./triage_demo --demo`.

**Saving the demo output:** in Windows PowerShell 5.1, `>` writes UTF-16, which looks
odd in some editors. To get a plain text file, use:

```powershell
cmd /c ".\triage_demo.exe --demo > sample_output.txt"
```

---

## 4. Using the menu

```text
========== EMERGENCY TRIAGE DSA DEMO ==========
1. Add a patient (manually or load test data file)
2. Update patient condition
3. Advance simulated time
4. Attend to the highest-priority patient
5. Display waiting queue
6. Query zone statistics
7. Run predefined demonstration
8. Remove a patient (left without being seen)
9. Show heap as a tree + ID->index map
0. Exit
```

| Option | What you enter | What happens |
|---|---|---|
| 1 | First choose: `1` manual entry, `2` load `test_patients.txt`, `3` load another file. Manual entry asks for ID, name, ESI (1-5), deterioration (0-4), zone (1-5) | Heap `insert` + one zone point update. Duplicate IDs and out-of-range values are rejected with a message. Waiting time starts at 0. |
| 2 | ID, new ESI, new deterioration | Heap `updatePatient`. Shows the old and new score, the old and new queue position, and whether the patient moved up or down. Waiting time is not changed. |
| 3 | Minutes passed (1-1440) | Every waiting patient's wait increases, every score is recalculated, the heap is rebuilt, and each zone's waiting-time sum is updated. |
| 4 | – | Heap `extractMin`. The patient is removed from their zone's count and waiting-time sum. On an empty queue it prints a message. |
| 5 | – | Table of all waiting patients in attend order (heap sort on a **copy**, so the heap is untouched). |
| 6 | From zone, to zone | Segment tree range query: patients, capacity, available capacity (or how far over capacity), total and average wait, and for multi-zone ranges the most crowded zone. |
| 7 | – | Runs the predefined demonstration on a fresh simulation, so your menu data is kept. |
| 8 | ID of the patient who left | Heap `removePatient` from anywhere in the heap. Shows where the last leaf moved (up or down). The zone statistics are reduced. |
| 9 | – | Prints the heap array level by level as a tree, plus the ID -> index map. |

Every heap operation prints a note such as `[heap: 2 swap(s), tree height 3]`. A single sift
never makes more swaps than the tree height, which is floor(log2 n). This is the O(log n)
bound you can see for yourself.

### Test data file (`test_patients.txt`)

Run the program from the `simple_triage_dsa` folder, choose **1**, then **2**. Each line is
`id, name, esi, deterioration, zone`; blank lines and `#` comments are ignored. Patients go
through the same `admitPatient()` checks as manual entry, in file order. The file has 10
valid patients followed by 6 deliberately invalid lines (duplicate ID, bad ESI, bad
deterioration, bad zone, missing fields, non-numeric ID) that are rejected with a message.
Edit the file to make your own data set, or choose **3** to load a different file.

Typing something that is not a number at a number prompt asks you again.

---

## 5. The dynamic priority formula

```cpp
priorityScore = (esi - 1) * 100
              - deteriorationLevel * 30
              - (waitingMinutes / 5) * 10;
```

* **Lower score = attended earlier.**
* ESI sets the base level: ESI 1 starts at 0, ESI 5 starts at 400.
* Each deterioration level takes off 30 points.
* Each **completed** 5-minute block of waiting takes off 10 points (`/` is integer division).
* **Tie-break:** with equal scores, the patient with the smaller `arrivalOrder` (who arrived
  earlier) goes first. This is done in `isHigherPriority()` in `Patient.h`, the only
  comparison the heap uses.

The weights are named constants at the top of `Patient.h`, so you can change them easily.

### Worked example

Farah Khan in the demo after step 5: ESI 3, deterioration 1, waited 40 minutes.

```text
(3 - 1) * 100   = 200
- 1 * 30        = -30
- (40 / 5) * 10 = -80
-----------------------
priorityScore   =  90
```

If she had waited 43 minutes instead, `43 / 5 = 8` (integer division), so the answer is still 90.

---

## 6. How the indexed binary min-heap works

### Array layout

The heap is a complete binary tree stored in a `std::vector<Patient>`:

```text
parent(i) = (i - 1) / 2      left(i) = 2i + 1      right(i) = 2i + 2
```

**Heap property:** no child has higher priority than its parent. So `heap[0]` is always the
next patient to attend. The array is **not** fully sorted; only parent-before-child order matters.

### Why the patient-ID index map is needed

To update a patient (for example, "patient 107 has deteriorated") we first need to know
**where** that patient is in the array. Without help, that is a linear search: O(n).

`std::unordered_map<int, int> indexOf` stores `patientId -> current array index`, so the
lookup is O(1) on average and the whole update is O(log n). The same lookup makes
`removePatient(id)` possible.

The map must change whenever a patient moves. All movement goes through one function,
`swapPatients(i, j)`, which swaps the two slots **and** updates both map entries. That is
why the map never goes out of date. `insert` adds a map entry and `extractMin` erases one.

### Insert: `insert(patient)` – O(log n)
1. Calculate the score, append the patient at the end of the array, record its index.
2. `siftUp`: while the patient beats its parent, swap them. At most one swap per level.

### Extract: `extractMin()` – O(log n)
1. Remember `heap[0]` (the answer).
2. Swap the last element into the root, `pop_back`, erase the removed ID from the map.
3. `siftDown` from the root: swap with the higher-priority child until neither child beats it.

### Update one patient: `updatePatient(id, newESI, newDeterioration)` – O(log n)
1. Find the index with the map (O(1) average).
2. Change ESI and deterioration, recalculate the score.
3. `repairPosition(index)`: if the patient now beats its parent, `siftUp`; otherwise `siftDown`.
   Only one of the two can be needed, because only this node's key changed.

**Trace from the demo (step 3):** Gabriel Ortiz (ID 107) is at index 6 with score 300.
His score drops to 10. Parent of 6 is index 2 (score 140), so they swap. Parent of 2 is
index 0 (score 40), so they swap again. He is now at the root, after 2 swaps (tree height 2).

### Remove any patient: `removePatient(id)` – O(log n)
1. Find the hole with the map.
2. Move the last leaf into the hole, `pop_back`, erase the ID from the map.
3. `repairPosition(hole)`: the moved leaf came from a **different subtree**, so it may be
   better than its new parent (sift **up**) or worse than its new children (sift **down**).
   `extractMin` is the special case where the hole is the root, so only sift-down is possible.

Step 7 of the demo shows both: when Diya leaves, leaf 108 moves **down**; when Ethan leaves,
leaf 106 moves **up**.

### Advance time for everyone: `advanceWaitingTime(minutes)` – O(n)
Every patient's waiting time and score changes, so this is **not** one O(log n) update.
The code updates all n scores in one pass, then calls `buildHeap()`, the bottom-up
"heapify" that sifts down every internal node from the last one to the root.
Building a heap bottom-up costs O(n) in total. (Calling `updatePatient` once per patient
would also work, but it costs O(n log n).)

### Display: `getAllPatientsSorted()` – O(n log n)
**Heap sort:** copy the whole heap object (O(n)), then call `extractMin` on the copy n times.
The real heap is never reordered.

### Why aging is still needed even though the order seldom changes
Everyone waiting ages by the same number of minutes, so all scores fall by roughly the
same amount and the order **among existing patients** usually stays the same (it changes
only through the 5-minute rounding). Aging matters when long-waiting patients are compared
with **new arrivals**. Step 5 of the demo shows this: Ben Carter (ESI 4, waited 70 min,
score 160) stays ahead of the newly arrived Hana Sato (ESI 3, score 200). If the scores
were stale, new arrivals would keep overtaking patients who have waited a long time.

---

## 7. How the segment tree combines zone statistics

Each **leaf** is one zone and each **internal node** is the combination of its two children:

```cpp
struct ZoneStats { int patientCount; int totalWaitingTime; int totalCapacity;   // summed
                   int busiestZone;  int busiestLoad; };                         // max

combineStats(a, b) = { a.patientCount + b.patientCount,
                       a.totalWaitingTime + b.totalWaitingTime,
                       a.totalCapacity + b.totalCapacity,
                       the side with the larger load (patients - capacity), left side on ties }
```

A segment tree works for **any associative** combine operation, not only sums. The
"most crowded zone" field shows this with a max (argmax). See `PROFESSOR_REVIEW.md`.

Tree for k = 5 zones (internal indices 0-4, which are zones 1-5 in the menu):

```text
                    node1 [0..4]
                 /                \
          node2 [0..2]          node3 [3..4]
          /         \            /        \
   node4 [0..1]   node5 [2]   node6 [3]  node7 [4]
    /       \
node8 [0]  node9 [1]
```

**Index conversion:** zone `z` in the menu is index `z - 1` in the tree (`toTreeIndex()` in `main.cpp`).

### Point update – `update(index, values)` – O(log k)
Go down one root-to-leaf path, replace the leaf, then recombine each node on the way back
up. Only about log k nodes change.

### Range query – `query(left, right)` – O(log k)
At each node there are three cases:
* **No overlap** with [left, right]: contributes nothing (all zeros).
* **Full overlap:** return the stored total straight away, without going deeper.
* **Partial overlap:** ask both children and combine the answers.

At most about 4 nodes are visited per level, and there are O(log k) levels.
Example: zones 2-4 = indices [1..3] is answered by node9 + node5 + node6.

The program then works out:
* `available = capacity - patients`, or "OVER CAPACITY by `patients - capacity`" when demand is higher.
* `averageWait = totalWaitingTime / patientCount`, or "n/a" when the count is 0 (no division by zero).

### Keeping the tree in sync with the heap
The heap is the authoritative list of waiting patients. The tree is updated in exactly four places:

| Event | Zone change |
|---|---|
| Patient added (option 1) | that zone: count +1, waiting time +0 |
| Patient attended (option 4) | that zone: count −1, waiting time −(their wait) |
| Patient left without being seen (option 8) | that zone: count −1, waiting time −(their wait) |
| Time advanced by m minutes (option 3) | every zone: waiting time + m × (its patient count) |

Changing a patient's condition (option 2) does not change any zone statistics.
After every step, the demo prints a `[check]` line. It recomputes the zone totals by
scanning the heap and compares them with the tree. This scan is used **only** for that
check; menu queries never scan patients.

---

## 8. Time complexity

n = number of waiting patients, k = number of zones (5 here).

| Operation | Function | Time |
|---|---|---|
| Insert patient | `IndexedMinHeap::insert` | **O(log n)** |
| Attend next patient | `IndexedMinHeap::extractMin` | **O(log n)** |
| Update ONE patient's priority | `IndexedMinHeap::updatePatient` | **O(log n)** (O(1) average map lookup + one sift) |
| Remove any patient by ID | `IndexedMinHeap::removePatient` | **O(log n)** |
| Look up a patient by ID | `IndexedMinHeap::getPatient` | O(1) average |
| Advance time for ALL patients | `advanceWaitingTime` + zone updates | **O(n)** heap + O(k log k) tree, *not* O(log n) |
| Display queue (heap sort on a copy) | `getAllPatientsSorted` | O(n log n) |
| Zone point update | `SegmentTree::update` | **O(log k)** |
| Zone range query | `SegmentTree::query` | **O(log k)** |
| Build segment tree | `SegmentTree::build` | O(k) |

Memory: O(n) for the heap and map, O(k) for the tree (4k nodes).

**Updating one priority is not the same as advancing time.** A single condition change
touches one patient: O(log n). Advancing the clock changes all n patients, so it costs at
least O(n), because every waiting time has to be written.

---

## 9. Testing that was performed

* `tests.cpp` (80 checks, all passing): formula and tie-break, insert/extract order,
  duplicate and invalid inputs rejected without changes, updates up and down, aging,
  an empty heap, removal from the middle (sift-up case, sift-down case, last leaf, unknown
  ID), 2000 random operations including removals checked after every step against a
  brute-force scan (heap property, ID→index map, size, every lookup, every extracted
  minimum, swaps never above the tree height, heap-sort display equal to `std::sort`), and
  every possible range of segment trees with 1-9 zones checked against brute-force sums and
  brute-force busiest zone after random updates.
* Scripted menu session: adding and displaying patients, a duplicate ID, invalid ESI,
  deterioration and zone values, an empty name, non-numeric input, a condition update, an
  update of an unknown ID, advancing time, invalid minutes, single-zone, multi-zone and
  empty-zone queries, reversed and out-of-range zone queries, attending until the queue
  is empty, attending on an empty queue, and an invalid menu choice. All outputs matched
  hand-calculated values.
* Two `--demo` runs give byte-identical output. Every `[check]` line in `sample_output.txt` says OK.

---

## 10. Limitations

* With only 5 zones, a plain array could answer range queries just as quickly. The
  segment tree shows the technique, and it becomes useful when k is large.
* `unordered_map` lookups are O(1) **on average**. In the worst case (many hash collisions) they are slower.
* Advancing time visits every patient (O(n)). See the future extensions below.
* A patient's zone cannot be changed.
* Data is kept only in memory; nothing is saved between runs.
* Names longer than 14 characters make the queue table misaligned.
* An ID can be reused once its patient has been attended, because the heap only holds patients who are still waiting.
* Advancing time is limited to 1440 minutes per step to keep the integers small.

## Future Extensions — Not Implemented Yet

None of the items below are in the code. They are possible next steps:

* **Fibonacci heap and d-ary heap comparisons**: implement them next to the binary heap
  and compare decrease-key and extract-min costs.
* **A proper workload benchmark**: time each heap on large, realistic sequences of
  operations instead of relying only on big-O.
* **Hopcroft–Karp bed allocation**: use bipartite matching to assign waiting patients to
  suitable free beds.
* **Persistent segment trees**: keep old versions of the zone statistics, so questions like
  "how full was zones 2-4 at minute 30?" can be answered.

---

## 11. Two-minute presentation walkthrough

1. **(15 s) Problem.** "An ED queue where priorities change: patients deteriorate, and waiting
   makes them more urgent. We also need statistics for ranges of zones. I use an indexed
   binary min-heap and a segment tree." Mention that the formula is a teaching example, not medical.
2. **(20 s) Formula.** Show the formula in `Patient.h`, say that a lower score goes first,
   and explain the arrival-order tie-break. Do the Farah Khan example: 200 − 30 − 80 = 90.
3. **(15 s) Run** `.\triage_demo.exe --demo`. Steps 1–2: the patients arrive, then compare
   the sorted queue with the heap tree view to show that a heap is not a sorted list.
4. **(20 s) Step 3, priority update.** Gabriel's score goes from 300 to 10. "The index map
   finds him at index 6 in O(1). `siftUp` swaps him with index 2, then index 0, so the update
   is O(log n)." Point at `[heap: 2 swap(s), tree height 2]`. Show `updatePatient`,
   `repairPosition` and `swapPatients`.
5. **(10 s) Step 4.** Chen's score rises, so `siftDown` runs. The tie with Asha is broken by arrival order.
6. **(10 s) Step 5, aging.** "Every score changes, so this is O(n): one pass plus a bottom-up
   `buildHeap`, not O(log n)." Ben, who has waited 70 minutes, is not overtaken by the new
   ESI-3 arrival.
7. **(5 s) Step 6.** `extractMin` attends Gabriel, and zone 3's count and waiting sum drop.
8. **(15 s) Step 7, removal.** "Deleting from the middle: the last leaf fills the hole and
   can go down (Diya) **or up** (Ethan), so I call `repairPosition`, not just `siftDown`."
9. **(10 s) Step 8, segment tree.** Zones 2–4 are combined from three nodes in O(log k).
   Zone 2 is over capacity by 1 and is reported as the most crowded zone (a max merge).
   Zone 5 is empty, so there is no division by zero.
10. **(5 s) Close.** The `[check]` lines confirm the heap property, the index map and the
   zone statistics after every step. Future work: Fibonacci and d-ary heaps, benchmarks,
   Hopcroft–Karp, and persistent segment trees.
