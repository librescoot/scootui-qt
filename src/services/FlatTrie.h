#pragma once

#include <QChar>
#include <QString>
#include <QStringList>
#include <QList>
#include <cstdint>
#include <vector>

// A flat index-based trie. Nodes, child edges, and display name strings live
// in three contiguous std::vectors instead of the classic "one heap allocation
// per node with a QHash of children" layout. This eliminates both Qt6 QHash
// minimum-allocation overhead (~1 KB per child hash) and the heap fragmentation
// caused by hundreds of thousands of tiny new/delete calls during the build
// phase.
//
// Memory footprint for a trie of ~50k terminal strings with ~300k nodes:
//   ~6 MB for nodes, ~2.5 MB for child refs, ~2 MB for display names.
// Compared to the pointer+QHash layout that took ~130 MB for the same content.
//
// Usage: construct, insert() all strings, call finalize() once, then query.
// Attempting to insert after finalize() or query before finalize() is a bug.
class FlatTrie
{
public:
    static constexpr uint32_t INVALID = ~uint32_t(0);

    FlatTrie();

    FlatTrie(const FlatTrie &) = delete;
    FlatTrie &operator=(const FlatTrie &) = delete;
    FlatTrie(FlatTrie &&) = default;
    FlatTrie &operator=(FlatTrie &&) = default;

    // Build-phase API. Call finalize() once when all strings are inserted.
    void insert(const QString &normalized, const QString &displayName);
    void finalize();
    bool isFinalized() const { return m_finalized; }

    // Query-phase API. All node indices are opaque uint32_t handles; 0 is root.
    uint32_t root() const { return 0; }
    bool valid(uint32_t node) const { return node != INVALID; }

    // Walk the trie from `start` following the characters of `prefix`.
    // Returns INVALID if the prefix is not present.
    uint32_t findNode(uint32_t start, const QString &prefix) const;
    uint32_t findNode(const QString &prefix) const { return findNode(0, prefix); }

    // Does this node mark the end of an inserted string (has a display name)?
    bool hasDisplayName(uint32_t node) const;
    QString displayName(uint32_t node) const;

    // Number of distinct terminal nodes (= inserted strings) in the subtree
    // rooted at `node`. Precomputed during finalize() so queries are O(1).
    int subtreeUniqueCount(uint32_t node) const;

    // Depth-first collection of all display names in the subtree.
    QStringList collectDisplayNames(uint32_t node) const;

    // Characters leading to direct children of `node`. Returned in insertion
    // order during build; sorted after finalize() for deterministic output.
    QList<QChar> childChars(uint32_t node) const;

private:
    struct Node {
        // Final state (set by finalize()):
        uint32_t firstChild = 0;    // index into m_childRefs
        uint32_t childCount = 0;
        int32_t subtreeUniqueCount = 0;
        uint32_t displayNameIdx = 0; // 0 = no display name
    };
    struct ChildRef {
        char16_t ch = 0;
        uint32_t nodeIdx = 0;
    };

    // Build-time scratch. Freed on finalize(). Each entry is a flat vector of
    // edges out of the node at the same index in m_nodes. We deliberately
    // avoid per-node Qt containers here — this temporary structure was the
    // source of ~100 MB of QHash minimum-allocation overhead.
    std::vector<std::vector<ChildRef>> m_buildChildren;

    std::vector<Node> m_nodes;
    std::vector<ChildRef> m_childRefs;
    std::vector<QString> m_displayNames; // index 0 is always an empty QString

    bool m_finalized = false;

    int computeSubtreeCounts(uint32_t node);
    void collectDisplayNamesImpl(uint32_t node, QStringList &out) const;
};
