#include "nexora/system/DiagnosticReport.h"

#include "nexora/shared/AtomicFile.h"

#include <QDateTime>
#include <QDir>
#include <QFile>

#include <algorithm>

namespace nexora::system {

namespace {

constexpr qint64 MaxReportBytes = 512 * 1024;
constexpr qsizetype MaxLineChars = 2000;
constexpr int KeepReports = 10;

QString redactLine(const QString &line, const shared::RedactionRules &rules)
{
    QString cleaned = line.left(MaxLineChars);
    cleaned.replace(u'\n', u' ').replace(u'\r', u' ');
    return shared::redact(cleaned, rules);
}

void addSection(QStringList &lines, const QString &title, const QStringList &items, const QString &empty)
{
    lines << QString() << QStringLiteral("== %1 ==").arg(title);
    if (items.isEmpty())
        lines << empty;
    for (const QString &item : items)
        lines << item;
}

QString field(const QString &value)
{
    return value.isEmpty() ? QStringLiteral("unknown") : value;
}

} // namespace

QString buildDiagnosticReport(const DiagnosticInputs &in, const shared::RedactionRules &rules)
{
    QStringList head;
    head << QStringLiteral("Nexora OS diagnostic report") << QStringLiteral("Version: %1").arg(field(in.appVersion))
         << QStringLiteral("Build: %1").arg(field(in.buildId)) << QStringLiteral("Platform: %1").arg(field(in.platform))
         << QStringLiteral("Generated: %1").arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

    const HardwareSnapshot &hw = in.hardware;
    QStringList hardware;
    hardware << QStringLiteral("CPU: %1 (%2 threads)").arg(field(hw.cpuModel)).arg(hw.cpuThreads);
    hardware << QStringLiteral("Memory: %1 KiB total, %2 KiB available").arg(hw.memTotalKiB).arg(hw.memAvailableKiB);
    for (const GpuInfo &g : hw.gpus) {
        hardware << QStringLiteral("GPU %1: %2 (%3) device %4 driver %5")
                        .arg(g.card, field(g.vendorName), field(g.vendorId), field(g.deviceId), field(g.driver));
    }
    for (const DisplayOutput &o : hw.outputs) {
        hardware << QStringLiteral("Output %1: %2, modes: %3")
                        .arg(o.name, o.connected ? QStringLiteral("connected") : QStringLiteral("disconnected"),
                             o.modes.isEmpty() ? QStringLiteral("none") : o.modes.join(QStringLiteral(", ")));
    }
    for (const AudioCard &a : hw.audio)
        hardware << QStringLiteral("Audio card %1: %2 (%3)").arg(a.index).arg(a.name, a.id);
    for (const StorageInfo &s : hw.storage) {
        hardware << QStringLiteral("Storage %1: %2 %3, %4 of %5 bytes available")
                        .arg(s.path, field(s.device), field(s.fsType))
                        .arg(s.availableBytes)
                        .arg(s.totalBytes);
    }

    QStringList body = head;
    addSection(body, QStringLiteral("Hardware"), hardware, QString());
    addSection(body, QStringLiteral("Configuration issues"), in.configIssues, QStringLiteral("none"));
    addSection(body, QStringLiteral("Settings issues"), in.settingsIssues, QStringLiteral("none"));
    addSection(body, QStringLiteral("Library issues"), in.libraryIssues, QStringLiteral("none"));
    addSection(body, QStringLiteral("Recent launches"), in.launchSummaries, QStringLiteral("none"));

    QString text;
    qint64 used = 0;
    for (const QString &line : body) {
        const QString cleaned = redactLine(line, rules);
        text += cleaned + u'\n';
        used += cleaned.toUtf8().size() + 1;
    }

    // Log lines last: newest first until the budget is spent, then restore the order.
    const QString logHeader = QStringLiteral("\n== Recent log ==\n");
    used += logHeader.toUtf8().size();
    const QString truncated = QStringLiteral("[older log lines omitted]\n");
    QStringList kept;
    bool omitted = false;
    for (qsizetype i = in.recentLog.size() - 1; i >= 0; --i) {
        const QString cleaned = redactLine(in.recentLog.at(i), rules);
        const qint64 cost = cleaned.toUtf8().size() + 1;
        if (used + cost + truncated.toUtf8().size() > MaxReportBytes) {
            omitted = true;
            break;
        }
        used += cost;
        kept.append(cleaned);
    }
    std::reverse(kept.begin(), kept.end());
    text += logHeader;
    if (omitted)
        text += truncated;
    for (const QString &line : kept)
        text += line + u'\n';
    // Pathologically large non-log sections: hard cap (cutting mid-character is harmless here).
    if (text.toUtf8().size() > MaxReportBytes)
        text = QString::fromUtf8(text.toUtf8().left(MaxReportBytes));
    return text;
}

std::optional<QString> exportDiagnosticReport(const QString &text, const QString &diagnosticsDir, QString *error)
{
    if (!QDir().mkpath(diagnosticsDir)) {
        if (error)
            *error = QStringLiteral("cannot create %1").arg(diagnosticsDir);
        return std::nullopt;
    }
    const QString stem
        = QStringLiteral("diagnostics-") + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"));
    QString path = diagnosticsDir + u'/' + stem + QStringLiteral(".txt");
    for (int n = 1; QFile::exists(path); ++n)
        path = diagnosticsDir + u'/' + stem + u'-' + QString::number(n) + QStringLiteral(".txt");
    if (!shared::writeFileAtomically(path, text.toUtf8(), error))
        return std::nullopt;

    // Newest first by base name (timestamp, then collision counter), keep KeepReports.
    QDir dir(diagnosticsDir);
    QStringList reports = dir.entryList({QStringLiteral("diagnostics-*.txt")}, QDir::Files);
    std::sort(reports.begin(), reports.end(),
              [](const QString &a, const QString &b) { return a.chopped(4) > b.chopped(4); });
    for (int i = KeepReports; i < reports.size(); ++i)
        QFile::remove(dir.filePath(reports.at(i)));
    return path;
}

} // namespace nexora::system
