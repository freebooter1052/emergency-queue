# Emergency Department Dynamic Patient Triage Engine

Educational C++17 implementations of dynamic triage queues and supporting data
structures. The project compares indexed heaps, segment trees, and matching
algorithms while keeping a small interactive emergency-department simulation.

> **Educational simulation only.** Patients are fictional. The priority formulas
> are made-up teaching examples, not validated medical triage rules, and must
> not be used for real clinical decisions.

## What is included

The project has two executable surfaces:

* `triage_demo` is the interactive five-zone simulation and predefined walkthrough.
* `triage_bench` replays identical generated workloads through several heap
  implementations and compares a linear scan with bucketed segment-tree queries.

The core structures are:

| Component | Implementation | Purpose |
|---|---|---|
| Dynamic patient queue | `IndexedMinHeap.h` | Binary min-heap with an ID-to-index map |
| Zone statistics | `SegmentTree.h` | Counts, capacity, waiting-time totals, and busiest-zone range queries |
| Lazy zone/ESI analytics | `BucketSegmentTree.h` | Arrival-time buckets with wait summaries computed at query time |
| Alternative queues | `DaryHeap.h`, `FibonacciHeap.h` | Compare binary, d-ary, and Fibonacci heap behavior |
| Bed allocation | `BedMatching.h` | Hopcroft-Karp maximum bipartite matching |
| Benchmark keys/workloads | `HeapKey.h`, `Workload.h` | Shared ordering and reproducible operation streams |

## Repository layout

```text
emergency_triage_dsa/
├── main.cpp                 Interactive menu and predefined demonstration
├── Patient.h                Patient model, validation, and priority formulas
├── IndexedMinHeap.h         Indexed binary min-heap used by the demo
├── SegmentTree.h            Five-zone statistics tree used by the demo
├── BucketSegmentTree.h      Zone × ESI arrival-time analytics tree
├── DaryHeap.h               Array-based d-ary min-heap
├── FibonacciHeap.h          Handle-based Fibonacci min-heap
├── HeapKey.h                Shared score/order key for heap comparisons
├── BedMatching.h            Hopcroft-Karp patient-to-bed matching
├── Workload.h               Reproducible steady and mass-casualty workloads
├── benchmark.cpp             Heap and segment-tree benchmark executable
├── tests.cpp                 Unit, randomized, and cross-implementation tests
├── test_patients.txt         Sample input for the interactive loader
├── sample_output.txt         Captured demo output
├── benchmark_output.txt      Captured benchmark header/output sample
└── benchmark_progress.txt    Benchmark progress log
```

The `.exe` files in the folder are build artifacts; rebuild them locally rather
than relying on a previously generated binary.

## Build and run

The commands below use GCC/MinGW-w64 and PowerShell on Windows. No third-party
libraries are required.

```powershell
cd emergency_triage_dsa

# Interactive simulation and deterministic demo
g++ -std=c++17 -Wall -Wextra -O2 -static -o triage_demo.exe main.cpp
.\triage_demo.exe
.\triage_demo.exe --demo

# Full test executable
g++ -std=c++17 -Wall -Wextra -O2 -static -o triage_tests.exe tests.cpp
.\triage_tests.exe

# Heap and bucket-tree benchmark
g++ -std=c++17 -Wall -Wextra -O2 -static -o triage_bench.exe benchmark.cpp
.\triage_bench.exe --quick
```

For Linux or macOS, use the same commands without `-static` and `.exe`, then
run the binaries with `./`. The static Windows build can avoid conflicts with a
different `libstdc++-6.dll` earlier in `PATH`.

To capture plain-text demo output from Windows PowerShell 5.1:

```powershell
cmd /c ".\triage_demo.exe --demo > sample_output.txt"
```

## Interactive demo

The menu is:

```text
1. Add a patient (manually or load test data file)
2. Update patient condition
3. Advance simulated time
4. Attend to the highest-priority patient
5. Display waiting queue
6. Query zone statistics
7. Run predefined demonstration
8. Remove a patient (left without being seen)
9. Show heap as a tree + ID->index map
10. Transfer a patient to another zone
0. Exit
```

Patients can be entered manually or loaded from a file. Each data line has the
format `id, name, esi, deterioration, zone`; blank lines and lines beginning
with `#` are ignored. `test_patients.txt` contains valid and intentionally
invalid examples. Input validation rejects duplicate IDs, empty names, and
values outside the supported ESI, deterioration, and zone ranges.

The demo maintains the heap and zone tree together. Every mutation prints a
small operation trace and the predefined demo emits `[check]` lines that verify
the heap property, ID map, and zone aggregates. Its final step also runs the
Hopcroft-Karp bed matcher with one zone-compatible bed per zone and prints the
patient-to-bed assignments, unmatched patients, matching phase count, and
matching validity.

## Priority model

The regular simulation uses the lower-is-better score:

```text
score = (esi - 1) * 100
      - deteriorationLevel * 30
      - (waitingMinutes / 5) * 10
```

ESI is 1–5, deterioration is 0–4, and waiting time is rounded down to completed
five-minute blocks. Equal scores use earlier `arrivalOrder` as the tie-break.
The constants are defined at the top of `Patient.h`.

`Patient.h` also contains the continuous/lazy-aging key. With a linear waiting
penalty, the common clock term can be separated from each patient's fixed key,
so queue ordering can be maintained without rewriting every patient's score.
The interactive demo retains the block-rounded model above; the lazy key is
used by the bucket analytics and related tests.

## Data-structure behavior

### Indexed binary heap

`IndexedMinHeap` stores patients in a vector and maintains
`patientId -> array index` in an unordered map. All movement goes through the
map-aware swap/repair logic, which makes updates and arbitrary removals safe.

| Operation | Complexity |
|---|---:|
| Insert, attend next, update condition | O(log n) |
| Remove by ID | O(log n) |
| Lookup by ID | O(1) average |
| Queue rank | O(n) |
| Advance rounded waiting time for all patients | O(n) |
| Display in attend order (heap sort on a copy) | O(n log n) |

Advancing time changes every patient's score, so it cannot be an O(log n)
single-item update. The implementation updates scores and bottom-up heapifies.

### Zone segment tree

`SegmentTree` stores one leaf per zone and combines child values
associatively. A range query returns patient count, total capacity, total wait,
and the most crowded zone. Point updates and range queries are O(log k), where
`k` is the number of zones.

`BucketSegmentTree` uses one leaf per zone/ESI bucket and stores arrival times
instead of mutable waiting totals. For a clock value `now`, it derives:

```text
totalWait = count * now - sumArrival
longestWait = now - oldestArrival
shortestWait = now - newestArrival
```

This makes advancing the clock O(1) for the tree; bucket insertion/removal and
queries remain logarithmic in the number of leaves (with one range query per
requested ESI level).

### Alternative heaps and matching

`DaryHeap<D>` exposes the same insert, decrease-key, and extract-min workload
used by the benchmark. A larger branching factor reduces tree height but scans
more children during extraction.

`FibonacciHeap` provides amortized O(1) insert and decrease-key and amortized
O(log n) extract-min. Both alternative heaps use handle/position tables so
decrease-key does not search for an ID.

`HopcroftKarp` in `BedMatching.h` computes a maximum patient-to-bed matching in
O(E√V). Its optional greedy initialization can match patients in priority order
before the augmenting-path phases.

## Benchmark

`triage_bench` generates one operation stream per workload and replays that
exact stream against every heap. Each extraction is checked against a reference
ordered set, and each implementation reports validity, timing, memory
estimates, and a `touches/op` memory-traffic proxy. It runs:

* `steady arrivals`: balanced arrivals and departures.
* `mass-casualty burst`: a severe-arrival surge followed by backlog reduction.
* decrease-key ratios from 0.0 through 0.9.
* a segment-tree versus linear-scan query comparison for zone × ESI buckets.

Default options are deliberately larger than the quick run:

```text
--quick              preload 2,000; 40,000 operations; 3 trials
--trials N           number of timing trials (default 5)
--ops N              operations per workload (default 300,000)
--preload N          initial patients (default 10,000)
--drop POINTS        largest generated score decrease (default 120)
--seed S             random seed (default 2024)
--csv FILE           write sweep results as CSV
--svg FILE           write sweep results as SVG
--help               show usage
```

Example:

```powershell
.\triage_bench.exe --quick --seed 2024 --csv benchmark.csv --svg benchmark.svg
```

Benchmark numbers depend on the machine, compiler, and workload. They describe
these implementations under these generated workloads; they are not a general
ranking of data structures. `touches/op` is a proxy for entries/nodes read or
written, not a hardware cache-miss measurement.

## Tests

`tests.cpp` covers:

* priority formulas, tie-breaking, validation, and lazy aging;
* insert, extract, update, removal, transfer, rank, and randomized heap checks;
* segment-tree range queries and randomized consistency checks;
* d-ary and Fibonacci heap behavior against a common reference workload;
* bucket segment-tree updates, wait summaries, and linear-scan agreement;
* Hopcroft-Karp matching size, validity, and a larger sparse graph.

The test program reports passed and failed checks and returns a non-zero exit
code if any check fails.

## Limitations

* This is an in-memory educational project; data is not persisted between runs.
* The interactive simulation currently has five zones.
* `unordered_map` and hash-backed structures have O(1) average, not worst-case,
  lookup behavior.
* The block-rounded interactive priority model still requires an O(n) aging
  pass; the lazy-aging key is a separate continuous-aging technique.
* Benchmark timings are environment-dependent and should not be treated as
  production capacity measurements.
