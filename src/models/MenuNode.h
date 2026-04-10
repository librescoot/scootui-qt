#pragma once

#include <QString>
#include <QList>
#include <functional>

enum class MenuNodeType { Action, Submenu, Setting, CycleSetting };

struct CycleOption {
    QString label;
    std::function<void()> action;
};

class MenuNode
{
public:
    MenuNode(const QString &id, const QString &title, MenuNodeType type,
             MenuNode *parent = nullptr)
        : m_id(id), m_title(title), m_type(type), m_parent(parent) {}

    ~MenuNode() { qDeleteAll(m_children); }

    // Accessors
    QString id() const { return m_id; }
    QString title() const { return m_title; }
    QString headerTitle() const { return m_headerTitle.isEmpty() ? m_title.toUpper() : m_headerTitle; }
    MenuNodeType type() const { return m_type; }
    MenuNode *parent() const { return m_parent; }
    int currentValue() const { return m_currentValue; }
    const QList<MenuNode*> &children() const { return m_children; }
    const QList<CycleOption> &cycleOptions() const { return m_cycleOptions; }
    int cycleIndex() const { return m_cycleIndex; }
    QString currentValueLabel() const {
        if (m_cycleOptions.isEmpty()) return {};
        return m_cycleOptions[m_cycleIndex].label;
    }

    // Setters
    void setHeaderTitle(const QString &t) { m_headerTitle = t; }
    void setCurrentValue(int v) { m_currentValue = v; }
    void setOnAction(std::function<void()> fn) { m_onAction = std::move(fn); }
    void setIsVisible(std::function<bool()> fn) { m_isVisible = std::move(fn); }

    // Cycle operations
    void cycleNext() {
        if (m_cycleOptions.isEmpty()) return;
        int next = (m_cycleIndex + 1) % m_cycleOptions.size();
        if (m_cycleOptions[next].action)
            m_cycleOptions[next].action();
    }

    // Tree operations
    MenuNode *addChild(MenuNode *child) {
        child->m_parent = this;
        m_children.append(child);
        return child;
    }

    QList<MenuNode*> visibleChildren() const {
        QList<MenuNode*> result;
        for (auto *c : m_children) {
            if (!c->m_isVisible || c->m_isVisible())
                result.append(c);
        }
        return result;
    }

    void executeAction() {
        if (m_onAction) m_onAction();
    }

    std::function<void()> action() const { return m_onAction; }

    bool hasChildren() const { return !m_children.isEmpty(); }

    // Factory helpers
    static MenuNode *action(const QString &id, const QString &title,
                            std::function<void()> onAction = nullptr,
                            std::function<bool()> isVisible = nullptr) {
        auto *n = new MenuNode(id, title, MenuNodeType::Action);
        n->m_onAction = std::move(onAction);
        n->m_isVisible = std::move(isVisible);
        return n;
    }

    static MenuNode *submenu(const QString &id, const QString &title,
                             const QString &headerTitle = {},
                             std::function<bool()> isVisible = nullptr) {
        auto *n = new MenuNode(id, title, MenuNodeType::Submenu);
        n->m_headerTitle = headerTitle;
        n->m_isVisible = std::move(isVisible);
        return n;
    }

    static MenuNode *setting(const QString &id, const QString &title,
                             int currentValue, std::function<void()> onAction = nullptr) {
        auto *n = new MenuNode(id, title, MenuNodeType::Setting);
        n->m_currentValue = currentValue;
        n->m_onAction = std::move(onAction);
        return n;
    }

    static MenuNode *cycleSetting(const QString &id, const QString &title,
                                   QList<CycleOption> options, int currentIndex,
                                   std::function<bool()> isVisible = nullptr) {
        auto *n = new MenuNode(id, title, MenuNodeType::CycleSetting);
        n->m_cycleOptions = std::move(options);
        n->m_cycleIndex = qBound(0, currentIndex, qMax(0, n->m_cycleOptions.size() - 1));
        n->m_isVisible = std::move(isVisible);
        return n;
    }

private:
    QString m_id;
    QString m_title;
    QString m_headerTitle;
    MenuNodeType m_type;
    MenuNode *m_parent = nullptr;
    int m_currentValue = 0;
    QList<MenuNode*> m_children;
    std::function<void()> m_onAction;
    std::function<bool()> m_isVisible;
    QList<CycleOption> m_cycleOptions;
    int m_cycleIndex = 0;
};
