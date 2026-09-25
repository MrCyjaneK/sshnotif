#include "logstore.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTextStream>

static const char *kHookPath = "/usr/libexec/sshnotif/pam-hook";
static const char *kPamPath = "/etc/pam.d/sshd";

LogStore::LogStore(QObject *parent)
    : QObject(parent)
    , m_hookInstalled(false)
    , m_pamConfigured(false)
    , m_logReadable(false)
    , m_snoozeUntil(0)
{
    QDir().mkpath(dataDir());
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, &LogStore::refresh);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &LogStore::refresh);
    connect(&m_tick, &QTimer::timeout, this, [this]() {
        if (snoozed())
            emit snoozeChanged();
    });
    m_tick.start(15000);
    refresh();
}

QString LogStore::dataDir()
{
    return QDir::homePath() + QStringLiteral("/.local/share/sshnotif");
}

QString LogStore::logPath()
{
    return dataDir() + QStringLiteral("/events.jsonl");
}

QString LogStore::snoozePath()
{
    return dataDir() + QStringLiteral("/snooze-until");
}

QString LogStore::statusSummary() const
{
    if (m_hookInstalled && m_pamConfigured)
        return QStringLiteral("Hook and PAM are installed");
    if (!m_hookInstalled && !m_pamConfigured)
        return QStringLiteral("Helper and PAM line are missing");
    if (!m_hookInstalled)
        return QStringLiteral("PAM line present, helper binary missing");
    return QStringLiteral("Helper installed, PAM line missing");
}

bool LogStore::snoozed() const
{
    return m_snoozeUntil > QDateTime::currentMSecsSinceEpoch() / 1000;
}

QString LogStore::snoozeRemainingText() const
{
    qint64 now = QDateTime::currentMSecsSinceEpoch() / 1000;
    if (m_snoozeUntil <= now)
        return QString();
    qint64 secs = m_snoozeUntil - now;
    qint64 h = secs / 3600;
    qint64 m = (secs % 3600) / 60;
    if (h > 0)
        return QStringLiteral("%1h %2m remaining").arg(h).arg(m);
    if (m > 0)
        return QStringLiteral("%1m remaining").arg(m);
    return QStringLiteral("%1s remaining").arg(secs);
}

void LogStore::refresh()
{
    loadStatus();
    loadSnooze();
    loadEvents();
    setupWatcher();
}

void LogStore::snoozeMinutes(int minutes)
{
    if (minutes < 0)
        minutes = 0;
    if (minutes > 120)
        minutes = 120;
    QDir().mkpath(dataDir());
    qint64 until = QDateTime::currentMSecsSinceEpoch() / 1000 + minutes * 60;
    QFile f(snoozePath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        f.write(QByteArray::number(until));
        f.write("\n");
        f.close();
    }
    loadSnooze();
    setupWatcher();
}

void LogStore::clearSnooze()
{
    QFile::remove(snoozePath());
    m_snoozeUntil = 0;
    emit snoozeChanged();
    setupWatcher();
}

void LogStore::clearLogs()
{
    QFile::remove(logPath());
    if (!m_events.isEmpty()) {
        m_events.clear();
        emit eventsChanged();
    }
    setupWatcher();
}

bool LogStore::runSetup(const QString &action)
{
    QProcess p;
    p.start(QStringLiteral("/usr/libexec/sshnotif/pam-setup"), QStringList() << action);
    if (!p.waitForStarted(2000) || !p.waitForFinished(8000)) {
        m_lastError = QStringLiteral("pam-setup did not finish");
        emit statusChanged();
        return false;
    }
    if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) {
        QString err = QString::fromUtf8(p.readAllStandardError()).trimmed();
        if (err.isEmpty())
            err = QStringLiteral("pam-setup exited %1").arg(p.exitCode());
        m_lastError = err;
        emit statusChanged();
        return false;
    }
    m_lastError.clear();
    refresh();
    return true;
}

bool LogStore::installPam()
{
    return runSetup(QStringLiteral("install"));
}

bool LogStore::uninstallPam()
{
    return runSetup(QStringLiteral("uninstall"));
}

void LogStore::loadStatus()
{
    bool hook = QFileInfo(QString::fromLatin1(kHookPath)).isExecutable();
    bool pam = false;
    QFile pamFile(QString::fromLatin1(kPamPath));
    if (pamFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray data = pamFile.readAll();
        pam = data.contains("/usr/libexec/sshnotif/pam-hook");
    }
    bool readable = QFileInfo(logPath()).isReadable() || !QFileInfo(logPath()).exists();

    bool statusChangedFlag = (hook != m_hookInstalled) || (pam != m_pamConfigured) || (readable != m_logReadable);
    m_hookInstalled = hook;
    m_pamConfigured = pam;
    m_logReadable = readable;
    if (statusChangedFlag)
        emit statusChanged();
}

void LogStore::loadSnooze()
{
    qint64 until = 0;
    QFile f(snoozePath());
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        until = QString::fromUtf8(f.readAll()).trimmed().toLongLong();
    }
    if (until != m_snoozeUntil) {
        m_snoozeUntil = until;
        emit snoozeChanged();
    }
}

void LogStore::loadEvents()
{
    QVariantList events;
    QFile f(logPath());
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const qint64 maxTail = 256 * 1024;
        if (f.size() > maxTail) {
            f.seek(f.size() - maxTail);
            f.readLine();
        }
        QTextStream in(&f);
        in.setCodec("UTF-8");
        while (!in.atEnd()) {
            const QString line = in.readLine().trimmed();
            if (line.isEmpty())
                continue;
            QJsonParseError err;
            QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8(), &err);
            if (err.error != QJsonParseError::NoError || !doc.isObject())
                continue;
            events.append(doc.object().toVariantMap());
        }
    }

    const int keep = 300;
    if (events.size() > keep)
        events = events.mid(events.size() - keep);

    QVariantList newestFirst;
    newestFirst.reserve(events.size());
    for (int i = events.size() - 1; i >= 0; --i)
        newestFirst.append(events.at(i));

    if (newestFirst != m_events) {
        m_events = newestFirst;
        emit eventsChanged();
    }
}

void LogStore::setupWatcher()
{
    const QStringList files = m_watcher.files();
    if (!files.isEmpty())
        m_watcher.removePaths(files);
    const QStringList dirs = m_watcher.directories();
    if (!dirs.isEmpty())
        m_watcher.removePaths(dirs);

    QDir().mkpath(dataDir());
    m_watcher.addPath(dataDir());
    if (QFileInfo::exists(logPath()))
        m_watcher.addPath(logPath());
    if (QFileInfo::exists(snoozePath()))
        m_watcher.addPath(snoozePath());
    if (QFileInfo::exists(QString::fromLatin1(kPamPath)))
        m_watcher.addPath(QString::fromLatin1(kPamPath));
}
