#pragma once

// SegmentTree.h
// Segment tree over the department zones. Each leaf stores one zone's statistics;
// each internal node stores the combined statistics of all zones in its range.
//
// Internal indexing:
//   - zones are 0-based inside the tree (zone 1 in the UI is index 0)
//   - node 1 is the root and covers [0, zoneCount - 1]
//   - node v has children 2v (left half) and 2v + 1 (right half)

#include <vector>

// Aggregate statistics for one zone or a range of zones.
struct ZoneStats {
    int patientCount = 0;      // number of waiting patients
    int totalWaitingTime = 0;  // sum of their waiting minutes
    int totalCapacity = 0;     // number of treatment spaces
};

// Combines the results of two disjoint ranges. Every field is a sum, so this is
// associative, and ZoneStats{} (all zeros) is the identity for an empty range.
inline ZoneStats combineStats(const ZoneStats& left, const ZoneStats& right) {
    ZoneStats result;
    result.patientCount = left.patientCount + right.patientCount;
    result.totalWaitingTime = left.totalWaitingTime + right.totalWaitingTime;
    result.totalCapacity = left.totalCapacity + right.totalCapacity;
    return result;
}

class SegmentTree {
public:
    explicit SegmentTree(int numberOfZones)
        : zoneCount(numberOfZones), tree(4 * numberOfZones) {}  // 4k nodes is always enough

    // Builds the tree from one ZoneStats per zone. O(k).
    bool build(const std::vector<ZoneStats>& zoneValues) {
        if (static_cast<int>(zoneValues.size()) != zoneCount || zoneCount == 0) {
            return false;
        }
        buildNode(1, 0, zoneCount - 1, zoneValues);
        return true;
    }

    // Point update: replaces one zone's statistics (0-based index). O(log k).
    bool update(int zoneIndex, const ZoneStats& newValues) {
        if (zoneIndex < 0 || zoneIndex >= zoneCount) {
            return false;
        }
        updateNode(1, 0, zoneCount - 1, zoneIndex, newValues);
        return true;
    }

    // Range query over zones [left, right] (0-based, inclusive). O(log k).
    // An invalid range returns empty statistics.
    ZoneStats query(int left, int right) const {
        if (left < 0 || right >= zoneCount || left > right) {
            return ZoneStats{};
        }
        return queryNode(1, 0, zoneCount - 1, left, right);
    }

    // Statistics of a single zone (0-based). O(log k).
    ZoneStats getZone(int zoneIndex) const {
        return query(zoneIndex, zoneIndex);
    }

    int getZoneCount() const {
        return zoneCount;
    }

private:
    int zoneCount;
    std::vector<ZoneStats> tree;

    // Node `node` covers zones [start, end].
    void buildNode(int node, int start, int end, const std::vector<ZoneStats>& zoneValues) {
        if (start == end) {
            tree[node] = zoneValues[start];  // leaf = one zone
            return;
        }
        int mid = (start + end) / 2;
        buildNode(2 * node, start, mid, zoneValues);
        buildNode(2 * node + 1, mid + 1, end, zoneValues);
        tree[node] = combineStats(tree[2 * node], tree[2 * node + 1]);
    }

    // Walks one root-to-leaf path (height O(log k)), replaces the leaf, then
    // recombines each node on the way back up. No other node is touched.
    void updateNode(int node, int start, int end, int zoneIndex, const ZoneStats& newValues) {
        if (start == end) {
            tree[node] = newValues;
            return;
        }
        int mid = (start + end) / 2;
        if (zoneIndex <= mid) {
            updateNode(2 * node, start, mid, zoneIndex, newValues);
        } else {
            updateNode(2 * node + 1, mid + 1, end, zoneIndex, newValues);
        }
        tree[node] = combineStats(tree[2 * node], tree[2 * node + 1]);
    }

    // Three cases:
    //   no overlap      -> contributes nothing (identity)
    //   full overlap    -> use this node's stored total without going deeper
    //   partial overlap -> ask both children and combine
    // At most about 4 nodes are visited per level, so the cost is O(log k).
    ZoneStats queryNode(int node, int start, int end, int left, int right) const {
        if (right < start || end < left) {
            return ZoneStats{};
        }
        if (left <= start && end <= right) {
            return tree[node];
        }
        int mid = (start + end) / 2;
        ZoneStats leftResult = queryNode(2 * node, start, mid, left, right);
        ZoneStats rightResult = queryNode(2 * node + 1, mid + 1, end, left, right);
        return combineStats(leftResult, rightResult);
    }
};
