#pragma once

#include <algorithm>
#include <climits>
#include <queue>
#include <vector>

class HopcroftKarp {
public:
    HopcroftKarp(int leftCount, int rightCount)
        : left(leftCount),
          right(rightCount),
          adjacency(leftCount),
          matchOfLeft(leftCount, NONE),
          matchOfRight(rightCount, NONE),
          layer(leftCount) {}

    void addEdge(int u, int v) {
        if (u >= 0 && u < left && v >= 0 && v < right) {
            adjacency[u].push_back(v);
        }
    }

    int run(bool greedyInit = true) {
        std::fill(matchOfLeft.begin(), matchOfLeft.end(), NONE);
        std::fill(matchOfRight.begin(), matchOfRight.end(), NONE);
        phaseCount = 0;
        int matched = 0;

        if (greedyInit) {
            for (int u = 0; u < left; ++u) {
                for (int v : adjacency[u]) {
                    if (matchOfRight[v] == NONE) {
                        matchOfLeft[u] = v;
                        matchOfRight[v] = u;
                        ++matched;
                        break;
                    }
                }
            }
        }
        while (buildLayers()) {
            ++phaseCount;
            nextEdge.assign(left, 0);
            for (int u = 0; u < left; ++u) {
                if (matchOfLeft[u] == NONE && augment(u)) {
                    ++matched;
                }
            }
        }
        return matched;
    }

    int bedOfPatient(int u) const { return matchOfLeft[u]; }
    int patientOfBed(int v) const { return matchOfRight[v]; }
    int phases() const { return phaseCount; }

    bool isValidMatching() const {
        for (int u = 0; u < left; ++u) {
            int v = matchOfLeft[u];
            if (v == NONE) continue;
            if (v < 0 || v >= right || matchOfRight[v] != u) return false;
            bool isEdge = false;
            for (int w : adjacency[u]) isEdge = isEdge || (w == v);
            if (!isEdge) return false;
        }
        for (int v = 0; v < right; ++v) {
            if (matchOfRight[v] != NONE && matchOfLeft[matchOfRight[v]] != v) return false;
        }
        return true;
    }

private:
    static constexpr int NONE = -1;

    int left;
    int right;
    std::vector<std::vector<int>> adjacency;
    std::vector<int> matchOfLeft;
    std::vector<int> matchOfRight;
    std::vector<int> layer;
    std::vector<int> nextEdge;
    int phaseCount = 0;
    int shortestPath = INT_MAX;

    bool buildLayers() {
        std::queue<int> frontier;
        for (int u = 0; u < left; ++u) {
            if (matchOfLeft[u] == NONE) {
                layer[u] = 0;
                frontier.push(u);
            } else {
                layer[u] = INT_MAX;
            }
        }
        shortestPath = INT_MAX;
        while (!frontier.empty()) {
            int u = frontier.front();
            frontier.pop();
            if (layer[u] > shortestPath) {
                break;
            }
            for (int v : adjacency[u]) {
                int next = matchOfRight[v];
                if (next == NONE) {
                    shortestPath = std::min(shortestPath, layer[u]);
                } else if (layer[next] == INT_MAX) {
                    layer[next] = layer[u] + 1;
                    frontier.push(next);
                }
            }
        }
        return shortestPath != INT_MAX;
    }

    bool augment(int u) {
        for (int& i = nextEdge[u]; i < static_cast<int>(adjacency[u].size()); ++i) {
            int v = adjacency[u][i];
            int next = matchOfRight[v];
            bool endsHere = next == NONE && layer[u] == shortestPath;
            if (endsHere || (next != NONE && layer[next] == layer[u] + 1 && augment(next))) {
                matchOfLeft[u] = v;
                matchOfRight[v] = u;
                return true;
            }
        }
        layer[u] = INT_MAX;
        return false;
    }
};
