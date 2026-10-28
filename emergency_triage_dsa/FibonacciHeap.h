#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "HeapKey.h"

class FibonacciHeap {
public:
    explicit FibonacciHeap(int maxId) : handle(maxId, nullptr) {}

    FibonacciHeap(const FibonacciHeap&) = delete;
    FibonacciHeap& operator=(const FibonacciHeap&) = delete;

    ~FibonacciHeap() {
        std::vector<Node*> pending;
        if (minNode) pending.push_back(minNode);
        while (!pending.empty()) {
            Node* start = pending.back();
            pending.pop_back();
            Node* node = start;
            do {
                Node* next = node->right;
                if (node->child) pending.push_back(node->child);
                delete node;
                node = next;
            } while (node != start);
        }
    }

    bool insert(int id, HeapKey key) {
        if (id < 0 || id >= static_cast<int>(handle.size()) || handle[id] != nullptr) {
            return false;
        }
        Node* node = new Node{id, key, nullptr, nullptr, nullptr, nullptr, 0, false};
        node->left = node->right = node;
        handle[id] = node;
        addToRootList(node);
        ++count;
        ++touched;
        return true;
    }

    bool decreaseKey(int id, HeapKey newKey) {
        Node* node = lookup(id);
        if (node == nullptr || newKey >= node->key) {
            return false;
        }
        node->key = newKey;
        ++touched;
        Node* parent = node->parent;
        if (parent != nullptr && node->key < parent->key) {
            cut(node, parent);
            cascadingCut(parent);
        }
        if (node->key < minNode->key) {
            minNode = node;
        }
        return true;
    }

    bool extractMin(int& id, HeapKey& key) {
        Node* smallest = minNode;
        if (smallest == nullptr) {
            return false;
        }
        id = smallest->id;
        key = smallest->key;

        if (Node* firstChild = smallest->child) {
            Node* child = firstChild;
            do {
                Node* next = child->right;
                child->parent = nullptr;
                child->mark = false;
                addToRootList(child);
                ++touched;
                child = next;
            } while (child != firstChild);
            smallest->child = nullptr;
        }

        Node* replacement = smallest->right == smallest ? nullptr : smallest->right;
        unlink(smallest);
        handle[id] = nullptr;
        delete smallest;
        --count;
        ++touched;

        minNode = replacement;
        if (minNode != nullptr) {
            consolidate();
        }
        return true;
    }

    bool contains(int id) const { return lookup(id) != nullptr; }

    bool peekMin(int& id, HeapKey& key) const {
        if (minNode == nullptr) {
            return false;
        }
        id = minNode->id;
        key = minNode->key;
        return true;
    }

    int size() const { return count; }
    bool isEmpty() const { return count == 0; }

    std::uint64_t touches() const { return touched; }

    std::size_t memoryBytes() const {
        constexpr std::size_t nodeBytes = ((sizeof(Node) + 8 + 15) / 16) * 16;
        return static_cast<std::size_t>(count) * nodeBytes +
               handle.capacity() * sizeof(Node*) + degreeTable.capacity() * sizeof(Node*);
    }

    bool isValid() const {
        if (minNode == nullptr) return count == 0;
        int seen = 0;
        std::vector<const Node*> pending;
        pending.push_back(minNode);
        bool minOk = true;
        while (!pending.empty()) {
            const Node* start = pending.back();
            pending.pop_back();
            const Node* node = start;
            int siblings = 0;
            do {
                ++seen;
                ++siblings;
                if (node->right->left != node || node->left->right != node) return false;
                if (handle[node->id] != node) return false;
                if (node->parent == nullptr && node->key < minNode->key) minOk = false;
                if (node->parent != nullptr && node->key < node->parent->key) return false;
                int children = 0;
                if (node->child != nullptr) {
                    const Node* c = node->child;
                    do {
                        if (c->parent != node) return false;
                        ++children;
                        c = c->right;
                    } while (c != node->child);
                    pending.push_back(node->child);
                }
                if (children != node->degree) return false;
                node = node->right;
            } while (node != start && siblings <= count);
            if (node != start) return false;
        }
        return minOk && seen == count;
    }

    int rootCount() const {
        if (minNode == nullptr) return 0;
        int roots = 0;
        const Node* node = minNode;
        do {
            ++roots;
            node = node->right;
        } while (node != minNode);
        return roots;
    }

private:
    struct Node {
        int id;
        HeapKey key;
        Node* parent;
        Node* child;
        Node* left;
        Node* right;
        int degree;
        bool mark;
    };

    std::vector<Node*> handle;
    std::vector<Node*> degreeTable;
    Node* minNode = nullptr;
    int count = 0;
    std::uint64_t touched = 0;

    Node* lookup(int id) const {
        return (id >= 0 && id < static_cast<int>(handle.size())) ? handle[id] : nullptr;
    }

    void addToRootList(Node* node) {
        node->parent = nullptr;
        if (minNode == nullptr) {
            node->left = node->right = node;
            minNode = node;
            return;
        }
        node->left = minNode;
        node->right = minNode->right;
        minNode->right->left = node;
        minNode->right = node;
        if (node->key < minNode->key) {
            minNode = node;
        }
    }

    static void unlink(Node* node) {
        node->left->right = node->right;
        node->right->left = node->left;
        node->left = node->right = node;
    }

    void link(Node* child, Node* parent) {
        unlink(child);
        child->parent = parent;
        child->mark = false;
        if (parent->child == nullptr) {
            parent->child = child;
        } else {
            child->left = parent->child;
            child->right = parent->child->right;
            parent->child->right->left = child;
            parent->child->right = child;
        }
        ++parent->degree;
        touched += 2;
    }

    void cut(Node* node, Node* parent) {
        if (parent->child == node) {
            parent->child = node->right == node ? nullptr : node->right;
        }
        unlink(node);
        --parent->degree;
        node->mark = false;
        addToRootList(node);
        touched += 2;
    }

    void cascadingCut(Node* node) {
        while (node->parent != nullptr) {
            ++touched;
            if (!node->mark) {
                node->mark = true;
                return;
            }
            Node* parent = node->parent;
            cut(node, parent);
            node = parent;
        }
    }

    void consolidate() {
        int bound = 2;
        for (long long n = count; n > 0; n >>= 1) bound += 2;
        degreeTable.assign(bound, nullptr);

        std::vector<Node*> roots;
        Node* node = minNode;
        do {
            roots.push_back(node);
            node = node->right;
        } while (node != minNode);

        for (Node* root : roots) {
            ++touched;
            Node* current = root;
            int degree = current->degree;
            while (degreeTable[degree] != nullptr) {
                Node* other = degreeTable[degree];
                if (other->key < current->key) {
                    Node* swap = current;
                    current = other;
                    other = swap;
                }
                link(other, current);
                degreeTable[degree] = nullptr;
                ++degree;
            }
            degreeTable[degree] = current;
        }

        minNode = nullptr;
        for (Node* root : degreeTable) {
            if (root == nullptr) continue;
            if (minNode == nullptr) {
                root->left = root->right = root;
                minNode = root;
            } else {
                root->left = minNode;
                root->right = minNode->right;
                minNode->right->left = root;
                minNode->right = root;
                if (root->key < minNode->key) minNode = root;
            }
        }
    }
};
