
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "BucketSegmentTree.h"
#include "DaryHeap.h"
#include "FibonacciHeap.h"
#include "Workload.h"

#if defined(__x86_64__) || defined(__i386__)
#include <x86intrin.h>
#define HAVE_TSC 1
#endif

using Clock = std::chrono::steady_clock;


inline std::uint64_t readTicks() {
#ifdef HAVE_TSC
    return __rdtsc();
#else
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count());
#endif
}

struct TickClock {
    double ticksPerNs = 1.0;
    double overheadTicks = 0.0;

    TickClock() {
#ifdef HAVE_TSC
        auto wallStart = Clock::now();
        std::uint64_t tickStart = readTicks();
        while (Clock::now() - wallStart < std::chrono::milliseconds(80)) {
        }
        std::uint64_t tickEnd = readTicks();
        double ns = std::chrono::duration<double, std::nano>(Clock::now() - wallStart).count();
        ticksPerNs = static_cast<double>(tickEnd - tickStart) / ns;
#endif
        std::uint64_t sum = 0;
        const int rounds = 200000;
        for (int i = 0; i < rounds; ++i) {
            std::uint64_t a = readTicks();
            std::uint64_t b = readTicks();
            sum += b - a;
        }
        overheadTicks = static_cast<double>(sum) / rounds;
    }
};


constexpr int OP_KINDS = 3;

int kindOf(OpType type) {
    return type == OpType::Insert ? 0 : (type == OpType::DecreaseKey ? 1 : 2);
}

struct RunResult {
    double totalMs = 0;
    double nsPerKind[OP_KINDS] = {0, 0, 0};
    double peakKB = 0;
    double touchesPerOp = 0;
    bool correct = true;
};

template <class Heap>
inline bool applyOp(Heap& heap, const Op& op) {
    switch (op.type) {
        case OpType::Insert:
            return heap.insert(op.id, op.key);
        case OpType::DecreaseKey:
            return heap.decreaseKey(op.id, op.key);
        case OpType::ExtractMin: {
            int id;
            HeapKey key;
            return heap.extractMin(id, key) && id == op.expectedId;
        }
    }
    return false;
}

template <class Heap>
RunResult runStructure(const Workload& w, const TickClock& clock) {
    RunResult result;

    {
        Heap heap(w.maxId);
        for (const Op& op : w.preload) applyOp(heap, op);
        bool ok = true;
        auto start = Clock::now();
        for (const Op& op : w.operations) ok &= applyOp(heap, op);
        auto stop = Clock::now();
        result.totalMs = std::chrono::duration<double, std::milli>(stop - start).count();
        result.correct = ok && heap.isValid();
    }

    {
        Heap heap(w.maxId);
        for (const Op& op : w.preload) applyOp(heap, op);
        std::uint64_t touchBase = heap.touches();
        double ticks[OP_KINDS] = {0, 0, 0};
        long long counts[OP_KINDS] = {0, 0, 0};
        std::size_t peakBytes = heap.memoryBytes();
        bool ok = true;
        for (const Op& op : w.operations) {
            std::uint64_t a = readTicks();
            ok &= applyOp(heap, op);
            std::uint64_t b = readTicks();
            int kind = kindOf(op.type);
            ticks[kind] += static_cast<double>(b - a) - clock.overheadTicks;
            ++counts[kind];
            peakBytes = std::max(peakBytes, heap.memoryBytes());
        }
        for (int k = 0; k < OP_KINDS; ++k) {
            double ns = ticks[k] / clock.ticksPerNs;
            result.nsPerKind[k] = counts[k] > 0 ? std::max(0.0, ns / counts[k]) : 0.0;
        }
        result.peakKB = static_cast<double>(peakBytes) / 1024.0;
        result.touchesPerOp = static_cast<double>(heap.touches() - touchBase) / w.operations.size();
        result.correct = result.correct && ok;
    }
    return result;
}

struct Structure {
    const char* name;
    RunResult (*run)(const Workload&, const TickClock&);
};

const std::vector<Structure>& structures() {
    static const std::vector<Structure> all = {
        {"binary (d=2)", &runStructure<DaryHeap<2>>},
        {"4-ary", &runStructure<DaryHeap<4>>},
        {"8-ary", &runStructure<DaryHeap<8>>},
        {"Fibonacci", &runStructure<FibonacciHeap>},
    };
    return all;
}

struct Summary {
    std::string name;
    double meanMs = 0, sdMs = 0;
    double nsPerKind[OP_KINDS] = {0, 0, 0};
    double peakKB = 0, touchesPerOp = 0;
    bool correct = true;
};

double meanOf(const std::vector<double>& v) {
    double sum = 0;
    for (double x : v) sum += x;
    return v.empty() ? 0 : sum / v.size();
}

double sampleSd(const std::vector<double>& v) {
    if (v.size() < 2) return 0;
    double m = meanOf(v), sum = 0;
    for (double x : v) sum += (x - m) * (x - m);
    return std::sqrt(sum / (v.size() - 1));
}

struct WorkloadReport {
    WorkloadSpec spec;
    Workload sample;
    std::vector<Summary> rows;
};

WorkloadReport benchmarkWorkload(WorkloadSpec spec, int trials, const TickClock& clock) {
    const std::vector<Structure>& all = structures();
    std::vector<std::vector<RunResult>> runs(all.size());
    unsigned baseSeed = spec.seed;
    WorkloadReport report;
    for (int trial = 0; trial <= trials; ++trial) {
        spec.seed = baseSeed + static_cast<unsigned>(trial);
        Workload workload = generateWorkload(spec);
        if (trial == 1) report.sample = workload;
        for (size_t s = 0; s < all.size(); ++s) {
            RunResult r = all[s].run(workload, clock);
            if (trial > 0) runs[s].push_back(r);
        }
    }
    spec.seed = baseSeed;
    report.spec = spec;
    for (size_t s = 0; s < all.size(); ++s) {
        Summary row;
        row.name = all[s].name;
        std::vector<double> times;
        for (const RunResult& r : runs[s]) {
            times.push_back(r.totalMs);
            row.correct = row.correct && r.correct;
            for (int k = 0; k < OP_KINDS; ++k) row.nsPerKind[k] += r.nsPerKind[k] / runs[s].size();
            row.peakKB = std::max(row.peakKB, r.peakKB);
            row.touchesPerOp += r.touchesPerOp / runs[s].size();
        }
        row.meanMs = meanOf(times);
        row.sdMs = sampleSd(times);
        report.rows.push_back(row);
    }
    return report;
}


std::string fixed(double value, int digits) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(digits) << value;
    return out.str();
}

const Summary& fastestOf(const WorkloadReport& report) {
    return *std::min_element(report.rows.begin(), report.rows.end(),
                             [](const Summary& a, const Summary& b) { return a.meanMs < b.meanMs; });
}

void printDetail(const WorkloadReport& report, int trials) {
    const Workload& w = report.sample;
    std::cout << "\n=== " << report.spec.name << ", decrease-key ratio "
              << fixed(report.spec.decreaseRatio, 2) << " ===\n"
              << "  preload " << report.spec.preload << " patients, " << report.spec.operations
              << " operations (about " << w.insertCount << " insert, " << w.decreaseCount
              << " decrease-key, " << w.extractCount << " extract-min), peak queue "
              << w.peakQueueSize << ", " << trials << " trials after 1 warm-up\n";
    std::cout << "  " << std::left << std::setw(14) << "structure" << std::right << std::setw(22)
              << "total ms (mean +- sd)" << std::setw(11) << "insert ns" << std::setw(12)
              << "decr-key ns" << std::setw(11) << "extract ns" << std::setw(10) << "peak KB"
              << std::setw(12) << "touches/op" << std::setw(9) << "correct" << "\n";
    for (const Summary& row : report.rows) {
        std::cout << "  " << std::left << std::setw(14) << row.name << std::right << std::setw(22)
                  << (fixed(row.meanMs, 1) + " +- " + fixed(row.sdMs, 1)) << std::setw(11)
                  << fixed(row.nsPerKind[0], 0) << std::setw(12) << fixed(row.nsPerKind[1], 0)
                  << std::setw(11) << fixed(row.nsPerKind[2], 0) << std::setw(10)
                  << fixed(row.peakKB, 0) << std::setw(12) << fixed(row.touchesPerOp, 1)
                  << std::setw(9) << (row.correct ? "yes" : "NO") << "\n";
    }
    std::cout << "  fastest overall: " << fastestOf(report).name << "\n";
}

struct Sweep {
    std::string label;
    std::vector<double> ratios;
    std::vector<WorkloadReport> reports;
};

Sweep runSweep(const std::string& label, WorkloadSpec (*make)(double), const std::vector<double>& ratios,
               int preload, int operations, int maxDrop, int trials, unsigned seed,
               const TickClock& clock) {
    Sweep sweep;
    sweep.label = label;
    sweep.ratios = ratios;
    for (double ratio : ratios) {
        WorkloadSpec spec = make(ratio);
        spec.preload = preload;
        spec.operations = operations;
        spec.maxScoreDrop = maxDrop;
        spec.seed = seed;
        sweep.reports.push_back(benchmarkWorkload(spec, trials, clock));
        std::cerr << "  [" << label << "] decrease-key ratio " << fixed(ratio, 2) << " done\n";
    }
    return sweep;
}

void printSweep(const Sweep& sweep) {
    std::cout << "\n=== Total time (ms, mean of trials) vs decrease-key ratio: " << sweep.label << " ===\n";
    std::cout << "  " << std::left << std::setw(8) << "ratio";
    for (const Summary& row : sweep.reports[0].rows) std::cout << std::right << std::setw(14) << row.name;
    std::cout << std::setw(16) << "fastest" << std::setw(18) << "Fib / binary" << "\n";
    for (size_t i = 0; i < sweep.ratios.size(); ++i) {
        const WorkloadReport& report = sweep.reports[i];
        std::cout << "  " << std::left << std::setw(8) << fixed(sweep.ratios[i], 2);
        for (const Summary& row : report.rows) std::cout << std::right << std::setw(14) << fixed(row.meanMs, 1);
        const Summary& fib = report.rows.back();
        std::cout << std::setw(16) << fastestOf(report).name << std::setw(18)
                  << fixed(fib.meanMs / report.rows.front().meanMs, 2) << "\n";
    }
    int firstWin = -1;
    for (size_t i = 0; i < sweep.ratios.size() && firstWin < 0; ++i) {
        if (fastestOf(sweep.reports[i]).name == "Fibonacci") firstWin = static_cast<int>(i);
    }
    if (firstWin < 0) {
        std::cout << "  Result: the Fibonacci heap was not the fastest at any tested ratio.\n";
    } else {
        std::cout << "  Result: the Fibonacci heap first became the fastest at ratio "
                  << fixed(sweep.ratios[firstWin], 2) << ".\n";
    }
}

void writeCsv(const std::string& path, const std::vector<Sweep>& sweeps) {
    std::ofstream out(path);
    if (!out) {
        std::cerr << "Cannot write " << path << "\n";
        return;
    }
    out << "workload,decrease_ratio,structure,mean_ms,sd_ms,insert_ns,decrease_ns,extract_ns,"
           "peak_kb,touches_per_op,correct\n";
    for (const Sweep& sweep : sweeps) {
        for (size_t i = 0; i < sweep.ratios.size(); ++i) {
            for (const Summary& row : sweep.reports[i].rows) {
                out << sweep.label << ',' << sweep.ratios[i] << ',' << row.name << ',' << row.meanMs << ','
                    << row.sdMs << ',' << row.nsPerKind[0] << ',' << row.nsPerKind[1] << ','
                    << row.nsPerKind[2] << ',' << row.peakKB << ',' << row.touchesPerOp << ','
                    << (row.correct ? 1 : 0) << '\n';
            }
        }
    }
    std::cout << "\nWrote " << path << "\n";
}

void writeSvg(const std::string& path, const std::vector<Sweep>& sweeps) {
    std::ofstream out(path);
    if (!out) {
        std::cerr << "Cannot write " << path << "\n";
        return;
    }
    const char* colors[] = {"#1f77b4", "#2ca02c", "#9467bd", "#d62728"};
    const int panelW = 520, panelH = 360, left = 60, right = 20, top = 40, bottom = 50;
    int width = panelW * static_cast<int>(sweeps.size());
    out << "<svg xmlns='http://www.w3.org/2000/svg' width='" << width << "' height='" << panelH
        << "' font-family='sans-serif' font-size='12'>\n<rect width='100%' height='100%' fill='white'/>\n";
    for (size_t p = 0; p < sweeps.size(); ++p) {
        const Sweep& sweep = sweeps[p];
        double maxMs = 0;
        for (const WorkloadReport& r : sweep.reports)
            for (const Summary& row : r.rows) maxMs = std::max(maxMs, row.meanMs);
        maxMs *= 1.08;
        double minRatio = sweep.ratios.front(), maxRatio = sweep.ratios.back();
        double x0 = p * panelW + left, plotW = panelW - left - right, plotH = panelH - top - bottom;
        auto px = [&](double ratio) { return x0 + (ratio - minRatio) / (maxRatio - minRatio) * plotW; };
        auto py = [&](double ms) { return top + plotH - ms / maxMs * plotH; };

        out << "<text x='" << x0 << "' y='22' font-size='14' font-weight='bold'>Total time vs decrease-key ratio: "
            << sweep.label << "</text>\n";
        out << "<rect x='" << x0 << "' y='" << top << "' width='" << plotW << "' height='" << plotH
            << "' fill='none' stroke='#999'/>\n";
        for (int t = 0; t <= 4; ++t) {
            double ms = maxMs * t / 4;
            out << "<line x1='" << x0 << "' x2='" << x0 + plotW << "' y1='" << py(ms) << "' y2='" << py(ms)
                << "' stroke='#eee'/><text x='" << x0 - 6 << "' y='" << py(ms) + 4
                << "' text-anchor='end'>" << fixed(ms, 0) << "</text>\n";
        }
        for (double ratio : sweep.ratios) {
            out << "<text x='" << px(ratio) << "' y='" << top + plotH + 16 << "' text-anchor='middle'>"
                << fixed(ratio, 1) << "</text>\n";
        }
        out << "<text x='" << x0 + plotW / 2 << "' y='" << panelH - 8
            << "' text-anchor='middle'>decrease-key ratio</text>\n";
        out << "<text x='14' y='" << top + plotH / 2 << "' transform='rotate(-90 14 " << top + plotH / 2
            << ")' text-anchor='middle'>total time (ms)</text>\n";
        for (size_t s = 0; s < sweep.reports[0].rows.size(); ++s) {
            out << "<polyline fill='none' stroke='" << colors[s % 4] << "' stroke-width='2' points='";
            for (size_t i = 0; i < sweep.ratios.size(); ++i) {
                out << px(sweep.ratios[i]) << ',' << py(sweep.reports[i].rows[s].meanMs) << ' ';
            }
            out << "'/>\n";
            double lx = x0 + 10 + 110 * (s % 4), ly = top + 14;
            out << "<rect x='" << lx << "' y='" << ly - 9 << "' width='12' height='3' fill='" << colors[s % 4]
                << "'/><text x='" << lx + 16 << "' y='" << ly - 2 << "'>" << sweep.reports[0].rows[s].name
                << "</text>\n";
        }
    }
    out << "</svg>\n";
    std::cout << "Wrote " << path << "\n";
}


void benchmarkSegmentTree(unsigned seed) {
    std::cout << "\n=== Segment tree vs linear scan: zone x ESI range statistics ===\n"
              << "  25 buckets (5 zones x 5 ESI levels), 5000 random range queries per size,\n"
              << "  best of 3 repetitions; every answer is checked against the scan.\n";
    std::cout << "  " << std::right << std::setw(9) << "patients" << std::setw(16) << "scan ns/query"
              << std::setw(16) << "tree ns/query" << std::setw(10) << "speedup" << std::setw(15)
              << "nodes/query" << std::setw(18) << "tree update ns" << std::setw(10) << "correct" << "\n";

    for (int n : {100, 1000, 10000, 100000}) {
        std::mt19937 rng(seed + n);
        auto pick = [&](int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(rng); };
        std::vector<BucketRecord> records;
        BucketSegmentTree tree(5, 5);
        for (int i = 0; i < n; ++i) {
            BucketRecord r{pick(1, 5), pick(1, 5), pick(0, 10000)};
            records.push_back(r);
            tree.addPatient(r.zone, r.esi, r.arrival);
        }
        struct Query { int zf, zt, ef, et; };
        std::vector<Query> queries;
        for (int i = 0; i < 5000; ++i) {
            int a = pick(1, 5), b = pick(1, 5), c = pick(1, 5), d = pick(1, 5);
            queries.push_back({std::min(a, b), std::max(a, b), std::min(c, d), std::max(c, d)});
        }

        bool correct = true;
        long long sink = 0;
        double scanNs = 1e18, treeNs = 1e18;
        for (int rep = 0; rep < 3; ++rep) {
            auto t0 = Clock::now();
            for (const Query& q : queries) sink += linearScanStats(records, q.zf, q.zt, q.ef, q.et).count;
            auto t1 = Clock::now();
            const int treeRepeats = 20;
            for (int again = 0; again < treeRepeats; ++again)
                for (const Query& q : queries) sink += tree.query(q.zf, q.zt, q.ef, q.et).count;
            auto t2 = Clock::now();
            scanNs = std::min(scanNs, std::chrono::duration<double, std::nano>(t1 - t0).count() / queries.size());
            treeNs = std::min(treeNs, std::chrono::duration<double, std::nano>(t2 - t1).count() /
                                          (queries.size() * treeRepeats));
        }
        tree.resetNodeCount();
        for (const Query& q : queries) {
            BucketStats a = tree.query(q.zf, q.zt, q.ef, q.et);
            BucketStats b = linearScanStats(records, q.zf, q.zt, q.ef, q.et);
            if (a.count != b.count || a.sumArrival != b.sumArrival || a.oldestArrival != b.oldestArrival ||
                a.newestArrival != b.newestArrival) {
                correct = false;
            }
        }
        double nodesPerQuery = static_cast<double>(tree.nodesVisited()) / queries.size();

        const int updates = 20000;
        auto u0 = Clock::now();
        for (int i = 0; i < updates; ++i) {
            BucketRecord& r = records[pick(0, n - 1)];
            tree.removePatient(r.zone, r.esi, r.arrival);
            r = BucketRecord{pick(1, 5), pick(1, 5), pick(0, 10000)};
            tree.addPatient(r.zone, r.esi, r.arrival);
        }
        auto u1 = Clock::now();
        double updateNs = std::chrono::duration<double, std::nano>(u1 - u0).count() / updates;
        correct = correct && tree.isConsistent() && sink >= 0;

        std::cout << "  " << std::right << std::setw(9) << n << std::setw(16) << fixed(scanNs, 0)
                  << std::setw(16) << fixed(treeNs, 0) << std::setw(9) << fixed(scanNs / treeNs, 1) << "x"
                  << std::setw(15) << fixed(nodesPerQuery, 1) << std::setw(18) << fixed(updateNs, 0)
                  << std::setw(10) << (correct ? "yes" : "NO") << "\n";
    }
    std::cout << "  The scan grows with the number of patients; the tree's query work depends only on the\n"
              << "  number of buckets (k = 25). A tree update also pays a multiset step inside its leaf.\n";
}


int main(int argc, char* argv[]) {
    int preload = 10000, operations = 300000, trials = 5, maxDrop = 120;
    unsigned seed = 2024;
    bool quick = false;
    std::string csvPath, svgPath;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto next = [&]() { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
        if (arg == "--quick") quick = true;
        else if (arg == "--trials") trials = std::atoi(next().c_str());
        else if (arg == "--ops") operations = std::atoi(next().c_str());
        else if (arg == "--preload") preload = std::atoi(next().c_str());
        else if (arg == "--drop") maxDrop = std::atoi(next().c_str());
        else if (arg == "--seed") seed = static_cast<unsigned>(std::atoi(next().c_str()));
        else if (arg == "--csv") csvPath = next();
        else if (arg == "--svg") svgPath = next();
        else {
            std::cout << "Usage: triage_bench [--quick] [--trials N] [--ops N] [--preload N] [--drop POINTS]\n"
                      << "                    [--seed S] [--csv file] [--svg file]\n"
                      << "  --drop: largest score drop of one deterioration (default 120; ESI 4 -> 2 is 200)\n";
            return arg == "--help" ? 0 : 1;
        }
    }
    if (quick) {
        preload = 2000;
        operations = 40000;
        trials = 3;
    }
    if (trials < 1 || operations < 1 || preload < 0 || maxDrop < 1) {
        std::cout << "trials, ops and drop must be at least 1 and preload at least 0.\n";
        return 1;
    }

    std::cout << "Heap benchmark. Same generated operation stream for every structure; every ExtractMin is\n"
              << "checked against a reference ordered set. Educational priorities, fictional patients.\n"
              << "Per-operation ns come from a calibrated "
#ifdef HAVE_TSC
              << "CPU cycle counter"
#else
              << "steady clock"
#endif
              << " (timer overhead subtracted); 'touches/op' counts heap entries or nodes read/written\n"
              << "(a memory-traffic proxy, NOT hardware cache misses). Largest deterioration drop: "
              << maxDrop << " points.\n";
    TickClock clock;

    const std::vector<double> ratios = {0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9};
    Sweep steady = runSweep("steady arrivals", &steadyWorkload, ratios, preload, operations, maxDrop,
                            trials, seed, clock);
    Sweep burst = runSweep("mass-casualty burst", &burstWorkload, ratios, preload, operations, maxDrop,
                           trials, seed, clock);

    for (const Sweep* sweep : {&steady, &burst}) {
        for (size_t i = 0; i < sweep->ratios.size(); ++i) {
            if (std::abs(sweep->ratios[i] - 0.3) < 1e-9) printDetail(sweep->reports[i], trials);
        }
    }
    printSweep(steady);
    printSweep(burst);

    bool allCorrect = true;
    for (const Sweep* sweep : {&steady, &burst})
        for (const WorkloadReport& r : sweep->reports)
            for (const Summary& row : r.rows) allCorrect = allCorrect && row.correct;
    std::cout << "\nAll heaps produced the reference extraction order and kept a valid structure: "
              << (allCorrect ? "yes" : "NO - see the 'correct' column") << "\n";

    benchmarkSegmentTree(seed);

    std::vector<Sweep> both = {steady, burst};
    if (!csvPath.empty()) writeCsv(csvPath, both);
    if (!svgPath.empty()) writeSvg(svgPath, both);
    std::cout << "\nNote: numbers depend on the machine, compiler and workload. They show how these\n"
              << "implementations behaved here, not a general ranking of the data structures.\n";
    return allCorrect ? 0 : 1;
}
