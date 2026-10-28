#pragma once

#include <algorithm>
#include <climits>
#include <set>
#include <vector>

// Segment tree over zone x ESI buckets: the "live analytics" structure from the slides.
//
// Leaves are laid out ESI-major, leaf = (esi - 1) * zones + (zone - 1), so "ESI 2 in zones
// 1..4" is ONE contiguous range and costs one O(log k) range query (k = zones * ESI levels).
// A query over several ESI levels is one range query per level, still O(E log k).
//
// Lazy waiting time: a bucket stores ARRIVAL times, not waiting times. A waiting time grows
// with the clock, so a stored total would go stale every minute. Instead
//     totalWait   = count * now - sumArrival
//     longestWait = now - oldestArrival      (the "minimum" arrival time)
//     shortestWait= now - newestArrival
// are computed at query time from the clock, so advancing time touches nothing in the tree.

struct BucketStats {
    int count = 0;
    long long sumArrival = 0;
    int oldestArrival = INT_MAX;
    int newestArrival = INT_MIN;
};

inline BucketStats combineBuckets(const BucketStats& a, const BucketStats& b) {
    BucketStats result;
    result.count = a.count + b.count;
    result.sumArrival = a.sumArrival + b.sumArrival;
    result.oldestArrival = std::min(a.oldestArrival, b.oldestArrival);
    result.newestArrival = std::max(a.newestArrival, b.newestArrival);
    return result;
}

struct WaitSummary {
    int count = 0;
    long long totalWait = 0;
    double averageWait = 0.0;
    int longestWait = 0;
    int shortestWait = 0;
};

inline WaitSummary summarizeWaits(const BucketStats& stats, int now) {
    WaitSummary summary;
    summary.count = stats.count;
    if (stats.count == 0) {
        return summary;
    }
    summary.totalWait = static_cast<long long>(stats.count) * now - stats.sumArrival;
    summary.averageWait = static_cast<double>(summary.totalWait) / stats.count;
    summary.longestWait = now - stats.oldestArrival;
    summary.shortestWait = now - stats.newestArrival;
    return summary;
}

// One waiting patient as the linear-scan baseline sees it.
struct BucketRecord {
    int zone;
    int esi;
    int arrival;
};

// The O(n) baseline the segment tree is compared with: look at every patient.
inline BucketStats linearScanStats(const std::vector<BucketRecord>& records, int zoneFrom,
                                   int zoneTo, int esiFrom, int esiTo) {
    BucketStats result;
    for (const BucketRecord& r : records) {
        if (r.zone >= zoneFrom && r.zone <= zoneTo && r.esi >= esiFrom && r.esi <= esiTo) {
            BucketStats one;
            one.count = 1;
            one.sumArrival = r.arrival;
            one.oldestArrival = one.newestArrival = r.arrival;
            result = combineBuckets(result, one);
        }
    }
    return result;
}

class BucketSegmentTree {
public:
    BucketSegmentTree(int numberOfZones, int numberOfEsiLevels)
        : zones(numberOfZones),
          esiLevels(numberOfEsiLevels),
          leafCount(numberOfZones * numberOfEsiLevels),
          tree(4 * numberOfZones * numberOfEsiLevels),
          buckets(numberOfZones * numberOfEsiLevels) {}

    bool addPatient(int zone, int esi, int arrival) {
        int leaf = leafIndex(zone, esi);
        if (leaf < 0) {
            return false;
        }
        buckets[leaf].arrivals.insert(arrival);
        buckets[leaf].sum += arrival;
        refreshLeaf(1, 0, leafCount - 1, leaf);
        return true;
    }

    bool removePatient(int zone, int esi, int arrival) {
        int leaf = leafIndex(zone, esi);
        if (leaf < 0) {
            return false;
        }
        auto found = buckets[leaf].arrivals.find(arrival);
        if (found == buckets[leaf].arrivals.end()) {
            return false;
        }
        buckets[leaf].arrivals.erase(found);
        buckets[leaf].sum -= arrival;
        refreshLeaf(1, 0, leafCount - 1, leaf);
        return true;
    }

    // Statistics for zones [zoneFrom, zoneTo] and ESI levels [esiFrom, esiTo] (1-based, inclusive).
    BucketStats query(int zoneFrom, int zoneTo, int esiFrom, int esiTo) const {
        BucketStats result;
        if (zoneFrom < 1 || zoneTo > zones || zoneFrom > zoneTo || esiFrom < 1 ||
            esiTo > esiLevels || esiFrom > esiTo) {
            return result;
        }
        for (int esi = esiFrom; esi <= esiTo; ++esi) {
            int left = leafIndex(zoneFrom, esi);
            int right = leafIndex(zoneTo, esi);
            result = combineBuckets(result, queryNode(1, 0, leafCount - 1, left, right));
        }
        return result;
    }

    // Segment-tree nodes touched by the last query() calls (reset with resetNodeCount()).
    long long nodesVisited() const { return visited; }
    void resetNodeCount() const { visited = 0; }

    int getLeafCount() const { return leafCount; }

    // Rebuilds every node from the buckets and compares; used by the tests.
    bool isConsistent() const {
        for (int leaf = 0; leaf < leafCount; ++leaf) {
            BucketStats expected = statsOf(buckets[leaf]);
            BucketStats actual = queryNode(1, 0, leafCount - 1, leaf, leaf);
            if (expected.count != actual.count || expected.sumArrival != actual.sumArrival ||
                expected.oldestArrival != actual.oldestArrival ||
                expected.newestArrival != actual.newestArrival) {
                return false;
            }
        }
        return true;
    }

private:
    struct Bucket {
        std::multiset<int> arrivals;
        long long sum = 0;
    };

    int zones;
    int esiLevels;
    int leafCount;
    std::vector<BucketStats> tree;
    std::vector<Bucket> buckets;
    mutable long long visited = 0;

    int leafIndex(int zone, int esi) const {
        if (zone < 1 || zone > zones || esi < 1 || esi > esiLevels) {
            return -1;
        }
        return (esi - 1) * zones + (zone - 1);
    }

    static BucketStats statsOf(const Bucket& bucket) {
        BucketStats stats;
        if (!bucket.arrivals.empty()) {
            stats.count = static_cast<int>(bucket.arrivals.size());
            stats.sumArrival = bucket.sum;
            stats.oldestArrival = *bucket.arrivals.begin();
            stats.newestArrival = *bucket.arrivals.rbegin();
        }
        return stats;
    }

    void refreshLeaf(int node, int start, int end, int leaf) {
        if (start == end) {
            tree[node] = statsOf(buckets[leaf]);
            return;
        }
        int mid = (start + end) / 2;
        if (leaf <= mid) {
            refreshLeaf(2 * node, start, mid, leaf);
        } else {
            refreshLeaf(2 * node + 1, mid + 1, end, leaf);
        }
        tree[node] = combineBuckets(tree[2 * node], tree[2 * node + 1]);
    }

    BucketStats queryNode(int node, int start, int end, int left, int right) const {
        ++visited;
        if (right < start || end < left) {
            return BucketStats{};
        }
        if (left <= start && end <= right) {
            return tree[node];
        }
        int mid = (start + end) / 2;
        return combineBuckets(queryNode(2 * node, start, mid, left, right),
                              queryNode(2 * node + 1, mid + 1, end, left, right));
    }
};
