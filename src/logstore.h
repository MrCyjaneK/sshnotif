#ifndef LOGSTORE_H
#define LOGSTORE_H

#include <QObject>
#include <QVariantList>
#include <QFileSystemWatcher>
#include <QTimer>

class LogStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList events READ events NOTIFY eventsChanged)
    Q_PROPERTY(bool hookInstalled READ hookInstalled NOTIFY statusChanged)
    Q_PROPERTY(bool pamConfigured READ pamConfigured NOTIFY statusChanged)
    Q_PROPERTY(bool logReadable READ logReadable NOTIFY statusChanged)
    Q_PROPERTY(bool deployed READ deployed NOTIFY statusChanged)
    Q_PROPERTY(QString statusSummary READ statusSummary NOTIFY statusChanged)
    Q_PROPERTY(qint64 snoozeUntil READ snoozeUntil NOTIFY snoozeChanged)
    Q_PROPERTY(bool snoozed READ snoozed NOTIFY snoozeChanged)
    Q_PROPERTY(QString snoozeRemainingText READ snoozeRemainingText NOTIFY snoozeChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY statusChanged)

public:
    explicit LogStore(QObject *parent = 0);

    QVariantList events() const { return m_events; }
    bool hookInstalled() const { return m_hookInstalled; }
    bool pamConfigured() const { return m_pamConfigured; }
    bool logReadable() const { return m_logReadable; }
    bool deployed() const { return m_hookInstalled && m_pamConfigured; }
    QString statusSummary() const;
    QString lastError() const { return m_lastError; }
    qint64 snoozeUntil() const { return m_snoozeUntil; }
    bool snoozed() const;
    QString snoozeRemainingText() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void snoozeMinutes(int minutes);
    Q_INVOKABLE void clearSnooze();
    Q_INVOKABLE void clearLogs();
    Q_INVOKABLE bool installPam();
    Q_INVOKABLE bool uninstallPam();

    static QString dataDir();
    static QString logPath();
    static QString snoozePath();

signals:
    void eventsChanged();
    void statusChanged();
    void snoozeChanged();

private:
    void loadEvents();
    void loadSnooze();
    void loadStatus();
    void setupWatcher();
    bool runSetup(const QString &action);

    QVariantList m_events;
    QFileSystemWatcher m_watcher;
    QTimer m_tick;
    bool m_hookInstalled;
    bool m_pamConfigured;
    bool m_logReadable;
    qint64 m_snoozeUntil;
    QString m_lastError;
};

#endif
