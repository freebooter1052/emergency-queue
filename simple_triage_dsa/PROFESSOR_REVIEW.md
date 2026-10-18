# Professor's Review: Suggestions and What Was Changed

This file records a DSA-style review of the first version of the project, the improvements
that were implemented, and how to explain each one in a lab viva. The aim of every change
is to make the **algorithms more visible and easier to defend**, not to add features for
show.

The first version was already correct: a hand-written indexed binary min-heap, a segment
tree, clear complexity claims, and tests. The review asked one question: *"What would a
DSA examiner ask that this version cannot yet demonstrate?"*

---

## Summary of changes

| # | Suggestion | Where | Main DSA idea it shows |
|---|---|---|---|
| 1 | Remove any patient by ID ("left without being seen") | `IndexedMinHeap::removePatient`, menu 8, demo step 7 | Deleting from the middle of a heap can need **sift-up or sift-down** |
| 2 | Count the swaps made by each heap operation | `lastOperationSwaps()`, `height()` | The O(log n) bound, **measured** instead of only claimed |
| 3 | Show the heap as a tree, with the ID → index map | `printHeapTree()`, menu 9 | How an array *is* a complete binary tree |
| 4 | Produce the sorted display with heap sort | `getAllPatientsSorted()` | Heap sort, using our own heap instead of `std::sort` |
| 5 | "Most crowded zone" in a range query | `ZoneStats::busiestZone`, `combineStats`, `makeLeaf` | Segment trees work for **any associative merge**, not just sums |
| 6 | Reject a duplicate ID straight away in manual entry | `handleAddPatient()` | Input validation (small usability fix) |

Tests grew from 63 to **80 checks, all passing**. The demo now has 8 steps, and every
`[check]` line in `sample_output.txt` says OK.

---

## 1. Remove any patient by ID

### Reviewer's observation
> "Your index map is the whole reason for the *indexed* heap, but you only use it for
> `updatePatient`. The textbook operation that really needs it is **delete an arbitrary
> element**, and it hides a classic trap. Also, real EDs have patients who leave without
> being seen; your README listed that as a limitation."

### What was implemented
`removePatient(patientId, removedPatient)` in `IndexedMinHeap.h`:

```cpp
int index = indexOf[patientId];          // 1. find the hole in O(1)
swapPatients(index, size() - 1);         // 2. move the last leaf into the hole
heap.pop_back();
indexOf.erase(patientId);
if (index < size()) repairPosition(index); // 3. repair: up OR down
```

`main.cpp` calls it from `removePatientAndReport()`, which also updates the zone's count
and waiting-time sum in the segment tree.

### The trap: why `siftDown` alone is wrong
In `extractMin`, the hole is the root, so the moved leaf can only be *too big* and must
go **down**. In the middle of the heap, the moved leaf comes from a **different subtree**.
It may be smaller than its new parent, and then it must go **up**.

The demo (step 7) shows both cases on real data:

```text
Before:   Level 0:  [0] 101(60)
          Level 1:  [1] 104(120)  [2] 103(60)
          Level 2:  [3] 102(160)  [4] 105(320)  [5] 106(90)  [6] 108(200)

Diya (104) leaves -> hole at [1]. Last leaf 108(200) fills it.
  Its children are 102(160) and 105(320); 160 beats 200, so 108 moves DOWN to [3].

          Level 0:  [0] 101(60)
          Level 1:  [1] 102(160)  [2] 103(60)
          Level 2:  [3] 108(200)  [4] 105(320)  [5] 106(90)

Ethan (105) leaves -> hole at [4]. Last leaf 106(90) fills it.
  Its new parent is 102(160); 90 beats 160, so 106 moves UP to [1].
```

If the code used only `siftDown`, 106 would stay below 102 and the heap property would be
broken. The unit test `testHeapRemove()` checks exactly this case.

### Complexity
O(1) average map lookup + one sift of at most `height` levels = **O(log n)**.

### How to explain it (viva script)
"To delete from the middle, I find the patient's index with the map, move the last leaf
into the hole and shrink the array. The moved leaf came from another part of the tree, so
it might belong higher or lower. `repairPosition` checks the parent: if the leaf beats its
parent it sifts up, otherwise it sifts down. Only one direction can ever be needed."

### Likely question
**Q: Why can only one direction be needed, never both?**
A: If the leaf beats its parent, it also beats everything below the hole, because the
old subtree under the hole was already at least as large as that parent. So once it moves
up it never needs to move down. If it does not beat its parent, it can only be too large
for its children, so it moves down.

---

## 2. Count the swaps (make O(log n) visible)

### Reviewer's observation
> "You *claim* insert, extract and update are O(log n). Can you *show* it while the
> program is running?"

### What was implemented
* `swapPatients()` increases a counter `swapsInLastOperation` (it is the only place
  elements move, so nothing is missed).
* Each public operation resets the counter at its start. `extractMin` and `removePatient`
  reset it *after* moving the last leaf into the hole, so only the sift swaps are counted.
* `height()` returns floor(log2 n) by halving n until it reaches 1.
* `main.cpp` prints a note after each operation, for example:

```text
Added patient 104 (Diya Rao) ... -> priority score 200  [heap: 1 swap(s), tree height 2]
Heap work     : [heap: 2 swap(s), tree height 2]        <- Gabriel's sift-up in step 3
```

### Why the bound holds
Each swap moves the element exactly one level up or down, and a complete binary tree with
n nodes has floor(log2 n) levels below the root. So **swaps ≤ height = floor(log2 n)** for
one insert, extract, update or remove. The randomised test checks this inequality after
each of 2000 operations.

### A bonus observation from the counter
Advancing time rebuilds the whole heap. In both time steps of the demo the rebuild made
**0 swaps**: everyone aged by the same amount, so nobody changed order. The counter backs
up what the README says about aging.

### How to explain it
"Every swap moves a patient one level, and the tree has only log2 n levels, so one sift is
at most log2 n swaps. The program prints the swap count next to the height so you can see
the bound being respected."

### Likely question
**Q: Is counting swaps the same as measuring running time?**
A: No. It counts the main unit of work, and comparisons are at most two per level. It is
evidence for the bound, not a benchmark. A real timing benchmark is listed as future work.

---

## 3. Heap tree view and ID → index map

### Reviewer's observation
> "A flat array `[0] 103 [1] 104 [2] 101 ...` is hard to read as a tree in front of an
> examiner. Also, the index map is never shown, so nobody can check it by eye."

### What was implemented
`printHeapTree()` (menu option 9, also used in the demo) prints the array **level by
level**. Level L holds indices 2^L − 1 to 2^(L+1) − 2. It then prints the map, sorted by
ID:

```text
  Internal heap array drawn as a tree, written as [index] patientID(score):
    Level 0:  [0] 107(10)
    Level 1:  [1] 104(200)  [2] 103(40)
    Level 2:  [3] 102(240)  [4] 105(400)  [5] 106(170)  [6] 101(140)
  ID -> index map:  101->6  102->3  103->2  104->1  105->4  106->5  107->0
```

You can check by hand that each parent beats its children, and that every map entry points
to the right slot.

### How to explain it
"The heap is just a vector, but index i has children 2i+1 and 2i+2, so printing it in
groups of 1, 2, 4, 8 shows the tree. The map line shows that `indexOf` agrees with the
array after every change."

---

## 4. Heap sort for the sorted display

### Reviewer's observation
> "Option 5 used `std::sort` on a copy. It works, but in a heap project the natural way to
> list the elements in order is to pop them from the heap: that is **heap sort**."

### What was implemented
```cpp
std::vector<Patient> getAllPatientsSorted() const {
    IndexedMinHeap copy = *this;           // O(n) copy; the real heap is untouched
    std::vector<Patient> ordered;
    Patient next;
    while (copy.extractMin(next)) ordered.push_back(next);   // n pops x O(log n)
    return ordered;
}
```

### Complexity
O(n) copy + n × O(log n) extractions = **O(n log n)**, the same as before, but now done by
our own data structure. The randomised test compares this output with `std::sort` every
50 steps and confirms that the real heap is still valid afterwards.

### How to explain it
"Repeatedly extracting the minimum gives the elements in sorted order. That is heap sort.
I do it on a copy, so the real queue does not change."

### Likely question
**Q: Why copy instead of popping the real heap and pushing everyone back?**
A: Pushing back costs another O(n log n) and changes the internal layout. Copying is O(n)
and leaves the real heap exactly as it was.

---

## 5. Segment tree with a non-additive merge: most crowded zone

### Reviewer's observation
> "Every field in your segment tree is a **sum**. An examiner will ask: *could you just
> use prefix sums instead?* For sums with point updates the answer is "a Fenwick tree
> would do", so show something that needs the general segment-tree idea: a **max**."

### What was implemented
`ZoneStats` gained two fields:

```cpp
int busiestZone = -1;   // index of the most crowded zone in this range (-1 = empty range)
int busiestLoad = 0;    // load = waiting patients - capacity (positive = over capacity)
```

* **Leaf** (`makeLeaf`): a single zone is its own busiest zone, with load = count − capacity.
* **Merge** (`combineStats`): keep the side with the larger load; on a tie keep the left
  side, which means the lower zone number.
* **Empty range** (`ZoneStats{}`): `busiestZone = -1`, so the merge always takes the other side.

`printZoneRange()` shows it for multi-zone ranges:

```text
Zones 2 to 4 (segment tree range query, O(log k)):
    ...
    Most crowded zone    : zone 2 (3 waiting / 2 capacity, load +1)
```

(Zone 2's capacity was changed from 3 to 2 so that the demo still shows an over-capacity
zone after two patients leave in step 7.)

### Why this is valid for a segment tree
A segment tree only needs the merge to be **associative**: merge(merge(a, b), c) =
merge(a, merge(b, c)). Taking the maximum is associative, and so is summing. That is why
both kinds of field can sit in the same node and still be answered in **O(log k)**.

Unlike sums, a maximum **cannot** be answered with prefix sums: you cannot subtract
max(1..1) from max(1..4) to get max(2..4). This is the real reason to use a segment tree.

### How to explain it
"Each node stores sums for counting and also the zone with the largest overload in its
range. Merging two children means adding the sums and keeping the larger overload. Because
both operations are associative, the same O(log k) query answers both."

### Likely question
**Q: What does the tie-break do, and does it matter?**
A: With equal loads, the left child wins, so the lower zone number is reported. This
makes the answer deterministic, and the brute-force test uses the same rule.

---

## 6. Early duplicate-ID check (small fix)

In manual entry the program used to ask for name, ESI, deterioration and zone before
saying the ID was a duplicate. It now checks `contains(id)` straight after the ID is
typed. `admitPatient()` still checks again, so file loading and the demo are protected too.

---

## Suggestions considered but NOT implemented (and why)

| Idea | Why it was left out |
|---|---|
| **Global clock to avoid O(n) time advancement** (store arrival time, compute wait = now − arrival) | With the `(wait / 5)` rounding, patients' scores do not all change by the same amount, so the heap order is not guaranteed to stay valid without a rebuild. Making this correct would complicate the formula and the explanation. The O(n) cost is honest and documented. |
| Replace `unordered_map` with a plain vector indexed by ID | Faster and worst-case O(1), but only works if IDs are small and dense. The hash map is more general and easier to justify. |
| Fenwick (binary indexed) tree for the sums | It would handle the sums, but not the "most crowded zone" max. The segment tree handles both. |
| Fibonacci / d-ary heap, benchmarks, Hopcroft–Karp, persistent segment tree | Already listed under *Future Extensions — Not Implemented Yet* in the README. They are larger projects. |
| `long long` for waiting-time sums | With at most 1440 minutes per step and a small number of patients, `int` cannot overflow in this demo. Worth changing for a long-running simulation. |

---

## How to verify everything yourself

```powershell
cd simple_triage_dsa
g++ -std=c++17 -Wall -Wextra -O2 -static -o triage_demo.exe main.cpp
g++ -std=c++17 -Wall -Wextra -O2 -static -o triage_tests.exe tests.cpp
.\triage_tests.exe          # expect: Passed: 80   Failed: 0
.\triage_demo.exe --demo    # steps 1-8, every [check] line says OK
.\triage_demo.exe           # try options 8 (remove) and 9 (tree view)
```

New tests in `tests.cpp`:
* `testHeapRemove()`: hand-built heap with a known layout; removal causing **sift-up**,
  removal of the root by ID causing **sift-down**, removal of the last leaf (0 swaps), and
  an unknown ID.
* Randomised heap test: now includes random removals, checks **swaps ≤ height** after
  every single-patient operation, and compares the heap-sort display with `std::sort`.
* Segment tree: busiest zone checked by hand on fixed data, and against a brute-force
  argmax for every range in trees of size 1 to 9.
