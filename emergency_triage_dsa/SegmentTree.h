#pragma once

#include <vector>

struct ZoneStats {
    int patientCount = 0;
    int totalWaitingTime = 0;
    int totalCapacity = 0;

    int busiestZone = -1;
    int busiestLoad = 0;
};

inline ZoneStats combineStats(const ZoneStats& left, const ZoneStats& right) {
    ZoneStats result;
    result.patientCount = left.patientCount + right.patientCount;
    result.totalWaitingTime = left.totalWaitingTime + right.totalWaitingTime;
    result.totalCapacity = left.totalCapacity + right.totalCapacity;

    bool takeRight = left.busiestZone == -1 ||
                     (right.busiestZone != -1 && right.busiestLoad > left.busiestLoad);
    const ZoneStats& winner = takeRight ? right : left;
    result.busiestZone = winner.busiestZone;
    result.busiestLoad = winner.busiestLoad;
    return result;
}

inline ZoneStats makeLeaf(const ZoneStats& values, int zoneIndex) {
    ZoneStats leaf = values;
    leaf.busiestZone = zoneIndex;
    leaf.busiestLoad = values.patientCount - values.totalCapacity;
    return leaf;
}

class SegmentTree {
public:
    explicit SegmentTree(int numberOfZones)
        : zoneCount(numberOfZones), tree(4 * numberOfZones) {}

    bool build(const std::vector<ZoneStats>& zoneValues) {
        if (static_cast<int>(zoneValues.size()) != zoneCount || zoneCount == 0) {
            return false;
        }
        buildNode(1, 0, zoneCount - 1, zoneValues);
        return true;
    }

    bool update(int zoneIndex, const ZoneStats& newValues) {
        if (zoneIndex < 0 || zoneIndex >= zoneCount) {
            return false;
        }
        updateNode(1, 0, zoneCount - 1, zoneIndex, newValues);
        return true;
    }

    ZoneStats query(int left, int right) const {
        if (left < 0 || right >= zoneCount || left > right) {
            return ZoneStats{};
        }
        return queryNode(1, 0, zoneCount - 1, left, right);
    }

    ZoneStats getZone(int zoneIndex) const {
        return query(zoneIndex, zoneIndex);
    }

    int getZoneCount() const {
        return zoneCount;
    }

private:
    int zoneCount;
    std::vector<ZoneStats> tree;

    void buildNode(int node, int start, int end, const std::vector<ZoneStats>& zoneValues) {
        if (start == end) {
            tree[node] = makeLeaf(zoneValues[start], start);
            return;
        }
        int mid = (start + end) / 2;
        buildNode(2 * node, start, mid, zoneValues);
        buildNode(2 * node + 1, mid + 1, end, zoneValues);
        tree[node] = combineStats(tree[2 * node], tree[2 * node + 1]);
    }

    void updateNode(int node, int start, int end, int zoneIndex, const ZoneStats& newValues) {
        if (start == end) {
            tree[node] = makeLeaf(newValues, start);
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
