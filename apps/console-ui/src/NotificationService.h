#pragma once

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <chrono>
#include <deque>
#include <optional>

namespace nexora::ui {

// Central notification queue for the console UI (docs/notifications.md).
//
// Producers call post(); the UI shows |current| (one at a time, Toast.qml) and |history|.
// Policy, all testable from C++:
//   - dedup: the same key (default: category + text) within dedupWindow() increments the
//     count of the existing entry instead of showing the event again;
//   - queue: critical notifications go to the front; each one is shown for durationMs
//     (info), 2x (warning) or 3x (critical);
//   - quiet mode and hidden categories: info (quiet) or any non-critical notification of a
//     hidden category only goes to the history;
//   - history: newest first, at most 100 in memory; warnings and criticals (newest 50) are
//     also written to historyFile and loaded again at start, so a failure reported while
//     shutting down is still visible after the next boot.
class NotificationService : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by the application")

    // Empty map when nothing is shown; otherwise {id, category, severity, text, count, createdAt}.
    Q_PROPERTY(QVariantMap current READ current NOTIFY currentChanged)
    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)
    Q_PROPERTY(bool quiet READ quiet NOTIFY settingsChanged)

public:
    enum class Category { Controller, Game, SaveState, Library, Storage, Update, System, Recovery };
    enum class Severity { Info, Warning, Critical };

    struct Notification
    {
        quint64 id = 0;
        Category category = Category::System;
        Severity severity = Severity::Info;
        QString text;
        QString key;
        int count = 1;
        QDateTime createdAt; // UTC, first occurrence
        QDateTime updatedAt; // UTC, last occurrence
        bool fromPreviousRun = false; // loaded from historyFile
    };

    static constexpr int MaxHistory = 100;
    static constexpr int MaxPersisted = 50;

    // |historyFile| empty = no persistence (tests, --check-config).
    explicit NotificationService(const QString &historyFile = {}, QObject *parent = nullptr);

    // Queues |text|; see the class comment. Empty text is ignored. Returns the id of the entry
    // the event ended up in (a new one or the deduplicated one).
    quint64 post(Category category, Severity severity, const QString &text, const QString &key = {});
    // For QML producers: |category| by name ("library", ...), |error| = warning, else info.
    Q_INVOKABLE void postMessage(const QString &category, const QString &text, bool error);
    // Hides the current notification and shows the next one, if any.
    Q_INVOKABLE void dismiss();
    Q_INVOKABLE void clearHistory();

    QVariantMap current() const;
    QVariantList history() const;
    const std::optional<Notification> &currentNotification() const { return m_current; }
    const QList<Notification> &historyEntries() const { return m_history; }
    int queuedCount() const { return int(m_queue.size()); }

    bool quiet() const { return m_quiet; }
    void setQuiet(bool quiet);
    void setHiddenCategories(const QStringList &categories);
    void setDurationMs(int ms);
    int durationMs() const { return m_durationMs; }
    void setDedupWindow(std::chrono::milliseconds window) { m_dedupWindow = window; }
    std::chrono::milliseconds dedupWindow() const { return m_dedupWindow; }

    // Reads historyFile (warnings and criticals of earlier runs). Missing file is fine; an
    // unreadable one is moved aside.
    void loadHistory();

    static QString categoryName(Category category);
    static std::optional<Category> categoryFromName(const QString &name);
    static QString severityName(Severity severity);

signals:
    void currentChanged();
    void historyChanged();
    void settingsChanged();

private:
    bool shouldDisplay(const Notification &n) const;
    Notification *findRecent(const QString &key);
    void showNext();
    void addToHistory(const Notification &n);
    void persist() const;
    std::chrono::milliseconds displayTime(Severity severity) const;
    static QVariantMap toMap(const Notification &n);

    QString m_historyFile;
    std::optional<Notification> m_current;
    std::deque<Notification> m_queue;
    QList<Notification> m_history; // newest first
    QTimer m_hideTimer;
    quint64 m_nextId = 1;
    bool m_quiet = false;
    QList<Category> m_hidden;
    int m_durationMs = 3500;
    std::chrono::milliseconds m_dedupWindow{10000};
};

} // namespace nexora::ui
