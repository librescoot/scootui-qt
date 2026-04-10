#include "FlatTrie.h"

#include <algorithm>

FlatTrie::FlatTrie()
{
    // Node 0 is the root. Display name slot 0 is an empty QString sentinel
    // so that Node::displayNameIdx == 0 cleanly means "no display name".
    m_nodes.push_back({});
    m_buildChildren.emplace_back();
    m_displayNames.emplace_back();
}

void FlatTrie::insert(const QString &normalized, const QString &displayName)
{
    Q_ASSERT(!m_finalized);

    uint32_t current = 0;
    for (QChar qc : normalized) {
        char16_t c = qc.unicode();
        auto &children = m_buildChildren[current];
        auto it = std::find_if(children.begin(), children.end(),
                               [c](const ChildRef &r) { return r.ch == c; });
        if (it != children.end()) {
            current = it->nodeIdx;
            continue;
        }
        uint32_t newIdx = uint32_t(m_nodes.size());
        m_nodes.push_back({});
        m_buildChildren.emplace_back();
        m_buildChildren[current].push_back({c, newIdx});
        current = newIdx;
    }

    if (!displayName.isEmpty() && m_nodes[current].displayNameIdx == 0) {
        m_displayNames.push_back(displayName);
        m_nodes[current].displayNameIdx = uint32_t(m_displayNames.size() - 1);
    }
}

void FlatTrie::finalize()
{
    if (m_finalized)
        return;

    // Pre-count total edges so m_childRefs can be reserved once.
    size_t totalEdges = 0;
    for (const auto &v : m_buildChildren)
        totalEdges += v.size();
    m_childRefs.clear();
    m_childRefs.reserve(totalEdges);

    for (size_t i = 0; i < m_nodes.size(); ++i) {
        auto &children = m_buildChildren[i];
        std::sort(children.begin(), children.end(),
                  [](const ChildRef &a, const ChildRef &b) { return a.ch < b.ch; });
        m_nodes[i].firstChild = uint32_t(m_childRefs.size());
        m_nodes[i].childCount = uint32_t(children.size());
        for (const auto &c : children)
            m_childRefs.push_back(c);
    }

    // Free the build-time scratch. swap-with-empty guarantees the capacity is
    // returned to the allocator instead of lingering.
    std::vector<std::vector<ChildRef>>().swap(m_buildChildren);

    m_nodes.shrink_to_fit();
    m_displayNames.shrink_to_fit();

    m_finalized = true;
    computeSubtreeCounts(0);
}

uint32_t FlatTrie::findNode(uint32_t start, const QString &prefix) const
{
    Q_ASSERT(m_finalized);
    if (start >= m_nodes.size())
        return INVALID;

    uint32_t current = start;
    for (QChar qc : prefix) {
        char16_t c = qc.unicode();
        const Node &n = m_nodes[current];
        // Binary search within the sorted [firstChild, firstChild + childCount) range.
        const ChildRef *first = m_childRefs.data() + n.firstChild;
        const ChildRef *last = first + n.childCount;
        auto it = std::lower_bound(first, last, c,
            [](const ChildRef &r, char16_t c) { return r.ch < c; });
        if (it == last || it->ch != c)
            return INVALID;
        current = it->nodeIdx;
    }
    return current;
}

bool FlatTrie::hasDisplayName(uint32_t node) const
{
    Q_ASSERT(m_finalized);
    return node < m_nodes.size() && m_nodes[node].displayNameIdx != 0;
}

QString FlatTrie::displayName(uint32_t node) const
{
    Q_ASSERT(m_finalized);
    if (node >= m_nodes.size())
        return {};
    uint32_t idx = m_nodes[node].displayNameIdx;
    if (idx == 0 || idx >= m_displayNames.size())
        return {};
    return m_displayNames[idx];
}

int FlatTrie::subtreeUniqueCount(uint32_t node) const
{
    Q_ASSERT(m_finalized);
    if (node >= m_nodes.size())
        return 0;
    return m_nodes[node].subtreeUniqueCount;
}

QStringList FlatTrie::collectDisplayNames(uint32_t node) const
{
    Q_ASSERT(m_finalized);
    QStringList out;
    if (node < m_nodes.size())
        collectDisplayNamesImpl(node, out);
    return out;
}

QList<QChar> FlatTrie::childChars(uint32_t node) const
{
    Q_ASSERT(m_finalized);
    QList<QChar> out;
    if (node >= m_nodes.size())
        return out;
    const Node &n = m_nodes[node];
    out.reserve(int(n.childCount));
    for (uint32_t i = 0; i < n.childCount; ++i)
        out.append(QChar(m_childRefs[n.firstChild + i].ch));
    return out;
}

int FlatTrie::computeSubtreeCounts(uint32_t node)
{
    int count = (m_nodes[node].displayNameIdx != 0) ? 1 : 0;
    uint32_t first = m_nodes[node].firstChild;
    uint32_t n = m_nodes[node].childCount;
    for (uint32_t i = 0; i < n; ++i)
        count += computeSubtreeCounts(m_childRefs[first + i].nodeIdx);
    m_nodes[node].subtreeUniqueCount = count;
    return count;
}

void FlatTrie::collectDisplayNamesImpl(uint32_t node, QStringList &out) const
{
    const Node &n = m_nodes[node];
    if (n.displayNameIdx != 0)
        out.append(m_displayNames[n.displayNameIdx]);
    uint32_t first = n.firstChild;
    for (uint32_t i = 0; i < n.childCount; ++i)
        collectDisplayNamesImpl(m_childRefs[first + i].nodeIdx, out);
}
